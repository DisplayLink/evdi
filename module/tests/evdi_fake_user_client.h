/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2024 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_FAKE_USER_CLIENT_H
#define EVDI_FAKE_USER_CLIENT_H

#ifdef CONFIG_DRM_EVDI_KUNIT_TEST


#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <kunit/device.h>

#include <linux/file.h>
#include <linux/compiler_types.h>
#include <drm/drm_device.h>

/* kunit tests helpers faking evdi userspace client, usually displaylink-driver daemon. */

struct evdi_fake_user_data {
	struct file *file;
};
void __user *evdi_kunit_alloc_usermem(struct kunit *test, unsigned int size);

void evdi_fake_user_client_create(struct kunit *test);
void evdi_fake_user_client_connect(struct kunit *test, struct drm_device *device);
void evdi_fake_user_client_disconnect(struct kunit *test, struct drm_device *device);

#endif // CONFIG_DRM_EVDI_KUNIT_TEST
#endif // EVDI_FAKE_USER_CLIENT_H

