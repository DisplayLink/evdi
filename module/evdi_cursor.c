/*
 * evdi_cursor.c
 *
 * Copyright (c) 2016 The Chromium OS Authors
 * Copyright (c) 2016 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <linux/compiler.h>

#include "evdi_cursor.h"
#include "evdi_drv.h"

#define EVDI_CURSOR_W 64
#define EVDI_CURSOR_H 64
#define EVDI_CURSOR_BUF (EVDI_CURSOR_W * EVDI_CURSOR_H)

/*
 * EVDI drm cursor private structure.
 */
struct evdi_cursor {
	uint32_t buffer[EVDI_CURSOR_BUF];
	bool enabled;
	int x;
	int y;
};

int evdi_cursor_alloc(struct evdi_cursor **cursor)
{
	struct evdi_cursor *new_cursor = kzalloc(sizeof(struct evdi_cursor),
		GFP_KERNEL);
	if (!new_cursor)
		return -ENOMEM;
	*cursor = new_cursor;
	return 0;
}

void evdi_cursor_free(struct evdi_cursor *cursor)
{
	BUG_ON(!cursor);
	kfree(cursor);
}

void evdi_cursor_copy(struct evdi_cursor *dst, struct evdi_cursor *src)
{
	memcpy(dst, src, sizeof(struct evdi_cursor));
}

bool evdi_cursor_enabled(struct evdi_cursor *cursor)
{
	return cursor->enabled;
}

static int evdi_cursor_download(struct evdi_cursor *cursor,
		struct drm_gem_object *obj)
{
	struct evdi_gem_object *evdi_gem_obj = to_evdi_bo(obj);
	uint32_t *src_ptr, *dst_ptr;
	size_t i;
	int ret = evdi_gem_vmap(evdi_gem_obj);

	if (ret != 0) {
		DRM_ERROR("failed to vmap cursor\n");
		return ret;
	}

	src_ptr = evdi_gem_obj->vmapping;
	dst_ptr = cursor->buffer;
	for (i = 0; i < EVDI_CURSOR_BUF; ++i)
		dst_ptr[i] = le32_to_cpu(src_ptr[i]);
	return 0;
}

int evdi_cursor_set(__maybe_unused struct drm_crtc *crtc, struct drm_file *file,
		uint32_t handle, uint32_t width, uint32_t height,
		struct evdi_cursor *cursor)
{
	if (handle) {
		struct drm_gem_object *obj;
		int err;
		/* Currently we only support 64x64 cursors */
		if (width != EVDI_CURSOR_W || height != EVDI_CURSOR_H) {
			DRM_ERROR("we currently only support %dx%d cursors\n",
					EVDI_CURSOR_W, EVDI_CURSOR_H);
			return -EINVAL;
		}
		#if KERNEL_VERSION(4, 6, 0) >= LINUX_VERSION_CODE
			obj = drm_gem_object_lookup(crtc->dev, file, handle);
		#else
			obj = drm_gem_object_lookup(file, handle);
		#endif

		if (!obj) {
			DRM_ERROR("failed to lookup gem object.\n");
			return -EINVAL;
		}
		err = evdi_cursor_download(cursor, obj);
		drm_gem_object_unreference(obj);
		if (err != 0) {
			DRM_ERROR("failed to copy cursor.\n");
			return err;
		}
		cursor->enabled = true;
	} else {
		cursor->enabled = false;
	}

	return 0;
}

int evdi_cursor_move(__always_unused struct drm_crtc *crtc,
		     int x, int y, struct evdi_cursor *cursor)
{
	cursor->x = x;
	cursor->y = y;
	return 0;
}

static inline uint32_t blend_component(uint32_t pixel,
				  uint32_t blend,
				  uint32_t alpha)
{
	uint32_t pre_blend = (pixel * (255 - alpha) + blend * alpha);

	return (pre_blend + ((pre_blend + 1) << 8)) >> 16;
}

static inline uint32_t blend_alpha(const uint32_t pixel_val32,
				uint32_t blend_val32)
{
	uint32_t alpha = (blend_val32 >> 24);

	return blend_component(pixel_val32 & 0xff,
			       blend_val32 & 0xff, alpha) |
			blend_component((pixel_val32 & 0xff00) >> 8,
				(blend_val32 & 0xff00) >> 8, alpha) << 8 |
			blend_component((pixel_val32 & 0xff0000) >> 16,
				(blend_val32 & 0xff0000) >> 16, alpha) << 16;
}

int evdi_cursor_composing_pixel(char __user *buffer,
				int const cursor_value,
				int const fb_value,
				int cmd_offset)
{
	int const composed_value = blend_alpha(fb_value, cursor_value);

	return copy_to_user(buffer + cmd_offset, &composed_value, 4);
}

int evdi_cursor_composing_and_copy(struct evdi_cursor *cursor,
				   struct evdi_framebuffer *ufb,
				   char __user *buffer,
				   int buf_byte_stride,
				   __always_unused int const max_x,
				   __always_unused int const max_y)
{
	int x, y;
	struct drm_framebuffer *fb = &ufb->base;
	int h_cursor_w = EVDI_CURSOR_W >> 1;
	int h_cursor_h = EVDI_CURSOR_H >> 1;

	for (y = -EVDI_CURSOR_H/2; y < EVDI_CURSOR_H/2; ++y) {
		for (x = -EVDI_CURSOR_W/2; x < EVDI_CURSOR_W/2; ++x) {
			uint32_t curs_val;
			int *fbsrc;
			int fb_value;
			int cmd_offset;
			int cursor_pix;
			int const mouse_pix_x = cursor->x + x + h_cursor_w;
			int const mouse_pix_y = cursor->y + y + h_cursor_h;
			bool const is_pix_sane =
				mouse_pix_x >= 0 &&
				mouse_pix_y >= 0 &&
				mouse_pix_x < fb->width &&
				mouse_pix_y < fb->height  &&
				cursor &&
				cursor->enabled;

			if (!is_pix_sane)
				continue;

			cursor_pix = h_cursor_w+x +
				    (h_cursor_h+y)*EVDI_CURSOR_W;
			if (cursor_pix < 0  ||
			    cursor_pix > EVDI_CURSOR_BUF-1) {
				EVDI_WARN("cursor %d,%d\n", x, y);
				continue;
			}
			curs_val = cursor->buffer[cursor_pix];
			fbsrc = (int *)ufb->obj->vmapping;
			fb_value = *(fbsrc + ((fb->pitches[0]>>2) *
						  mouse_pix_y + mouse_pix_x));
			cmd_offset = (buf_byte_stride * mouse_pix_y) +
						       (mouse_pix_x * 4);
			if (evdi_cursor_composing_pixel(buffer,
						    curs_val,
						    fb_value,
						    cmd_offset)) {
				EVDI_ERROR("Failed to compose cursor pixel\n");
				return -EFAULT;
			}
		}
	}

	return 0;
}

void evdi_get_cursor_position(int *x, int *y, struct evdi_cursor *cursor)
{
	*x = cursor->x;
	*y = cursor->y;
}
