/*
 * evdi_cursor.h
 *
 * Copyright (c) 2016 The Chromium OS Authors
 * Copyright (c) 2016 - 2017 DisplayLink (UK) Ltd.
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

#ifndef _EVDI_CURSOR_H_
#define _EVDI_CURSOR_H_

#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>

struct evdi_cursor;
struct evdi_framebuffer;
struct evdi_cursor_hline {
	uint32_t *buffer;
	int width;
	int offset;
};

extern int evdi_cursor_alloc(struct evdi_cursor **cursor);
extern void evdi_cursor_free(struct evdi_cursor *cursor);
extern void evdi_cursor_copy(struct evdi_cursor *dst, struct evdi_cursor *src);
extern bool evdi_cursor_enabled(struct evdi_cursor *cursor);
extern void evdi_cursor_get_hline(struct evdi_cursor *cursor, int x, int y,
		struct evdi_cursor_hline *hline);
extern int evdi_cursor_set(struct drm_crtc *crtc, struct drm_file *file,
		uint32_t handle, uint32_t width, uint32_t height,
		struct evdi_cursor *cursor);
extern int evdi_cursor_move(struct drm_crtc *crtc, int x, int y,
		struct evdi_cursor *cursor);
extern void evdi_get_cursor_position(int *x, int *y,
				     struct evdi_cursor *cursor);
extern int evdi_cursor_composing_pixel(char *buffer,
				       int const cursor_value,
				       int const fb_value,
				       int cmd_offset);
extern int evdi_cursor_composing_and_copy(struct evdi_cursor *cursor,
				   struct evdi_framebuffer *ufb,
				   char __user *buffer,
				   int buf_byte_stride,
				   int const max_x,
				   int const max_y);
#endif
