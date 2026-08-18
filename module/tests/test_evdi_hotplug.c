// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */


#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <kunit/device.h>
#include "evdi_drm_drv.h"
#include "tests/evdi_test.h"
#include "tests/evdi_fake_user_client.h"
#include "tests/evdi_fake_compositor.h"


struct evdi_hotplug_test {
	struct evdi_test_data base;
	int dpms_mode;
	int mode_changed_count;
	int hdisplay;
	int vdisplay;
};
#define to_evdi_hotplug_test(x) container_of(x, struct evdi_hotplug_test, base)

static void testhook_painter_send_dpms(int mode)
{
	struct kunit *test = kunit_get_current_test();
	struct evdi_hotplug_test *data = to_evdi_hotplug_test(test->priv);

	data->dpms_mode = mode;
}

static void testhook_painter_send_mode_changed(const struct drm_display_mode *mode)
{
	struct kunit *test = kunit_get_current_test();
	struct evdi_hotplug_test *data = to_evdi_hotplug_test(test->priv);

	data->mode_changed_count++;
	data->hdisplay = mode->hdisplay;
	data->vdisplay = mode->vdisplay;
}

static int suite_test_hotplug_init(struct kunit *test)
{
	struct evdi_hotplug_test *data =
		kunit_kzalloc(test, sizeof(struct evdi_hotplug_test), GFP_KERNEL);

	evdi_test_data_init(test, &data->base);
	data->base.hooks.painter_send_dpms = testhook_painter_send_dpms;
	data->base.hooks.painter_send_mode_changed = testhook_painter_send_mode_changed;
	data->base.dev = evdi_drm_device_create(data->base.parent);

	evdi_fake_compositor_create(test);
	evdi_fake_user_client_create(test);

	evdi_fake_user_client_connect(test, data->base.dev);
	evdi_fake_compositor_connect(test, data->base.dev);

	return 0;
}

static void suite_test_hotplug_exit(struct kunit *test)
{
	struct evdi_hotplug_test *data = to_evdi_hotplug_test(test->priv);

	evdi_fake_compositor_disconnect(test, data->base.dev);
	evdi_fake_user_client_disconnect(test, data->base.dev);

	if (data->base.dev) {
		evdi_drm_device_remove(data->base.dev);
		data->base.dev = NULL;
	}

	evdi_test_data_exit(test, &data->base);
	kunit_kfree(test, test->priv);
}

static void test_evdi_painter_replays_mode_on_reconnect_without_modeset(struct kunit *test)
{
	struct evdi_hotplug_test *data = to_evdi_hotplug_test(test->priv);
	struct evdi_device *evdi = (struct evdi_device *)data->base.dev->dev_private;
	struct drm_clip_rect rect;

	data->mode_changed_count = 0;
	data->dpms_mode = DRM_MODE_DPMS_OFF;

	/* Replug too quick for the compositor to tear the pipeline down. */
	evdi_fake_user_client_disconnect(test, data->base.dev);
	evdi_fake_user_client_connect(test, data->base.dev);

	KUNIT_EXPECT_EQ(test, data->mode_changed_count, 1);
	KUNIT_EXPECT_EQ(test, data->hdisplay, 640);
	KUNIT_EXPECT_EQ(test, data->vdisplay, 480);
	KUNIT_EXPECT_EQ(test, data->dpms_mode, DRM_MODE_DPMS_ON);
	KUNIT_EXPECT_TRUE(test, evdi_painter_needs_full_modeset(evdi->painter));

	rect = evdi_painter_framebuffer_size(evdi->painter);
	KUNIT_EXPECT_EQ(test, rect.x2, 640);
	KUNIT_EXPECT_EQ(test, rect.y2, 480);
}

static void test_evdi_painter_does_not_replay_mode_when_pipeline_was_torn_down(struct kunit *test)
{
	struct evdi_hotplug_test *data = to_evdi_hotplug_test(test->priv);
	struct evdi_device *evdi = (struct evdi_device *)data->base.dev->dev_private;

	data->mode_changed_count = 0;
	data->dpms_mode = DRM_MODE_DPMS_OFF;

	evdi_fake_compositor_disconnect(test, data->base.dev);
	evdi_fake_user_client_disconnect(test, data->base.dev);
	evdi_fake_user_client_connect(test, data->base.dev);

	KUNIT_EXPECT_EQ(test, data->mode_changed_count, 0);
	KUNIT_EXPECT_NE(test, data->dpms_mode, DRM_MODE_DPMS_ON);
	KUNIT_EXPECT_TRUE(test, evdi_painter_needs_full_modeset(evdi->painter));
}

static struct kunit_case evdi_test_cases[] = {
	KUNIT_CASE(test_evdi_painter_replays_mode_on_reconnect_without_modeset),
	KUNIT_CASE(test_evdi_painter_does_not_replay_mode_when_pipeline_was_torn_down),
	{}
};

static struct kunit_suite evdi_test_suite = {
	.name = "drm_evdi_hotplug_tests",
	.test_cases = evdi_test_cases,
	.init = suite_test_hotplug_init,
	.exit = suite_test_hotplug_exit,
};

kunit_test_suite(evdi_test_suite);
