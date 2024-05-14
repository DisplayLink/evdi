// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */


#include <kunit/test.h>
#include <kunit/device.h>
#include "evdi_drm_drv.h"

static void test_evdi_create_drm_device(struct kunit *test)
{
	struct device *parent = kunit_device_register(test, "/dev/card1");
	struct drm_device *dev = evdi_drm_device_create(parent);

	KUNIT_EXPECT_NOT_NULL(test, dev);

	evdi_drm_device_remove(dev);

	kunit_device_unregister(test, parent);
}

static struct kunit_case evdi_test_cases[] = {
	KUNIT_CASE(test_evdi_create_drm_device),
	{}
};

static struct kunit_suite evdi_test_suite = {
	.name = "drm_evdi_tests",
	.test_cases = evdi_test_cases,
};

kunit_test_suite(evdi_test_suite);

MODULE_LICENSE("GPL");
