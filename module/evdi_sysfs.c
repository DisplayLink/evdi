// SPDX-License-Identifier: GPL-2.0-only
/*
 * evdi_sysfs.c
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


#include <linux/device.h>
#include <linux/slab.h>
#include "evdi_sysfs.h"
#include "evdi_params.h"
#include "evdi_debug.h"
#include "evdi_platform_drv.h"

static ssize_t version_show(__always_unused struct device *dev,
			    __always_unused struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u.%u.%u\n", DRIVER_MAJOR,
			DRIVER_MINOR, DRIVER_PATCH);
}

static ssize_t count_show(__always_unused struct device *dev,
			  __always_unused struct device_attribute *attr,
			  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_platform_device_count(dev));
}

static ssize_t add_device_with_usb_path(__always_unused struct device *dev,
			 const char *buf, size_t count)
{
	char *usb_path = kstrdup(buf, GFP_KERNEL);
	char *temp_path = usb_path;
	char *bus_token = NULL;
	char *usb_token = NULL;
	char *itf_token = NULL;

	temp_path = strnstr(temp_path, "usb:", count);
	if (!temp_path)
		goto err_parse_usb_path;


	temp_path = strim(temp_path);

	bus_token = strsep(&temp_path, ":");
	if (!bus_token)
		goto err_parse_usb_path;

	usb_token = strsep(&temp_path, ":");
	if (!usb_token)
		goto err_parse_usb_path;

	itf_token = strsep(&temp_path, ":");


	EVDI_INFO("Attaching to %s:%s\n", bus_token, usb_token);
	return count;

err_parse_usb_path:
	EVDI_ERROR("Unable to parse usb path: %s", buf);
	kfree(usb_path);
	return -EINVAL;
}

static ssize_t add_store(struct device *dev,
			 __always_unused struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int val;
	int ret;

	if (strnstr(buf, "usb:", count))
		return add_device_with_usb_path(dev, buf, count);

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Invalid device count \"%s\"\n", buf);
		return -EINVAL;
	}

	ret = evdi_platform_add_devices(dev, val);
	if (ret)
		return ret;

	return count;
}

static ssize_t remove_all_store(struct device *dev,
				__always_unused struct device_attribute *attr,
				__always_unused const char *buf,
				size_t count)
{
	evdi_platform_remove_all_devices(dev);
	return count;
}

static ssize_t loglevel_show(__always_unused struct device *dev,
			     __always_unused struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_loglevel);
}

static ssize_t loglevel_store(__always_unused struct device *dev,
			      __always_unused struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Unable to parse %u\n", val);
		return -EINVAL;
	}
	if (val > EVDI_LOGLEVEL_VERBOSE) {
		EVDI_ERROR("Invalid loglevel %u\n", val);
		return -EINVAL;
	}

	EVDI_INFO("Setting loglevel to %u\n", val);
	evdi_loglevel = val;
	return count;
}

static struct device_attribute evdi_device_attributes[] = {
	__ATTR_RO(count),
	__ATTR_RO(version),
	__ATTR_RW(loglevel),
	__ATTR_WO(add),
	__ATTR_WO(remove_all)
};

void evdi_sysfs_init(struct device *root)
{
	int i;

	if (!PTR_ERR_OR_ZERO(root))
		for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++)
			device_create_file(root, &evdi_device_attributes[i]);
}

void evdi_sysfs_exit(struct device *root)
{
	int i;

	if (PTR_ERR_OR_ZERO(root)) {
		EVDI_ERROR("root device is null");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++)
		device_remove_file(root, &evdi_device_attributes[i]);
}

