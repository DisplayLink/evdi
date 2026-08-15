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
 * color_props — load-time only (read at CRTC init). Reload the module to change.
 *
 *   both  (default) — advertise GAMMA_LUT + CTM
 *   gamma           — GAMMA_LUT only
 *   ctm             — CTM only
 *
 *   modprobe evdi color_props=ctm
 *   EVDI_COLOR_PROPS=ctm sudo ./scripts/install.sh
 *
 * Mutter Night Light: uses GAMMA_LUT if present, else CTM. So color_props=ctm
 * makes GNOME send a real CTM (with a Mutter that supports CTM Night Light).
 */
extern char *evdi_color_props;

bool evdi_color_props_has_gamma(void);
bool evdi_color_props_has_ctm(void);

#endif /* EVDI_PARAMS_H */
