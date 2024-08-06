/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2024 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */


#ifndef EVDI_TEST_H
#define EVDI_TEST_H

#ifdef CONFIG_DRM_EVDI_KUNIT_TEST
#define EVDI_TEST_HOOK(foo) foo
#else
#include <linux/printk.h>
#define EVDI_TEST_HOOK(foo) no_printk(__stringify(foo))
#endif

#ifdef CONFIG_DRM_EVDI_KUNIT_TEST

#include <linux/completion.h>

/* evdi hooks for kunit tests */
void evdi_testhook_drm_device_destroyed(void);

struct evdi_test_hooks {
	void (*drm_device_destroyed)(void);
};

/* evdi kunit base type for test private data */
struct evdi_test_data {
	struct device *parent;
	struct drm_device *dev;
	struct completion *dev_destroyed;
	struct evdi_test_hooks hooks;
};

void evdi_test_data_init(struct kunit *test, struct evdi_test_data *data);
void evdi_test_data_exit(struct kunit *test, struct evdi_test_data *data);

#endif // CONFIG_DRM_EVDI_KUNIT_TEST
#endif // EVDI_TEST_H

