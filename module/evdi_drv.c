// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc_helper.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
#include <linux/iommu.h>
#endif

#include "evdi_drv.h"
#include "evdi_drm.h"
#include "evdi_params.h"
#include "evdi_debug.h"
#include "evdi_platform_drv.h"
#include "evdi_sysfs.h"

MODULE_AUTHOR("DisplayLink (UK) Ltd.");
MODULE_DESCRIPTION("Extensible Virtual Display Interface");
MODULE_LICENSE("GPL");

#define EVDI_DEVICE_COUNT_MAX 16

static struct evdi_platform_drv_context {
	struct device *root_dev;
	unsigned int dev_count;
	struct platform_device *devices[EVDI_DEVICE_COUNT_MAX];
} g_ctx;

static struct drm_driver driver;

struct drm_ioctl_desc evdi_painter_ioctls[] = {
	DRM_IOCTL_DEF_DRV(EVDI_CONNECT, evdi_painter_connect_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_REQUEST_UPDATE,
			  evdi_painter_request_update_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_GRABPIX, evdi_painter_grabpix_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_DDCCI_RESPONSE, evdi_painter_ddcci_response_ioctl,
			  DRM_UNLOCKED),
};

static const struct vm_operations_struct evdi_gem_vm_ops = {
	.fault = evdi_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

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

static int evdi_enable_vblank(__always_unused struct drm_device *dev,
			      __always_unused unsigned int pipe)
{
	return 1;
}

static void evdi_disable_vblank(__always_unused struct drm_device *dev,
				__always_unused unsigned int pipe)
{
}

static struct drm_driver driver = {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
#else
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME
			 | DRIVER_ATOMIC,
#endif
	.unload = evdi_driver_unload,
	.preclose = evdi_driver_preclose,

	.postclose = evdi_driver_postclose,

	/* gem hooks */
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	.gem_free_object_unlocked = evdi_gem_free_object,
#else
	.gem_free_object = evdi_gem_free_object,
#endif
	.gem_vm_ops = &evdi_gem_vm_ops,

	.dumb_create = evdi_dumb_create,
	.dumb_map_offset = evdi_gem_mmap,
	.dumb_destroy = drm_gem_dumb_destroy,

	.ioctls = evdi_painter_ioctls,
	.num_ioctls = ARRAY_SIZE(evdi_painter_ioctls),

	.fops = &evdi_driver_fops,

	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import = drm_gem_prime_import,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_get_sg_table = evdi_prime_get_sg_table,
	.gem_prime_import_sg_table = evdi_prime_import_sg_table,

	.enable_vblank = evdi_enable_vblank,
	.disable_vblank = evdi_disable_vblank,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCH,
};

static void evdi_platform_device_add(struct evdi_platform_drv_context *ctx)
{
	struct platform_device *pdev = NULL;
	struct platform_device_info pdevinfo = {
		.parent = NULL,
		.name = "evdi",
		.id = ctx->dev_count,
		.res = NULL,
		.num_res = 0,
		.data = NULL,
		.size_data = 0,
		.dma_mask = DMA_BIT_MASK(32),
	};

	pdev = evdi_platform_dev_create(&pdevinfo);
	ctx->devices[ctx->dev_count++] = pdev;
}

struct platform_device *evdi_platform_dev_create(struct platform_device_info *info)
{
	struct platform_device *platform_dev = NULL;

	platform_dev = platform_device_register_full(info);
	if (dma_set_mask(&platform_dev->dev, DMA_BIT_MASK(64))) {
		EVDI_DEBUG("Unable to change dma mask to 64 bit. ");
		EVDI_DEBUG("Sticking with 32 bit\n");
	}

	EVDI_INFO("Evdi platform_device create\n");

	return platform_dev;
}

int evdi_platform_add_devices(struct device *device, unsigned int val)
{
	struct evdi_platform_drv_context *ctx =
		(struct evdi_platform_drv_context *)dev_get_drvdata(device);

	if (val == 0) {
		EVDI_WARN("Adding 0 devices has no effect\n");
		return 0;
	}
	if (val > EVDI_DEVICE_COUNT_MAX - ctx->dev_count) {
		EVDI_ERROR("Evdi device add failed. Too many devices.\n");
		return -EINVAL;
	}

	EVDI_DEBUG("Increasing device count to %u\n",
		   ctx->dev_count + val);
	while (val--)
		evdi_platform_device_add(ctx);
	return 0;
}

static int evdi_platform_device_probe(struct platform_device *pdev)
{
	struct drm_device *dev;
	int ret;
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
#if IS_ENABLED(CONFIG_IOMMU_API) && defined(CONFIG_INTEL_IOMMU)
	struct dev_iommu iommu;
#endif
#endif
	EVDI_CHECKPT();

/* Intel-IOMMU workaround: platform-bus unsupported, force ID-mapping */
#if IS_ENABLED(CONFIG_IOMMU_API) && defined(CONFIG_INTEL_IOMMU)
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	memset(&iommu, 0, sizeof(iommu));
	iommu.priv = (void *)-1;
	pdev->dev.iommu = &iommu;
#else
#define INTEL_IOMMU_DUMMY_DOMAIN                ((void *)-1)
	pdev->dev.archdata.iommu = INTEL_IOMMU_DUMMY_DOMAIN;
#endif
#endif

	dev = drm_dev_alloc(&driver, &pdev->dev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = evdi_driver_setup(dev);
	if (ret)
		goto err_free;

	ret = drm_dev_register(dev, 0);
	if (ret)
		goto err_free;

	return 0;

err_free:
	drm_dev_put(dev);
	return ret;
}

static int evdi_platform_device_remove(struct platform_device *pdev)
{
	struct drm_device *drm_dev =
	    (struct drm_device *)platform_get_drvdata(pdev);
	EVDI_CHECKPT();

	drm_dev_unplug(drm_dev);

	return 0;
}

void evdi_platform_remove_all_devices(struct device *device)
{
	int i;
	struct evdi_platform_drv_context *ctx =
		(struct evdi_platform_drv_context *)dev_get_drvdata(device);

	for (i = 0; i < ctx->dev_count; ++i) {
		if (ctx->devices[i]) {
			EVDI_INFO("Removing evdi %d\n", i);
			evdi_platform_dev_destroy(ctx->devices[i]);
			ctx->devices[i] = NULL;
		}
	}
	ctx->dev_count = 0;
}

void evdi_platform_dev_destroy(struct platform_device *dev)
{
	platform_device_unregister(dev);
	EVDI_INFO("Evdi platform_device destroy\n");
}

int evdi_platform_device_count(struct device *device)
{
	struct evdi_platform_drv_context *ctx =
		(struct evdi_platform_drv_context *)dev_get_drvdata(device);

	return ctx->dev_count;
}

static struct platform_driver evdi_platform_driver = {
	.probe = evdi_platform_device_probe,
	.remove = evdi_platform_device_remove,
	.driver = {
		   .name = "evdi",
		   .mod_name = KBUILD_MODNAME,
		   .owner = THIS_MODULE,
	}
};

static int __init evdi_init(void)
{
	int ret;

	EVDI_INFO("Initialising logging on level %u\n", evdi_loglevel);
	EVDI_INFO("Atomic driver:%s",
		(driver.driver_features & DRIVER_ATOMIC) ? "yes" : "no");

	g_ctx.root_dev = root_device_register("evdi");
	dev_set_drvdata(g_ctx.root_dev, &g_ctx);
	evdi_sysfs_init(g_ctx.root_dev);
	ret = platform_driver_register(&evdi_platform_driver);
	if (ret)
		return ret;

	if (evdi_initial_device_count)
		return evdi_platform_add_devices(
			g_ctx.root_dev, evdi_initial_device_count);

	return 0;
}

static void __exit evdi_exit(void)
{
	EVDI_CHECKPT();
	evdi_platform_remove_all_devices(g_ctx.root_dev);
	platform_driver_unregister(&evdi_platform_driver);

	if (!PTR_ERR_OR_ZERO(g_ctx.root_dev)) {
		evdi_sysfs_exit(g_ctx.root_dev);
		dev_set_drvdata(g_ctx.root_dev, NULL);
		root_device_unregister(g_ctx.root_dev);
	}
}

module_init(evdi_init);
module_exit(evdi_exit);
