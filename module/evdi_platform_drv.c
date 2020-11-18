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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include "evdi_params.h"
#include "evdi_debug.h"
#include "evdi_platform_drv.h"
#include "evdi_platform_dev.h"
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

static int evdi_platform_drv_get_free_idx(struct evdi_platform_drv_context *ctx)
{
	int i;

	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		if (ctx->devices[i] == NULL)
			return i;
	}
	return -ENOMEM;
}

int evdi_platform_device_add(struct device *device, struct device *parent)
{
	struct platform_device *pdev = NULL;
	struct evdi_platform_drv_context *ctx =
		(struct evdi_platform_drv_context *)dev_get_drvdata(device);
	struct platform_device_info pdevinfo = {
		.parent = parent,
		.name = DRIVER_NAME,
		.id = evdi_platform_drv_get_free_idx(ctx),
		.res = NULL,
		.num_res = 0,
		.data = NULL,
		.size_data = 0,
		.dma_mask = DMA_BIT_MASK(32),
	};

	if (pdevinfo.id < 0 || ctx->dev_count >= EVDI_DEVICE_COUNT_MAX) {
		EVDI_ERROR("Evdi device add failed. Too many devices.\n");
		return -EINVAL;
	}

	pdev = evdi_platform_dev_create(&pdevinfo);
	ctx->devices[pdevinfo.id] = pdev;
	ctx->dev_count++;
	return 0;
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

	EVDI_DEBUG("Increasing device count to %u\n", ctx->dev_count + val);
	while (val-- && evdi_platform_device_add(device, NULL) == 0)
		;
	return 0;
}

void evdi_platform_remove_all_devices(struct device *device)
{
	int i;
	struct evdi_platform_drv_context *ctx =
		(struct evdi_platform_drv_context *)dev_get_drvdata(device);

	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		if (ctx->devices[i]) {
			EVDI_INFO("Removing evdi %d\n", i);
			evdi_platform_dev_destroy(ctx->devices[i]);
			ctx->devices[i] = NULL;
		}
	}
	ctx->dev_count = 0;
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
		   .name = DRIVER_NAME,
		   .mod_name = KBUILD_MODNAME,
		   .owner = THIS_MODULE,
	}
};

static int __init evdi_init(void)
{
	int ret;

	EVDI_INFO("Initialising logging on level %u\n", evdi_loglevel);
	EVDI_INFO("Atomic driver: yes");

	memset(&g_ctx, 0, sizeof(g_ctx));
	g_ctx.root_dev = root_device_register(DRIVER_NAME);
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
