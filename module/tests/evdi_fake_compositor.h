/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2024 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_FAKE_COMPOSITOR_H
#define EVDI_FAKE_COMPOSITOR_H

#ifdef CONFIG_DRM_EVDI_KUNIT_TEST


#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <kunit/device.h>



/* kunit tests helpers faking userspace compositor leveraging evdi to add virtual display.
 * e.g. Xorg, gnome-wayland, kwin
 */
struct evdi_fake_compositor_data {
	struct evdi_framebuffer *efb;
	struct drm_display_mode mode;
};

void evdi_fake_compositor_create(struct kunit *test);
void evdi_fake_compositor_connect(struct kunit *test, struct drm_device *device);
void evdi_fake_compositor_disconnect(struct kunit *test, struct drm_device *device);


#endif // CONFIG_DRM_EVDI_KUNIT_TEST
#endif // EVDI_FAKE_COMPOSITOR_H

