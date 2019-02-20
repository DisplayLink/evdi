// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2017 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <linux/version.h>
#include "evdi_drv.h"

/*
 * dummy connector to just get EDID,
 * all EVDI appear to have a DVI-D
 */

static int evdi_get_modes(struct drm_connector *connector)
{
	struct evdi_device *evdi = connector->dev->dev_private;
	struct edid *edid = NULL;
	int ret = 0;

	edid = (struct edid *)evdi_painter_get_edid_copy(evdi);

	if (!edid) {
#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
		drm_connector_update_edid_property(connector, NULL);
#else
		drm_mode_connector_update_edid_property(connector, NULL);
#endif
		return 0;
	}

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
	ret = drm_connector_update_edid_property(connector, edid);
#else
	ret = drm_mode_connector_update_edid_property(connector, edid);
#endif

	if (!ret)
		drm_add_edid_modes(connector, edid);
	else
		EVDI_ERROR("Failed to set edid modes! error: %d", ret);

	kfree(edid);
	return ret;
}

static enum drm_mode_status evdi_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
	struct evdi_device *evdi = connector->dev->dev_private;
	uint32_t mode_area = mode->hdisplay * mode->vdisplay;

	if (evdi->sku_area_limit == 0)
		return MODE_OK;

	if (mode_area > evdi->sku_area_limit) {
		EVDI_WARN("(dev=%d) Mode %dx%d@%d rejected\n",
			evdi->dev_index,
			mode->hdisplay,
			mode->vdisplay,
			drm_mode_vrefresh(mode));
		return MODE_BAD;
	}

	return MODE_OK;
}

static enum drm_connector_status
evdi_detect(struct drm_connector *connector, __always_unused bool force)
{
	struct evdi_device *evdi = connector->dev->dev_private;

	EVDI_CHECKPT();
	if (evdi_painter_is_connected(evdi)) {
		EVDI_DEBUG("(dev=%d) poll connector state: connected\n",
			   evdi->dev_index);
		return connector_status_connected;
	}
	EVDI_DEBUG("(dev=%d) poll connector state: disconnected\n",
		   evdi->dev_index);
	return connector_status_disconnected;
}

static void evdi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static struct drm_encoder *evdi_best_encoder(struct drm_connector *connector)
{

	return drm_encoder_find(connector->dev,
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
				NULL,
#endif
				connector->encoder_ids[0]);
}

static struct drm_connector_helper_funcs evdi_connector_helper_funcs = {
	.get_modes = evdi_get_modes,
	.mode_valid = evdi_mode_valid,
	.best_encoder = evdi_best_encoder,
};

static const struct drm_connector_funcs evdi_connector_funcs = {
#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
	.dpms = drm_atomic_helper_connector_dpms,
#endif
	.detect = evdi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = evdi_connector_destroy,
#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
	.set_property = drm_atomic_helper_connector_set_property,
#endif
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state
};

int evdi_connector_init(struct drm_device *dev, struct drm_encoder *encoder)
{
	struct drm_connector *connector;

	connector = kzalloc(sizeof(struct drm_connector), GFP_KERNEL);
	if (!connector)
		return -ENOMEM;

	/* TODO: Initialize connector with actual connector type */
	drm_connector_init(dev, connector, &evdi_connector_funcs,
			   DRM_MODE_CONNECTOR_DVII);
	drm_connector_helper_add(connector, &evdi_connector_helper_funcs);
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_register(connector);

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
	drm_connector_attach_encoder(connector, encoder);
#else
	drm_mode_connector_attach_encoder(connector, encoder);
#endif

#if KERNEL_VERSION(4, 9, 0) > LINUX_VERSION_CODE
	drm_object_attach_property(&connector->base,
				   dev->mode_config.dirty_info_property, 1);
#endif

	return 0;
}
