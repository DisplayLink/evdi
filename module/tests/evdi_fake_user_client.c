// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */


#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <kunit/device.h>

#include <linux/file.h>
#include <linux/vt_kern.h>

#include <drm/drm_file.h>

#include "evdi_drm_drv.h"
#include "evdi_drm.h"
#include "tests/evdi_test.h"
#include "tests/evdi_fake_user_client.h"

/* copied from drm/tests/drm_kunit_edid.h */
static const unsigned char test_edid_dvi_1080p[] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x31, 0xd8, 0x2a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x01, 0x03, 0x81, 0xa0, 0x5a, 0x78,
	0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38,
	0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfc, 0x00, 0x54, 0x65, 0x73, 0x74, 0x20, 0x45, 0x44,
	0x49, 0x44, 0x0a, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x32,
	0x46, 0x1e, 0x46, 0x0f, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab
};

void evdi_fake_user_client_create(struct kunit *test)
{
	struct kunit_resource *resource;
	struct evdi_fake_user_data *user_data;

	resource = kunit_find_named_resource(test, "fake_evdi_user");
	if (resource) {
		KUNIT_FAIL(test, "fake_evdi_user data already exists");
		return;
	}

	resource = kunit_kzalloc(test, sizeof(struct kunit_resource), GFP_KERNEL);
	user_data = kunit_kzalloc(test, sizeof(struct evdi_fake_user_data), GFP_KERNEL);
	kunit_add_named_resource(test, NULL, NULL, resource, "fake_evdi_user", user_data);
}

void evdi_fake_user_client_connect(struct kunit *test, struct drm_device *device)
{
	struct kunit_resource *resource = kunit_find_named_resource(test, "fake_evdi_user");
	struct evdi_fake_user_data *user_data = resource->data;

	void __user *user_edid = evdi_kunit_alloc_usermem(test, sizeof(test_edid_dvi_1080p));
	struct drm_evdi_connect connect_data = {
		.connected = true,
		.dev_index = device->primary->index,
		.edid = user_edid,
		.edid_length = sizeof(test_edid_dvi_1080p),
		.pixel_area_limit = 1920 * 1080,
		.pixel_per_second_limit = 1920 * 1080 * 60,
	};

	if (copy_to_user((unsigned char * __user)connect_data.edid, test_edid_dvi_1080p, sizeof(test_edid_dvi_1080p)))
		KUNIT_FAIL(test, "Failed to copy edid to userspace memory");
	user_data->file = mock_drm_getfile(device->primary, O_RDWR);

	evdi_painter_connect_ioctl(device, &connect_data, user_data->file->private_data);
}

void evdi_fake_user_client_disconnect(struct kunit *test, struct drm_device *device)
{
	struct kunit_resource *resource = kunit_find_named_resource(test, "fake_evdi_user");
	struct evdi_fake_user_data *user_data = resource->data;

	if (user_data->file) {
		fput(user_data->file);
		user_data->file = NULL;
		flush_delayed_fput();
	}
}

