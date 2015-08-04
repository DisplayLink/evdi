/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
#include <drm/drm_plane_helper.h>
#endif
#include "evdi_drv.h"
#include "evdi_ioctl.h"

static void evdi_crtc_dpms(struct drm_crtc *crtc, int mode)
{
  struct evdi_device *evdi = crtc->dev->dev_private;
  evdi_painter_dpms_notify(evdi, mode);
}

static bool evdi_crtc_mode_fixup(struct drm_crtc *crtc,
                                 const struct drm_display_mode *mode,
                                 struct drm_display_mode *adjusted_mode)
{
  return true;
}

static int evdi_crtc_mode_set(struct drm_crtc *crtc,
                              struct drm_display_mode *mode,
                              struct drm_display_mode *adjusted_mode,
                              int x,
                              int y,
                              struct drm_framebuffer *old_fb)
{
  struct drm_device *dev = NULL;
  struct evdi_device *evdi = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,15,0)
  struct evdi_framebuffer *ufb = to_evdi_fb(crtc->fb);
#else
  struct evdi_framebuffer *ufb = NULL;
  if (NULL == crtc->primary) {
    EVDI_DEBUG("evdi_crtc_mode_set primary plane is NULL");
    return 0;
  }
  ufb = to_evdi_fb(crtc->primary->fb);
#endif

  EVDI_ENTER();
  dev = ufb->base.dev;
  evdi = dev->dev_private;
  evdi_painter_mode_changed_notify(evdi, &ufb->base, adjusted_mode);
  /* damage all of it */
  evdi_handle_damage(ufb, 0, 0, ufb->base.width, ufb->base.height);
  EVDI_EXIT();
  return 0;
}


static void evdi_crtc_disable(struct drm_crtc *crtc)
{
  struct evdi_device *evdi = crtc->dev->dev_private;
  EVDI_CHECKPT();
  evdi_painter_crtc_state_notify(evdi, EVDI_CRTC_DISABLED);
}

static void evdi_crtc_destroy(struct drm_crtc *crtc)
{
  EVDI_CHECKPT();
  drm_crtc_cleanup(crtc);
  kfree(crtc);
}

static void evdi_crtc_prepare(struct drm_crtc *crtc)
{}

static void evdi_crtc_commit(struct drm_crtc *crtc)
{
  struct evdi_device *evdi = crtc->dev->dev_private;
  EVDI_CHECKPT();
  evdi_painter_crtc_state_notify(evdi, EVDI_CRTC_ENABLED);
}

static struct drm_crtc_helper_funcs evdi_helper_funcs = {
  .dpms = evdi_crtc_dpms,
  .mode_fixup = evdi_crtc_mode_fixup,
  .mode_set = evdi_crtc_mode_set,
  .prepare = evdi_crtc_prepare,
  .commit = evdi_crtc_commit,
  .disable = evdi_crtc_disable,
};

static const struct drm_crtc_funcs evdi_crtc_funcs = {
  .set_config = drm_crtc_helper_set_config,
  .destroy = evdi_crtc_destroy,
};

static int evdi_crtc_init(struct drm_device *dev)
{
  struct drm_crtc *crtc;
  int status = 0;

  EVDI_CHECKPT();
  crtc = kzalloc(sizeof(struct drm_crtc) + sizeof(struct drm_connector *), GFP_KERNEL);
  if (crtc == NULL) {
    return -ENOMEM;
  }

  status = drm_crtc_init(dev, crtc, &evdi_crtc_funcs);
  EVDI_DEBUG("drm_crtc_init: %d\n", status);
  drm_crtc_helper_add(crtc, &evdi_helper_funcs);

  return 0;
}

static const struct drm_mode_config_funcs evdi_mode_funcs = {
  .fb_create = evdi_fb_user_fb_create,
  .output_poll_changed = NULL,
};

int evdi_modeset_init(struct drm_device *dev)
{
  struct drm_encoder *encoder;
  EVDI_CHECKPT();
  drm_mode_config_init(dev);

  dev->mode_config.min_width = 640;
  dev->mode_config.min_height = 480;

  dev->mode_config.max_width = 3840;
  dev->mode_config.max_height = 2160;

  dev->mode_config.prefer_shadow = 0;
  dev->mode_config.preferred_depth = 24;

  dev->mode_config.funcs = &evdi_mode_funcs;

  drm_mode_create_dirty_info_property(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
  drm_dev_set_unique(dev, "%s", dev_name(dev->dev));
#endif
  evdi_crtc_init(dev);

  encoder = evdi_encoder_init(dev);

  evdi_connector_init(dev, encoder);

  return 0;
}

void evdi_modeset_cleanup(struct drm_device *dev)
{
  EVDI_CHECKPT();
  drm_mode_config_cleanup(dev);
}

