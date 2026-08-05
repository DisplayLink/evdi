/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_PARAMS_H
#define EVDI_PARAMS_H

extern unsigned int evdi_loglevel;
extern unsigned short int evdi_initial_device_count;

/*
 * Which DRM colour properties to advertise (compositors pick Night Light path
 * from what is present). Set at module load, e.g.:
 *   modprobe evdi color_props=ctm
 *   EVDI_COLOR_PROPS=gamma sudo ./scripts/install.sh
 *
 *   both  — GAMMA_LUT + CTM (default; GNOME prefers GAMMA_LUT)
 *   gamma — GAMMA_LUT only
 *   ctm   — CTM only (useful to force Mutter CTM path)
 */
extern char *evdi_color_props;

bool evdi_color_props_has_gamma(void);
bool evdi_color_props_has_ctm(void);

#endif /* EVDI_PARAMS_H */
