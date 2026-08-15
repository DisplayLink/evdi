// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/minmax.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fixed.h>

#include "evdi_color.h"
#include "evdi_debug.h"
#include "evdi_drm_drv.h"
#include "evdi_params.h"

static LIST_HEAD(evdi_color_list);
static DEFINE_MUTEX(evdi_color_list_lock);

static s64 evdi_sm2fixp(u64 sm)
{
	s64 magnitude = (s64)(sm & ~BIT_ULL(63));

	return (sm & BIT_ULL(63)) ? -magnitude : magnitude;
}

static bool evdi_gamma_is_identity(const u8 g[3][EVDI_GAMMA_LUT_SIZE])
{
	int c, i;

	for (c = 0; c < 3; ++c)
		for (i = 0; i < EVDI_GAMMA_LUT_SIZE; ++i)
			if (g[c][i] != (u8)i)
				return false;
	return true;
}

static bool evdi_ctm_is_identity(const s64 m[3][3])
{
	int r, c;

	for (r = 0; r < 3; ++r) {
		for (c = 0; c < 3; ++c) {
			s64 expect = (r == c) ? (s64)DRM_FIXED_ONE : 0;

			if (m[r][c] != expect)
				return false;
		}
	}
	return true;
}

static bool evdi_ctm_is_diagonal(const s64 m[3][3])
{
	int r, c;

	for (r = 0; r < 3; ++r)
		for (c = 0; c < 3; ++c)
			if (r != c && m[r][c] != 0)
				return false;
	return true;
}

/*
 * Same diagonal matrix as a 256-entry LUT — speed only, not a new model.
 * DRM order is CTM then gamma: when a real LUT is already loaded, compose
 * gamma_out[i] = gamma_in[clamp(scale * i)] instead of overwriting it.
 */
static void evdi_fuse_diagonal_ctm_to_gamma(struct evdi_color_data *data)
{
	u8 src_gamma[3][EVDI_GAMMA_LUT_SIZE];
	const bool compose = data->has_gamma;
	int c, i;

	if (compose)
		memcpy(src_gamma, data->gamma, sizeof(src_gamma));

	for (c = 0; c < 3; ++c) {
		s64 scale = data->ctm[c][c];

		for (i = 0; i < EVDI_GAMMA_LUT_SIZE; ++i) {
			s64 v = drm_fixp_mul(scale, drm_int2fixp(i));
			int mid = clamp(drm_fixp2int_round(v), 0, 255);

			data->gamma[c][i] = compose ? src_gamma[c][mid]
						    : (u8)mid;
		}
	}
	data->has_gamma = true;
	data->has_ctm = false;
	data->fused_diagonal = true;
}

static void evdi_color_load_gamma(struct evdi_color_data *data,
				  struct drm_crtc_state *crtc_state)
{
	const struct drm_color_lut *lut;
	unsigned int lut_len;
	int i, c;

	if (!crtc_state->gamma_lut) {
		data->has_gamma = false;
		return;
	}

	lut = (const struct drm_color_lut *)crtc_state->gamma_lut->data;
	lut_len = crtc_state->gamma_lut->length / sizeof(*lut);
	if (lut_len == 0) {
		data->has_gamma = false;
		return;
	}

	for (i = 0; i < EVDI_GAMMA_LUT_SIZE; ++i) {
		s64 idx_fp = drm_fixp_div(drm_int2fixp(i * (lut_len - 1)),
					   drm_int2fixp(EVDI_GAMMA_LUT_SIZE - 1));
		int idx_floor = drm_fixp2int(idx_fp);
		int idx_ceil = min_t(int, idx_floor + 1, lut_len - 1);
		s64 frac = idx_fp - drm_int2fixp(idx_floor);
		u16 raw[3] = { lut[idx_floor].red, lut[idx_floor].green, lut[idx_floor].blue };
		u16 raw_ceil[3] = { lut[idx_ceil].red, lut[idx_ceil].green, lut[idx_ceil].blue };

		for (c = 0; c < 3; ++c) {
			s64 floor_fp = drm_int2fixp(raw[c]);
			s64 ceil_fp = drm_int2fixp(raw_ceil[c]);
			s64 interp = floor_fp + drm_fixp_mul(ceil_fp - floor_fp, frac);
			int val16 = clamp(drm_fixp2int(interp), 0, 65535);

			data->gamma[c][i] = val16 >> 8;
		}
	}
	data->has_gamma = true;
}

static void evdi_color_load_ctm(struct evdi_color_data *data,
				struct drm_crtc_state *crtc_state)
{
	const struct drm_color_ctm *ctm;
	int r, c;

	if (!crtc_state->ctm) {
		data->has_ctm = false;
		return;
	}

	ctm = (const struct drm_color_ctm *)crtc_state->ctm->data;
	for (r = 0; r < 3; ++r)
		for (c = 0; c < 3; ++c)
			data->ctm[r][c] = evdi_sm2fixp(ctm->matrix[r * 3 + c]);

	data->has_ctm = true;
}

static void evdi_color_finalize_locked(struct evdi_color_data *data)
{
	data->fused_diagonal = false;

	/* Apply DRM order without degamma: CTM then gamma. */
	if (data->has_ctm && evdi_ctm_is_identity(data->ctm))
		data->has_ctm = false;
	else if (data->has_ctm && evdi_ctm_is_diagonal(data->ctm))
		evdi_fuse_diagonal_ctm_to_gamma(data);

	if (data->has_gamma && evdi_gamma_is_identity(data->gamma))
		data->has_gamma = false;

	data->active = data->has_gamma || data->has_ctm;
}

