/*
 * Copyright (c) 2015 - 2018 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "evdi_params.h"
#include "evdi_debug.h"

unsigned int evdi_loglevel __read_mostly = EVDI_LOGLEVEL_DEBUG;
bool evdi_enable_cursor_blending __read_mostly = true;

module_param_named(initial_loglevel, evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");

module_param_named(enable_cursor_blending,
		   evdi_enable_cursor_blending, bool, 0644);
MODULE_PARM_DESC(enable_cursor_blending, "Enables cursor compositing on user supplied framebuffer via EVDI_GRABPIX ioctl (default: true)");

