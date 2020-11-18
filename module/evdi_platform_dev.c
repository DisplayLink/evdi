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
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include "evdi_debug.h"

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


