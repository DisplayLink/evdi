// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/slab.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fixed.h>

#include "evdi_color.h"

/*
 * struct drm_color_ctm stores each entry as S31.32 sign-magnitude (bit 63
 * is the sign, bits 0-62 are the unsigned value) rather than the two's
 * complement drm_fixed.h (drm_fixp_t) format the drm_fixp_* helpers use.
 * Some newer kernels provide drm_sm2fixp() for this, but it isn't present
 * across the full 4.15+ range evdi supports, so convert it ourselves.
 */
static s64 evdi_sm2fixp(u64 sm)
{
	s64 magnitude = (s64)(sm & ~BIT_ULL(63));

	return (sm & BIT_ULL(63)) ? -magnitude : magnitude;
}

static void evdi_color_update_gamma(struct evdi_color_data *data,
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

	/*
	 * Build a direct 256-entry table indexed by raw 8-bit channel value,
	 * regardless of the length of the LUT userspace actually uploaded
	 * (expected to be EVDI_GAMMA_LUT_SIZE, since that's what we advertise
	 * via drm_mode_crtc_set_gamma_size(), but not all clients necessarily
	 * respect that hint).
	 */
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

			/* 16-bit LUT entry -> 8-bit raw pixel channel */
			data->gamma[c][i] = val16 >> 8;
		}
	}

	data->has_gamma = true;
}

static void evdi_color_update_ctm(struct evdi_color_data *data,
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

void evdi_color_transform_init(struct evdi_color_transform *color)
{
	mutex_init(&color->lock);
	memset(&color->data, 0, sizeof(color->data));
}

void evdi_color_transform_update(struct evdi_color_transform *color,
				  struct drm_crtc_state *crtc_state)
{
	if (!crtc_state->color_mgmt_changed)
		return;

	mutex_lock(&color->lock);
	evdi_color_update_gamma(&color->data, crtc_state);
	evdi_color_update_ctm(&color->data, crtc_state);
	color->data.active = color->data.has_gamma || color->data.has_ctm;
	mutex_unlock(&color->lock);
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

void evdi_color_transform_apply_row(const struct evdi_color_data *snapshot,
				     void *row, int width_px, bool swap_rb)
{
	u8 *px = row;
	const int r_idx = swap_rb ? 2 : 0;
	const int b_idx = swap_rb ? 0 : 2;
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
