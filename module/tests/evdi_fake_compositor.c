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

#include <drm/drm_fourcc.h>

#include "evdi_drm_drv.h"
#include "evdi_drm.h"
#include "tests/evdi_test.h"
#include "tests/evdi_fake_compositor.h"


void evdi_fake_compositor_create(struct kunit *test)
{
	struct kunit_resource *resource;
	struct evdi_fake_compositor_data *compositor_data;

	resource = kunit_find_named_resource(test, "fake_wayland");
	if (resource) {
		KUNIT_FAIL(test, "fake_wayland data already exists");
		return;
	}

	resource = kunit_kzalloc(test, sizeof(struct kunit_resource), GFP_KERNEL);
	compositor_data = kunit_kzalloc(test, sizeof(struct evdi_fake_compositor_data), GFP_KERNEL);
	kunit_add_named_resource(test, NULL, NULL, resource, "fake_wayland", compositor_data);

	static const struct drm_display_mode default_mode = {
			DRM_SIMPLE_MODE(640, 480, 64, 48)
	};
	struct evdi_framebuffer efb =  {
		.base = {
			.format = drm_format_info(DRM_FORMAT_XRGB8888),
			.pitches = { 4*640, 0, 0 },
			.width = 640,
			.height = 480,
			},
		.obj = NULL,
		.active = true
	};

	compositor_data->efb = kunit_kzalloc(test, sizeof(struct evdi_framebuffer), GFP_KERNEL);
	memcpy(compositor_data->efb, &efb, sizeof(struct evdi_framebuffer));
	memcpy(&compositor_data->mode, &default_mode, sizeof(default_mode));
}

/* Stands in for the atomic commit a real compositor would have done. */
static void fake_compositor_set_kms_state(struct drm_device *device,
					  struct evdi_framebuffer *efb,
					  struct drm_display_mode *mode)
{
	struct evdi_device *evdi = (struct evdi_device *)device->dev_private;
	struct drm_crtc *crtc = evdi->crtc;
	struct drm_plane *primary = crtc ? crtc->primary : NULL;

	if (!crtc || !crtc->state || !primary || !primary->state)
		return;

	crtc->state->enable = efb != NULL;
	crtc->state->active = efb != NULL;
	primary->state->crtc = efb ? crtc : NULL;
	primary->state->fb = efb ? &efb->base : NULL;

	if (mode)
		crtc->state->adjusted_mode = *mode;
}

void evdi_fake_compositor_connect(struct kunit *test, struct drm_device *device)
{
	struct kunit_resource *resource = kunit_find_named_resource(test, "fake_wayland");
	struct evdi_fake_compositor_data *compositor_data = resource->data;
	struct evdi_device *evdi = (struct evdi_device *)device->dev_private;

	fake_compositor_set_kms_state(device, compositor_data->efb,
				      &compositor_data->mode);

	evdi_painter_set_scanout_buffer(evdi->painter, compositor_data->efb);
	evdi_painter_mode_changed_notify(evdi, &compositor_data->mode);
	evdi_painter_dpms_notify(evdi->painter, DRM_MODE_DPMS_ON);
}

void evdi_fake_compositor_disconnect(__maybe_unused struct kunit *test, struct drm_device *device)
{
	fake_compositor_set_kms_state(device, NULL, NULL);
}