void evdi_color_transform_init(struct evdi_color_transform *color,
			       struct drm_device *ddev)
{
	memset(color, 0, sizeof(*color));
	mutex_init(&color->lock);
	color->ddev = ddev;
	INIT_LIST_HEAD(&color->link);

	mutex_lock(&evdi_color_list_lock);
	list_add_tail(&color->link, &evdi_color_list);
	mutex_unlock(&evdi_color_list_lock);
}

void evdi_color_transform_cleanup(struct evdi_color_transform *color)
{
	mutex_lock(&evdi_color_list_lock);
	list_del_init(&color->link);
	mutex_unlock(&evdi_color_list_lock);
	mutex_destroy(&color->lock);
}

bool evdi_color_transform_update(struct evdi_color_transform *color,
				 struct drm_crtc_state *crtc_state)
{
	struct evdi_color_data before;
	bool changed;

	if (!crtc_state->color_mgmt_changed)
		return false;

	mutex_lock(&color->lock);
	before = color->data;
	memset(&color->data, 0, sizeof(color->data));
	evdi_color_load_gamma(&color->data, crtc_state);
	evdi_color_load_ctm(&color->data, crtc_state);
	evdi_color_finalize_locked(&color->data);
	changed = memcmp(&before, &color->data, sizeof(before)) != 0;
	mutex_unlock(&color->lock);
	return changed;
}

bool evdi_color_transform_snapshot(struct evdi_color_transform *color,
				   struct evdi_color_data *snapshot)
{
	mutex_lock(&color->lock);
	*snapshot = color->data;
	mutex_unlock(&color->lock);
	return snapshot->active;
}

static s64 evdi_color_apply_ctm_channel(const struct evdi_color_data *snapshot,
					int row, s64 r, s64 g, s64 b)
{
	return drm_fixp_mul(snapshot->ctm[row][0], r) +
	       drm_fixp_mul(snapshot->ctm[row][1], g) +
	       drm_fixp_mul(snapshot->ctm[row][2], b);
}

int evdi_color_format_status(char *buf, size_t size)
{
	struct evdi_color_transform *color;
	int n = 0;

	n += scnprintf(buf + n, size - n,
		       "color_props=%s (reload module to change)\n"
		       "# apply only what the compositor programmed; no gamma↔ctm synthesis\n"
		       "# path=lut|ctm|fused_ctm_lut|off  drm=N is /dev/dri/cardN\n",
		       evdi_color_props ? evdi_color_props : "both");

	mutex_lock(&evdi_color_list_lock);
	list_for_each_entry(color, &evdi_color_list, link) {
		struct evdi_color_data d;
		int r_s, g_s, b_s;
		int drm_idx = -1;
		const char *path;

		mutex_lock(&color->lock);
		d = color->data;
		if (color->ddev && color->ddev->primary)
			drm_idx = color->ddev->primary->index;
		mutex_unlock(&color->lock);

		if (d.has_gamma) {
			r_s = d.gamma[0][255];
			g_s = d.gamma[1][255];
			b_s = d.gamma[2][255];
		} else if (d.has_ctm) {
			r_s = clamp(drm_fixp2int_round(
				drm_fixp_mul(d.ctm[0][0], drm_int2fixp(255))), 0, 255);
			g_s = clamp(drm_fixp2int_round(
				drm_fixp_mul(d.ctm[1][1], drm_int2fixp(255))), 0, 255);
			b_s = clamp(drm_fixp2int_round(
				drm_fixp_mul(d.ctm[2][2], drm_int2fixp(255))), 0, 255);
		} else {
			r_s = g_s = b_s = 255;
		}

		if (!d.active)
			path = "off";
		else if (d.fused_diagonal)
			path = "fused_ctm_lut";
		else if (d.has_ctm)
			path = "ctm";
		else if (d.has_gamma)
			path = "lut";
		else
			path = "off";

		n += scnprintf(buf + n, size > n ? size - n : 0,
			       "drm=%d path=%s active=%d has_gamma=%d has_ctm=%d scales_rgb≈%d/%d/%d\n",
			       drm_idx, path, d.active, d.has_gamma, d.has_ctm,
			       r_s, g_s, b_s);
	}
	mutex_unlock(&evdi_color_list_lock);
	return n;
}

void evdi_color_transform_apply_row(const struct evdi_color_data *snapshot,
				    void *row, int width_px, bool swap_rb)
{
	u8 *px = row;
	const int r_idx = swap_rb ? 0 : 2;
	const int b_idx = swap_rb ? 2 : 0;
	int i;

	if (!snapshot->active)
		return;

	for (i = 0; i < width_px; ++i, px += 4) {
		int val[3] = { px[r_idx], px[1], px[b_idx] };

		if (snapshot->has_ctm) {
			s64 r = drm_int2fixp(val[0]);
			s64 g = drm_int2fixp(val[1]);
			s64 b = drm_int2fixp(val[2]);

			val[0] = clamp(drm_fixp2int_round(
				evdi_color_apply_ctm_channel(snapshot, 0, r, g, b)), 0, 255);
			val[1] = clamp(drm_fixp2int_round(
				evdi_color_apply_ctm_channel(snapshot, 1, r, g, b)), 0, 255);
			val[2] = clamp(drm_fixp2int_round(
				evdi_color_apply_ctm_channel(snapshot, 2, r, g, b)), 0, 255);
		}

		if (snapshot->has_gamma) {
			val[0] = snapshot->gamma[0][val[0]];
			val[1] = snapshot->gamma[1][val[1]];
			val[2] = snapshot->gamma[2][val[2]];
		}

		px[r_idx] = val[0];
		px[1] = val[1];
		px[b_idx] = val[2];
	}
}
