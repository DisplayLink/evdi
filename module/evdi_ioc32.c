// SPDX-License-Identifier: GPL-2.0-only
/*
 * evdi_ioc32.c
 *
 * Copyright (c) 2016 The Chromium OS Authors
 * Copyright (c) 2017 - 2020 DisplayLink (UK) Ltd.
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

#include <linux/compat.h>

#include <linux/version.h>
#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE || defined(EL9)
#include <drm/drm_ioctl.h>
#elif KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_edid.h>
#include "evdi_drm.h"

#include "evdi_drm_drv.h"

struct drm_evdi_connect32 {
	int32_t connected;
	int32_t dev_index;
	uint32_t edid_ptr32;
	uint32_t edid_length;
	uint32_t pixel_area_limit;
	uint32_t pixel_per_second_limit;
};

struct drm_evdi_grabpix32 {
	uint32_t mode;
	int32_t buf_width;
	int32_t buf_height;
	int32_t buf_byte_stride;
	uint32_t buffer_ptr32;
	int32_t num_rects;
	uint32_t rects_ptr32;
};

static int compat_evdi_connect(struct file *file,
				unsigned int __always_unused cmd,
				unsigned long arg)
{
	struct drm_evdi_connect32 req32;
	struct drm_evdi_connect krequest;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32)))
		return -EFAULT;

	krequest.connected = req32.connected;
	krequest.dev_index = req32.dev_index;
	krequest.edid = compat_ptr(req32.edid_ptr32);
	krequest.edid_length = req32.edid_length;
	krequest.pixel_area_limit = req32.pixel_area_limit;
	krequest.pixel_per_second_limit = req32.pixel_per_second_limit;

	return drm_ioctl_kernel(file, evdi_painter_connect_ioctl, &krequest, 0);
}

static int compat_evdi_grabpix(struct file *file,
				unsigned int __always_unused cmd,
				unsigned long arg)
{
	struct drm_evdi_grabpix32 req32;
	struct drm_evdi_grabpix krequest;
	int ret;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32)))
		return -EFAULT;

	krequest.mode = req32.mode;
	krequest.buf_width = req32.buf_width;
	krequest.buf_height = req32.buf_height;
	krequest.buf_byte_stride = req32.buf_byte_stride;
	krequest.buffer = compat_ptr(req32.buffer_ptr32);
	krequest.num_rects = req32.num_rects;
	krequest.rects = compat_ptr(req32.rects_ptr32);

	ret = drm_ioctl_kernel(file, evdi_painter_grabpix_ioctl, &krequest, 0);
	if (ret)
		return ret;

	req32.num_rects = krequest.num_rects;
	if (copy_to_user((void __user *)arg, &req32, sizeof(req32)))
		return -EFAULT;
	return 0;
}

static drm_ioctl_compat_t *evdi_compat_ioctls[] = {
	[DRM_EVDI_CONNECT] = compat_evdi_connect,
	[DRM_EVDI_GRABPIX] = compat_evdi_grabpix,
};

/*
 * Called whenever a 32-bit process running under a 64-bit kernel
 * performs an ioctl on /dev/dri/card<n>.
 *
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or negative number on failure.
 */
long evdi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *fn = NULL;
	int ret;

	if (nr < DRM_COMMAND_BASE || nr >= DRM_COMMAND_END)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr < DRM_COMMAND_BASE + ARRAY_SIZE(evdi_compat_ioctls))
		fn = evdi_compat_ioctls[nr - DRM_COMMAND_BASE];

	if (fn != NULL)
		ret = (*fn) (filp, cmd, arg);
	else
		ret = drm_ioctl(filp, cmd, arg);

	return ret;
}
