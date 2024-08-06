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
#include <linux/vt_kern.h>
#include "evdi_drm_drv.h"
#include "tests/evdi_test.h"
#include "tests/evdi_fake_user_client.h"
#include "tests/evdi_fake_compositor.h"


struct evdi_vt_test {
	struct evdi_test_data base;
	struct notifier_block *vt_notifier;
	int dpms_mode;
};
#define to_evdi_vt_test(x) container_of(x, struct evdi_vt_test, base)

static void testhook_painter_vt_register(struct notifier_block *vt_notifier)
{
	struct kunit *test = kunit_get_current_test();
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	data->vt_notifier = vt_notifier;
}

static void testhook_painter_send_dpms(int mode)
{
	struct kunit *test = kunit_get_current_test();
	struct evdi_test_data *base = (struct evdi_test_data *)test->priv;
	struct evdi_vt_test *data = to_evdi_vt_test(base);

	data->dpms_mode = mode;
}

static int suite_test_vt_init(struct kunit *test)
{
	struct evdi_vt_test *data = kunit_kzalloc(test, sizeof(struct evdi_vt_test), GFP_KERNEL);

	evdi_test_data_init(test, &data->base);
	data->base.hooks.painter_vt_register = testhook_painter_vt_register;
	data->base.hooks.painter_send_dpms = testhook_painter_send_dpms;
	data->base.dev = evdi_drm_device_create(data->base.parent);

	return 0;
}

static void suite_test_vt_exit(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	if (data->base.dev) {
		evdi_drm_device_remove(data->base.dev);
		data->base.dev = NULL;
	}

	evdi_test_data_exit(test, &data->base);
	kunit_kfree(test, test->priv);
}

static int suite_test_vt_and_user_connected_init(struct kunit *test)
{
	int result = suite_test_vt_init(test);
	struct evdi_vt_test *data = (struct evdi_vt_test *)test->priv;

	KUNIT_EXPECT_EQ(test, result, 0);
	evdi_fake_compositor_create(test);
	evdi_fake_user_client_create(test);

	KUNIT_EXPECT_NE(test, data->dpms_mode, DRM_MODE_DPMS_OFF);
	evdi_fake_user_client_connect(test, data->base.dev);
	evdi_fake_compositor_connect(test, data->base.dev);

	return result;
}

static void suite_test_vt_and_user_connected_exit(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	evdi_fake_compositor_disconnect(test, data->base.dev);
	evdi_fake_user_client_disconnect(test, data->base.dev);

	return suite_test_vt_exit(test);
}

static void test_evdi_painter_registers_for_vt(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	KUNIT_EXPECT_NOT_NULL(test, data->vt_notifier->notifier_call);
}

static void test_evdi_painter_unregisters_for_vt_on_removal(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	evdi_drm_device_remove(data->base.dev);
	data->base.dev = NULL;

	KUNIT_EXPECT_NULL(test, data->vt_notifier->notifier_call);
}

static void test_evdi_painter_when_not_connected_does_not_send_dpms_off_event_on_fg_console_change(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);

	KUNIT_EXPECT_NE(test, data->dpms_mode, DRM_MODE_DPMS_OFF);

	data->vt_notifier->notifier_call(data->vt_notifier, 0, NULL);

	KUNIT_EXPECT_NE(test, data->dpms_mode, DRM_MODE_DPMS_OFF);
}

static void test_evdi_painter_when_connected_does_not_send_dpms_off_event_when_fg_console_has_not_changed(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);
	struct evdi_device *evdi = (struct evdi_device *)data->base.dev->dev_private;

	data->vt_notifier->notifier_call(data->vt_notifier, 0, NULL);

	KUNIT_EXPECT_EQ(test, data->dpms_mode, DRM_MODE_DPMS_ON);
	KUNIT_EXPECT_FALSE(test, evdi_painter_needs_full_modeset(evdi->painter));
}

static void test_evdi_painter_when_connected_sends_dpms_off_event_on_fg_console_change(struct kunit *test)
{
	struct evdi_vt_test *data = to_evdi_vt_test(test->priv);
	struct evdi_device *evdi = (struct evdi_device *)data->base.dev->dev_private;

	fg_console = fg_console + 1;

	data->vt_notifier->notifier_call(data->vt_notifier, 0, NULL);

	KUNIT_EXPECT_EQ(test, data->dpms_mode, DRM_MODE_DPMS_OFF);
	KUNIT_EXPECT_TRUE(test, evdi_painter_needs_full_modeset(evdi->painter));
}

static struct kunit_case evdi_test_cases[] = {
	KUNIT_CASE(test_evdi_painter_registers_for_vt),
	KUNIT_CASE(test_evdi_painter_unregisters_for_vt_on_removal),
	KUNIT_CASE(test_evdi_painter_when_not_connected_does_not_send_dpms_off_event_on_fg_console_change),
	{}
};

static struct kunit_case evdi_test_cases_with_user_connected[] = {
	KUNIT_CASE(test_evdi_painter_when_connected_does_not_send_dpms_off_event_when_fg_console_has_not_changed),
	KUNIT_CASE(test_evdi_painter_when_connected_sends_dpms_off_event_on_fg_console_change),
	{}
};

static struct kunit_suite evdi_test_suite = {
	.name = "drm_evdi_vt_tests",
	.test_cases = evdi_test_cases,
	.init = suite_test_vt_init,
	.exit = suite_test_vt_exit,
};

static struct kunit_suite evdi_test_suite_with_connected_user = {
	.name = "drm_evdi_vt_tests_with_connected_user",
	.test_cases = evdi_test_cases_with_user_connected,
	.init = suite_test_vt_and_user_connected_init,
	.exit = suite_test_vt_and_user_connected_exit,
};

kunit_test_suite(evdi_test_suite_with_connected_user);
kunit_test_suite(evdi_test_suite);
