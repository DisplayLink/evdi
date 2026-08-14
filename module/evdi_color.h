/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_COLOR_H
#define EVDI_COLOR_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct drm_crtc_state;

/*
 * Software colour management: apply whatever the compositor programs on the
 * CRTC (GAMMA_LUT and/or CTM). No synthesis between the two.
 *
 * Which properties exist is chosen at module load by color_props=
 * both|gamma|ctm (see evdi_params). Reload the module to change that.
 *
 * Diagonal CTMs are fused into a 256-entry LUT only as a speed optimisation
 * of the same matrix (Night Light style scales); identity is skipped.
 */
#define EVDI_GAMMA_LUT_SIZE 256

struct evdi_color_data {
	bool active;
	bool has_gamma;
	bool has_ctm;
	bool fused_diagonal;
	u8 gamma[3][EVDI_GAMMA_LUT_SIZE];
	/* row-major 3x3, drm_fixed.h S32.32 two's-complement fixed point */
	s64 ctm[3][3];
};

struct evdi_color_transform {
	struct mutex lock;
	struct list_head link;
	struct evdi_color_data data;
	struct drm_device *ddev;
	void *scratch;
	size_t scratch_bytes;
};

void evdi_color_transform_init(struct evdi_color_transform *color,
			       struct drm_device *ddev);
void evdi_color_transform_cleanup(struct evdi_color_transform *color);

/* Returns true if the effective apply payload changed (caller may full-dirty). */
bool evdi_color_transform_update(struct evdi_color_transform *color,
				 struct drm_crtc_state *crtc_state);

bool evdi_color_transform_snapshot(struct evdi_color_transform *color,
				   struct evdi_color_data *snapshot);

void *evdi_color_get_scratch(struct evdi_color_transform *color, size_t bytes);

void evdi_color_transform_apply_row(const struct evdi_color_data *snapshot,
				    void *row, int width_px, bool swap_rb);

int evdi_color_format_status(char *buf, size_t size);

#endif
