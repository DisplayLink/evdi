// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

#include "evdi_params.h"
#include "evdi_debug.h"

unsigned int evdi_loglevel __read_mostly = EVDI_LOGLEVEL_INFO;
unsigned short int evdi_initial_device_count __read_mostly;
char *evdi_color_props __read_mostly = "both";

module_param_named(initial_loglevel, evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");

module_param_named(initial_device_count,
		   evdi_initial_device_count, ushort, 0644);
MODULE_PARM_DESC(initial_device_count, "Initial DRM device count (default: 0)");

module_param_named(color_props, evdi_color_props, charp, 0644);
MODULE_PARM_DESC(color_props,
		 "Colour properties to advertise: both, gamma, or ctm (default: both)");

bool evdi_color_props_has_gamma(void)
{
	if (!evdi_color_props)
		return true;
	if (!strcmp(evdi_color_props, "ctm"))
		return false;
	/* both, gamma, or unknown → offer gamma */
	return true;
}

bool evdi_color_props_has_ctm(void)
{
	if (!evdi_color_props)
		return true;
	if (!strcmp(evdi_color_props, "gamma"))
		return false;
	/* both, ctm, or unknown → offer CTM */
	return true;
}

