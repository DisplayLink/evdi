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

#include <linux/platform_device.h>
#include <drm/drmP.h>
#include "evdi_drv.h"

int evdi_driver_load(struct drm_device *dev, unsigned long flags)
{
  struct platform_device* platdev = NULL;
  struct evdi_device *evdi;
  int ret;

  EVDI_CHECKPT();
  evdi = kzalloc(sizeof(struct evdi_device), GFP_KERNEL);
  if (!evdi) {
    return -ENOMEM;
  }

  evdi->ddev = dev;
  dev->dev_private = evdi;

  EVDI_CHECKPT();
  ret = evdi_modeset_init(dev);

  ret = evdi_fbdev_init(dev);

  evdi_painter_init(evdi);
  evdi_stats_init(evdi);

  drm_kms_helper_poll_init(dev);

  platdev = to_platform_device(dev->dev);
  platform_set_drvdata(platdev, dev);

  return 0;
}

int evdi_driver_unload(struct drm_device *dev)
{
  struct evdi_device *evdi = dev->dev_private;
  EVDI_CHECKPT();

  drm_kms_helper_poll_fini(dev);
  drm_connector_unplug_all(dev);
  evdi_fbdev_unplug(dev);
  drm_unplug_dev(dev);

  evdi_painter_cleanup(evdi);
  evdi_stats_cleanup(evdi);
  evdi_fbdev_cleanup(dev);
  evdi_modeset_cleanup(dev);

  kfree(evdi);
  return 0;
}

