/* SPDX-License-Identifier: GPL-2.0-only
 * evdi_platform_dev.h
 *
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

#ifndef _EVDI_PLATFORM_DEV_H_
#define _EVDI_PLATFORM_DEV_H_

#include <linux/types.h>
#include <linux/version.h>

struct platform_device_info;
struct platform_device;
struct drm_driver;
struct device;

struct platform_device *evdi_platform_dev_create(struct platform_device_info *info);
void evdi_platform_dev_destroy(struct platform_device *dev);

int evdi_platform_device_probe(struct platform_device *pdev);
/* EL9 kernel removed the callback that was returning void  */
#if KERNEL_VERSION(6, 11, 0) <= LINUX_VERSION_CODE
void evdi_platform_device_remove(struct platform_device *pdev);
#else
int evdi_platform_device_remove(struct platform_device *pdev);
#endif
bool evdi_platform_device_is_free(struct platform_device *pdev);
void evdi_platform_device_link(struct platform_device *pdev,
				struct device *parent);
void evdi_platform_device_unlink_if_linked_with(struct platform_device *pdev,
				struct device *parent);

#endif

