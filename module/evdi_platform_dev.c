// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 DisplayLink (UK) Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evdi_platform_dev.h"
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
#include <linux/iommu.h>
#endif

#include "evdi_platform_drv.h"
#include "evdi_debug.h"
#include "evdi_drm_drv.h"

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

void evdi_platform_dev_destroy(struct platform_device *dev)
{
	platform_device_unregister(dev);
	EVDI_INFO("Evdi platform_device destroy\n");
}

int evdi_platform_device_probe(struct platform_device *pdev)
{
	struct drm_device *dev;
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

	dev = evdi_drm_device_create(&pdev->dev);
	platform_set_drvdata(pdev, dev);
	return PTR_ERR_OR_ZERO(dev);
}

int evdi_platform_device_remove(struct platform_device *pdev)
{
	struct drm_device *drm_dev = (struct drm_device *)platform_get_drvdata(pdev);

	EVDI_CHECKPT();

	evdi_drm_device_remove(drm_dev);
	return 0;
}

bool evdi_platform_device_is_free(struct platform_device *pdev)
{
	struct drm_device *drm_dev =
		(struct drm_device *)platform_get_drvdata(pdev);
	struct evdi_device *evdi = drm_dev->dev_private;

	if (evdi && !evdi_painter_is_connected(evdi->painter))
		return true;
	return false;
}

