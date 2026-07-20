/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_COLOR_H
#define EVDI_COLOR_H

#include <linux/mutex.h>
#include <linux/types.h>

struct drm_crtc_state;

/*
 * evdi has no hardware CRTC gamma/CTM block, so Night Light and other
 * DRM color management clients need the transform applied in software
 * to the raw framebuffer bytes before they leave the driver.
 *
 * Advertised via drm_mode_crtc_set_gamma_size()/drm_crtc_enable_color_mgmt()
 * and consumed here as a direct 8-bit-indexed lookup table, independent of
 * whatever length LUT userspace actually uploads (see evdi_color_transform_update()).
 */
#define EVDI_GAMMA_LUT_SIZE 256

struct evdi_color_data {
	bool active;
	bool has_gamma;
	bool has_ctm;
	u8 gamma[3][EVDI_GAMMA_LUT_SIZE];
	/* row-major 3x3, drm_fixed.h S32.32 two's-complement fixed point */
	s64 ctm[3][3];
};

struct evdi_color_transform {
	struct mutex lock;
	struct evdi_color_data data;
};

void evdi_color_transform_init(struct evdi_color_transform *color);

/* Recomputes the LUT/CTM from crtc_state; cheap no-op unless
 * crtc_state->color_mgmt_changed is set (i.e. on actual property writes).
 */
void evdi_color_transform_update(struct evdi_color_transform *color,
				  struct drm_crtc_state *crtc_state);

/* Copies the current transform out under lock. Returns whether it's a
 * no-op identity transform, letting the caller skip apply_row() entirely.
 */
bool evdi_color_transform_snapshot(struct evdi_color_transform *color,
				    struct evdi_color_data *snapshot);

/* Applies gamma/CTM in place to one packed 32bpp scanline of width_px
 * pixels. swap_rb selects XBGR/ABGR (R and B swapped vs XRGB/ARGB) byte
 * order; the alpha/padding byte is always left untouched.
 */
void evdi_color_transform_apply_row(const struct evdi_color_data *snapshot,
				     void *row, int width_px, bool swap_rb);

#endif
