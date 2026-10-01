// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

#include "evdi_params.h"
#include "evdi_debug.h"
#include "evdi_color.h"

unsigned int evdi_loglevel __read_mostly = EVDI_LOGLEVEL_INFO;
unsigned short int evdi_initial_device_count __read_mostly;
char *evdi_color_props __read_mostly = "both";

module_param_named(initial_loglevel, evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");

module_param_named(initial_device_count,
		   evdi_initial_device_count, ushort, 0644);
MODULE_PARM_DESC(initial_device_count, "Initial DRM device count (default: 0)");

module_param_named(color_props, evdi_color_props, charp, 0444);
MODULE_PARM_DESC(color_props,
		 "Load-time DRM colour props: both, gamma, or ctm (default both). "
		 "Requires module reload. Mutter uses GAMMA_LUT if present, else CTM.");

static int color_status_get(char *buffer, const struct kernel_param *kp)
{
	return evdi_color_format_status(buffer, PAGE_SIZE);
}

static const struct kernel_param_ops color_status_ops = {
	.get = color_status_get,
};

module_param_cb(color_status, &color_status_ops, NULL, 0444);
MODULE_PARM_DESC(color_status,
		 "Read-only: advertised mode and per-card effective transform");

bool evdi_color_props_has_gamma(void)
{
	if (!evdi_color_props)
		return true;
	if (!strcmp(evdi_color_props, "ctm"))
		return false;
	return true;
}

bool evdi_color_props_has_ctm(void)
{
	if (!evdi_color_props)
		return true;
	if (!strcmp(evdi_color_props, "gamma"))
		return false;
	return true;
}
