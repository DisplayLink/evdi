// Copyright (c) 2015 DisplayLink (UK) Ltd.

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "evdi_debug.h"

unsigned g_evdi_loglevel = EVDI_LOGLEVEL_DEBUG;

module_param_named(initial_loglevel, g_evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");
