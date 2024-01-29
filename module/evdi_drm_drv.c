// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE || defined(EL8) || defined(EL9)
#include <drm/drm_ioctl.h>
#include <drm/drm_file.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#elif KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
#else
#include <drm/drmP.h>
#endif
#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <drm/drm_probe_helper.h>
#endif
#if KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <drm/drm_managed.h>
#endif
#include <drm/drm_atomic_helper.h>
#include "evdi_drm_drv.h"
#include "evdi_platform_drv.h"
#include "evdi_cursor.h"
#include "evdi_debug.h"
#include "evdi_drm.h"

#if KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE || defined(EL8)
#define EVDI_DRM_UNLOCKED 0
#else
#define EVDI_DRM_UNLOCKED DRM_UNLOCKED
#endif

static struct drm_driver driver;

struct drm_ioctl_desc evdi_painter_ioctls[] = {
	DRM_IOCTL_DEF_DRV(EVDI_CONNECT, evdi_painter_connect_ioctl, EVDI_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_REQUEST_UPDATE, evdi_painter_request_update_ioctl, EVDI_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_GRABPIX, evdi_painter_grabpix_ioctl, EVDI_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_DDCCI_RESPONSE, evdi_painter_ddcci_response_ioctl, EVDI_DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_ENABLE_CURSOR_EVENTS, evdi_painter_enable_cursor_events_ioctl, EVDI_DRM_UNLOCKED),
};

#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
static const struct vm_operations_struct evdi_gem_vm_ops = {
	.fault = evdi_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};
#endif

static const struct file_operations evdi_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.mmap = evdi_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.unlocked_ioctl = drm_ioctl,
	.release = drm_release,

#ifdef CONFIG_COMPAT
	.compat_ioctl = evdi_compat_ioctl,
#endif

	.llseek = noop_llseek,
};

#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
static int evdi_enable_vblank(__always_unused struct drm_device *dev,
			      __always_unused unsigned int pipe)
{
	return 1;
}

static void evdi_disable_vblank(__always_unused struct drm_device *dev,
				__always_unused unsigned int pipe)
{
}
#endif

static struct drm_driver driver = {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
#else
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME
			 | DRIVER_ATOMIC,
#endif

	.open = evdi_driver_open,
	.postclose = evdi_driver_postclose,

	/* gem hooks */
#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#elif KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	.gem_free_object_unlocked = evdi_gem_free_object,
#else
	.gem_free_object = evdi_gem_free_object,
#endif

#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
	.gem_vm_ops = &evdi_gem_vm_ops,
#endif

	.dumb_create = evdi_dumb_create,
	.dumb_map_offset = evdi_gem_mmap,
#if KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
	.dumb_destroy = drm_gem_dumb_destroy,
#endif

	.ioctls = evdi_painter_ioctls,
	.num_ioctls = ARRAY_SIZE(evdi_painter_ioctls),

	.fops = &evdi_driver_fops,

#if KERNEL_VERSION(6, 6, 0) > LINUX_VERSION_CODE
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
#endif
	.gem_prime_import = drm_gem_prime_import,
#if KERNEL_VERSION(6, 6, 0) > LINUX_VERSION_CODE
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
#endif
#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
	.preclose = evdi_driver_preclose,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_get_sg_table = evdi_prime_get_sg_table,
	.enable_vblank = evdi_enable_vblank,
	.disable_vblank = evdi_disable_vblank,
#endif
	.gem_prime_import_sg_table = evdi_prime_import_sg_table,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCH,
};

static void evdi_drm_device_release_cb(__always_unused struct drm_device *dev,
				       __always_unused void *ptr)
{
	struct evdi_device *evdi = dev->dev_private;

	evdi_cursor_free(evdi->cursor);
	evdi_painter_cleanup(evdi->painter);
	kfree(evdi);
	dev->dev_private = NULL;
	EVDI_INFO("Evdi drm_device removed.\n");
}

static int evdi_drm_device_init(struct drm_device *dev)
{
	struct evdi_device *evdi;
	int ret;

	EVDI_CHECKPT();
	evdi = kzalloc(sizeof(struct evdi_device), GFP_KERNEL);
	if (!evdi)
		return -ENOMEM;

	evdi->ddev = dev;
	evdi->dev_index = dev->primary->index;
	evdi->cursor_events_enabled = false;
	dev->dev_private = evdi;
	ret = evdi_painter_init(evdi);
	if (ret)
		goto err_free;
	ret =  evdi_cursor_init(&evdi->cursor);
	if (ret)
		goto err_free;

	evdi_modeset_init(dev);
#ifdef CONFIG_FB
	ret = evdi_fbdev_init(dev);
	if (ret)
		goto err_init;
#endif /* CONFIG_FB */

	ret = drm_vblank_init(dev, 1);
	if (ret)
		goto err_init;
	drm_kms_helper_poll_init(dev);

#if KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE || defined(EL8)
	ret = drmm_add_action_or_reset(dev, evdi_drm_device_release_cb, NULL);
	if (ret)
		goto err_init;
#endif

	return 0;

err_init:
#ifdef CONFIG_FB
	evdi_fbdev_cleanup(dev);
#endif /* CONFIG_FB */
err_free:
	EVDI_ERROR("Failed to setup drm device %d\n", ret);
	evdi_cursor_free(evdi->cursor);
	kfree(evdi->painter);
	kfree(evdi);
	dev->dev_private = NULL;
	return ret;
}

int evdi_driver_open(struct drm_device *dev, __always_unused struct drm_file *file)
{
	char buf[100];

	evdi_log_process(buf, sizeof(buf));
	EVDI_INFO("(card%d) Opened by %s\n", dev->primary->index, buf);
	return 0;
}

static void evdi_driver_close(struct drm_device *drm_dev, struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;

	EVDI_CHECKPT();
	if (evdi)
		evdi_painter_close(evdi, file);
}

void evdi_driver_preclose(struct drm_device *drm_dev, struct drm_file *file)
{
	evdi_driver_close(drm_dev, file);
}

void evdi_driver_postclose(struct drm_device *dev, struct drm_file *file)
{
	char buf[100];

	evdi_log_process(buf, sizeof(buf));
	evdi_driver_close(dev, file);
	EVDI_INFO("(card%d) Closed by %s\n", dev->primary->index, buf);
}

struct drm_device *evdi_drm_device_create(struct device *parent)
{
	struct drm_device *dev = NULL;
	int ret;

	dev = drm_dev_alloc(&driver, parent);
	if (IS_ERR(dev))
		return dev;

	ret = evdi_drm_device_init(dev);
	if (ret)
		goto err_free;

	ret = drm_dev_register(dev, 0);
	if (ret)
		goto err_free;

	return dev;

err_free:
	drm_dev_put(dev);
	return ERR_PTR(ret);
}

static void evdi_drm_device_deinit(struct drm_device *dev)
{
	drm_kms_helper_poll_fini(dev);
#ifdef CONFIG_FB
	evdi_fbdev_unplug(dev);
	evdi_fbdev_cleanup(dev);
#endif /* CONFIG_FB */
	evdi_modeset_cleanup(dev);
	drm_atomic_helper_shutdown(dev);
}

int evdi_drm_device_remove(struct drm_device *dev)
{
	drm_dev_unplug(dev);
	evdi_drm_device_deinit(dev);
#if KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
	evdi_drm_device_release_cb(dev, NULL);
#endif
	drm_dev_put(dev);
	return 0;
}

