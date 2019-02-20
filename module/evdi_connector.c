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
		drm_mode_connector_update_edid_property(connector, NULL);
		return 0;
	}

	ret = drm_mode_connector_update_edid_property(connector, edid);
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
		EVDI_WARN("Mode %dx%d@%d rejected\n",
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
		EVDI_DEBUG("(dev=%d) Painter is connected\n", evdi->dev_index);
		return connector_status_connected;
	}
	EVDI_DEBUG("Painter is disconnected\n");
	return connector_status_disconnected;
}

static struct drm_encoder *evdi_best_single_encoder(struct drm_connector
						    *connector)
{
	int enc_id = connector->encoder_ids[0];

	return drm_encoder_find(connector->dev,
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
				 NULL,
#endif
				 enc_id);
}

static int evdi_connector_set_property(
			__always_unused struct drm_connector *connector,
			__always_unused struct drm_property *property,
			__always_unused uint64_t val)
{
	return 0;
}

static void evdi_connector_destroy(struct drm_connector *connector)
{
#if KERNEL_VERSION(3, 17, 0) <= LINUX_VERSION_CODE
	drm_connector_unregister(connector);
#else
	drm_sysfs_connector_remove(connector);
#endif
	drm_connector_cleanup(connector);
	kfree(connector);
}

static struct drm_connector_helper_funcs evdi_connector_helper_funcs = {
	.get_modes = evdi_get_modes,
	.mode_valid = evdi_mode_valid,
	.best_encoder = evdi_best_single_encoder,
};

static const struct drm_connector_funcs evdi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = evdi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = evdi_connector_destroy,
	.set_property = evdi_connector_set_property,
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

#if KERNEL_VERSION(3, 17, 0) <= LINUX_VERSION_CODE
	drm_connector_register(connector);
#else
	drm_sysfs_connector_add(connector);
#endif
	drm_mode_connector_attach_encoder(connector, encoder);

#if KERNEL_VERSION(4, 9, 0) > LINUX_VERSION_CODE
	drm_object_attach_property(&connector->base,
				   dev->mode_config.dirty_info_property, 1);
#endif

	return 0;
}
