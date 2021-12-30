// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include "linux/thread_info.h"
#include "linux/mm.h"
#include <linux/version.h>
#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE
#include <drm/drm_file.h>
#include <drm/drm_vblank.h>
#include <drm/drm_ioctl.h>
#elif KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_edid.h>
#include "evdi_drm.h"
#include "evdi_drm_drv.h"
#include "evdi_cursor.h"
#include "evdi_params.h"
#include "evdi_i2c.h"
#include <linux/mutex.h>
#include <linux/compiler.h>
#include <linux/platform_device.h>
#include <linux/completion.h>

#include <linux/dma-buf.h>

#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE
MODULE_IMPORT_NS(DMA_BUF);
#endif

#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <drm/drm_probe_helper.h>
#endif

struct evdi_event_cursor_set_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_cursor_set cursor_set;
};

struct evdi_event_cursor_move_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_cursor_move cursor_move;
};

struct evdi_event_update_ready_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_update_ready update_ready;
};

struct evdi_event_dpms_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_dpms dpms;
};

struct evdi_event_mode_changed_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_mode_changed mode_changed;
};

struct evdi_event_crtc_state_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_crtc_state crtc_state;
};

struct evdi_event_ddcci_data_pending {
	struct drm_pending_event base;
	struct drm_evdi_event_ddcci_data ddcci_data;
};

#define MAX_DIRTS 16
#define EDID_EXT_BLOCK_SIZE 128
#define MAX_EDID_SIZE (255 * EDID_EXT_BLOCK_SIZE + sizeof(struct edid))
#define I2C_ADDRESS_DDCCI 0x37
#define DDCCI_TIMEOUT_MS 50

struct evdi_painter {
	bool is_connected;
	struct edid *edid;
	unsigned int edid_length;

	struct mutex lock;
	struct drm_clip_rect dirty_rects[MAX_DIRTS];
	int num_dirts;
	struct evdi_framebuffer *scanout_fb;

	struct drm_file *drm_filp;
	struct drm_device *drm_device;

	bool was_update_requested;
	bool needs_full_modeset;
	struct drm_crtc *crtc;
	struct drm_pending_vblank_event *vblank;

	struct list_head pending_events;
	struct delayed_work send_events_work;

	struct completion ddcci_response_received;
	char *ddcci_buffer;
	unsigned int ddcci_buffer_length;
};

static void expand_rect(struct drm_clip_rect *a, const struct drm_clip_rect *b)
{
	a->x1 = min(a->x1, b->x1);
	a->y1 = min(a->y1, b->y1);
	a->x2 = max(a->x2, b->x2);
	a->y2 = max(a->y2, b->y2);
}

static int rect_area(const struct drm_clip_rect *r)
{
	return (r->x2 - r->x1) * (r->y2 - r->y1);
}

static void merge_dirty_rects(struct drm_clip_rect *rects, int *count)
{
	int a, b;

	for (a = 0; a < *count - 1; ++a) {
		for (b = a + 1; b < *count;) {
			/* collapse to bounding rect if it is fewer pixels */
			const int area_a = rect_area(&rects[a]);
			const int area_b = rect_area(&rects[b]);
			struct drm_clip_rect bounding_rect = rects[a];

			expand_rect(&bounding_rect, &rects[b]);

			if (rect_area(&bounding_rect) <= area_a + area_b) {
				rects[a] = bounding_rect;
				rects[b] = rects[*count - 1];
				/* repass */
				b = a + 1;
				--*count;
			} else {
				++b;
			}
		}
	}
}

static void collapse_dirty_rects(struct drm_clip_rect *rects, int *count)
{
	int i;

	EVDI_VERBOSE("Not enough space for rects. They will be collapsed");

	for (i = 1; i < *count; ++i)
		expand_rect(&rects[0], &rects[i]);

	*count = 1;
}

static int copy_primary_pixels(struct evdi_framebuffer *efb,
			       char __user *buffer,
			       int buf_byte_stride,
			       int num_rects, struct drm_clip_rect *rects,
			       int const max_x,
			       int const max_y)
{
	struct drm_framebuffer *fb = &efb->base;
	struct drm_clip_rect *r;

	EVDI_CHECKPT();

	for (r = rects; r != rects + num_rects; ++r) {
		const int byte_offset = r->x1 * 4;
		const int byte_span = (r->x2 - r->x1) * 4;
		const int src_offset = fb->offsets[0] +
				       fb->pitches[0] * r->y1 + byte_offset;
		const char *src = (char *)efb->obj->vmapping + src_offset;
		const int dst_offset = buf_byte_stride * r->y1 + byte_offset;
		char __user *dst = buffer + dst_offset;
		int y = r->y2 - r->y1;

		/* rect size may correspond to previous resolution */
		if (max_x < r->x2 || max_y < r->y2) {
			EVDI_WARN("Rect size beyond expected dimensions\n");
			return -EFAULT;
		}

		EVDI_VERBOSE("copy rect %d,%d-%d,%d\n", r->x1, r->y1, r->x2,
			     r->y2);

		for (; y > 0; --y) {
			if (copy_to_user(dst, src, byte_span))
				return -EFAULT;

			src += fb->pitches[0];
			dst += buf_byte_stride;
		}
	}

	return 0;
}

static void copy_cursor_pixels(struct evdi_framebuffer *efb,
			       char __user *buffer,
			       int buf_byte_stride,
			       struct evdi_cursor *cursor)
{
	evdi_cursor_lock(cursor);
	if (evdi_cursor_compose_and_copy(cursor,
					 efb,
					 buffer,
					 buf_byte_stride))
		EVDI_ERROR("Failed to blend cursor\n");

	evdi_cursor_unlock(cursor);
}

#define painter_lock(painter)                           \
	do {                                            \
		EVDI_VERBOSE("Painter lock\n");         \
		mutex_lock(&painter->lock);             \
	} while (0)

#define painter_unlock(painter)                         \
	do {                                            \
		EVDI_VERBOSE("Painter unlock\n");       \
		mutex_unlock(&painter->lock);           \
	} while (0)

bool evdi_painter_is_connected(struct evdi_painter *painter)
{
	return painter ? painter->is_connected : false;
}

u8 *evdi_painter_get_edid_copy(struct evdi_device *evdi)
{
	u8 *block = NULL;

	EVDI_CHECKPT();

	painter_lock(evdi->painter);
	if (evdi_painter_is_connected(evdi->painter) &&
		evdi->painter->edid &&
		evdi->painter->edid_length) {
		block = kmalloc(evdi->painter->edid_length, GFP_KERNEL);
		if (block) {
			memcpy(block,
			       evdi->painter->edid,
			       evdi->painter->edid_length);
		}
	}
	painter_unlock(evdi->painter);
	return block;
}

static bool is_evdi_event_squashable(struct drm_pending_event *event)
{
	return event->event->type == DRM_EVDI_EVENT_CURSOR_SET ||
	       event->event->type == DRM_EVDI_EVENT_CURSOR_MOVE;
}

static void evdi_painter_add_event_to_pending_list(
	struct evdi_painter *painter,
	struct drm_pending_event *event)
{
	unsigned long flags;
	struct drm_pending_event *last_event = NULL;
	struct list_head *list = NULL;

	spin_lock_irqsave(&painter->drm_device->event_lock, flags);

	list = &painter->pending_events;
	if (!list_empty(list)) {
		last_event =
		  list_last_entry(list, struct drm_pending_event, link);
	}

	if (last_event &&
	    event->event->type == last_event->event->type &&
	    is_evdi_event_squashable(event)) {
		list_replace(&last_event->link, &event->link);
		kfree(last_event);
	} else
		list_add_tail(&event->link, list);

	spin_unlock_irqrestore(&painter->drm_device->event_lock, flags);
}

static bool evdi_painter_flush_pending_events(struct evdi_painter *painter)
{
	unsigned long flags;
	struct drm_pending_event *event_to_be_sent = NULL;
	struct list_head *list = NULL;
	bool has_space = false;
	bool flushed_all = false;

	spin_lock_irqsave(&painter->drm_device->event_lock, flags);

	list = &painter->pending_events;
	while ((event_to_be_sent = list_first_entry_or_null(
			list, struct drm_pending_event, link))) {
		has_space = drm_event_reserve_init_locked(painter->drm_device,
		    painter->drm_filp, event_to_be_sent,
		    event_to_be_sent->event) == 0;
		if (has_space) {
			list_del_init(&event_to_be_sent->link);
			drm_send_event_locked(painter->drm_device,
					      event_to_be_sent);
		} else
			break;
	}

	flushed_all = list_empty(&painter->pending_events);
	spin_unlock_irqrestore(&painter->drm_device->event_lock, flags);

	return flushed_all;
}

static void evdi_painter_send_event(struct evdi_painter *painter,
				    struct drm_pending_event *event)
{
	if (!event) {
		EVDI_ERROR("Null drm event!");
		return;
	}

	if (!painter->drm_filp) {
		EVDI_VERBOSE("Painter is not connected!");
		drm_event_cancel_free(painter->drm_device, event);
		return;
	}

	if (!painter->drm_device) {
		EVDI_WARN("Painter is not connected to drm device!");
		drm_event_cancel_free(painter->drm_device, event);
		return;
	}

	if (!painter->is_connected) {
		EVDI_WARN("Painter is not connected!");
		drm_event_cancel_free(painter->drm_device, event);
		return;
	}

	evdi_painter_add_event_to_pending_list(painter, event);
	if (delayed_work_pending(&painter->send_events_work))
		return;

	if (evdi_painter_flush_pending_events(painter))
		return;

	schedule_delayed_work(&painter->send_events_work, msecs_to_jiffies(5));
}

static struct drm_pending_event *create_update_ready_event(void)
{
	struct evdi_event_update_ready_pending *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		EVDI_ERROR("Failed to create update ready event");
		return NULL;
	}

	event->update_ready.base.type = DRM_EVDI_EVENT_UPDATE_READY;
	event->update_ready.base.length = sizeof(event->update_ready);
	event->base.event = &event->update_ready.base;
	return &event->base;
}

static void evdi_painter_send_update_ready(struct evdi_painter *painter)
{
	struct drm_pending_event *event = create_update_ready_event();

	evdi_painter_send_event(painter, event);
}

static uint32_t evdi_painter_get_gem_handle(struct evdi_painter *painter,
					   struct evdi_gem_object *obj)
{
	uint32_t handle = 0;

	if (!obj)
		return 0;

	handle = evdi_gem_object_handle_lookup(painter->drm_filp, &obj->base);

	if (handle)
		return handle;

	if (drm_gem_handle_create(painter->drm_filp,
			      &obj->base, &handle)) {
		EVDI_ERROR("Failed to create gem handle for %p\n",
			painter->drm_filp);
	}

	return handle;
}

static struct drm_pending_event *create_cursor_set_event(
		struct evdi_painter *painter,
		struct evdi_cursor *cursor)
{
	struct evdi_event_cursor_set_pending *event;
	struct evdi_gem_object *eobj = NULL;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		EVDI_ERROR("Failed to create cursor set event");
		return NULL;
	}

	event->cursor_set.base.type = DRM_EVDI_EVENT_CURSOR_SET;
	event->cursor_set.base.length = sizeof(event->cursor_set);

	evdi_cursor_lock(cursor);
	event->cursor_set.enabled = evdi_cursor_enabled(cursor);
	evdi_cursor_hotpoint(cursor, &event->cursor_set.hot_x,
				     &event->cursor_set.hot_y);
	evdi_cursor_size(cursor,
		&event->cursor_set.width,
		&event->cursor_set.height);
	evdi_cursor_format(cursor, &event->cursor_set.pixel_format);
	evdi_cursor_stride(cursor, &event->cursor_set.stride);
	eobj = evdi_cursor_gem(cursor);
	event->cursor_set.buffer_handle =
		evdi_painter_get_gem_handle(painter, eobj);
	if (eobj)
		event->cursor_set.buffer_length = eobj->base.size;
	if (!event->cursor_set.buffer_handle) {
		event->cursor_set.enabled = false;
		event->cursor_set.buffer_length = 0;
	}
	evdi_cursor_unlock(cursor);

	event->base.event = &event->cursor_set.base;
	return &event->base;
}

void evdi_painter_send_cursor_set(struct evdi_painter *painter,
				  struct evdi_cursor *cursor)
{
	struct drm_pending_event *event =
		create_cursor_set_event(painter, cursor);

	evdi_painter_send_event(painter, event);
}

static struct drm_pending_event *create_cursor_move_event(
		struct evdi_cursor *cursor)
{
	struct evdi_event_cursor_move_pending *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		EVDI_ERROR("Failed to create cursor move event");
		return NULL;
	}

	event->cursor_move.base.type = DRM_EVDI_EVENT_CURSOR_MOVE;
	event->cursor_move.base.length = sizeof(event->cursor_move);

	evdi_cursor_lock(cursor);
	evdi_cursor_position(
		cursor,
		&event->cursor_move.x,
		&event->cursor_move.y);
	evdi_cursor_unlock(cursor);

	event->base.event = &event->cursor_move.base;
	return &event->base;
}

void evdi_painter_send_cursor_move(struct evdi_painter *painter,
				   struct evdi_cursor *cursor)
{
	struct drm_pending_event *event = create_cursor_move_event(cursor);

	evdi_painter_send_event(painter, event);
}

static struct drm_pending_event *create_dpms_event(int mode)
{
	struct evdi_event_dpms_pending *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		EVDI_ERROR("Failed to create dpms event");
		return NULL;
	}

	event->dpms.base.type = DRM_EVDI_EVENT_DPMS;
	event->dpms.base.length = sizeof(event->dpms);
	event->dpms.mode = mode;
	event->base.event = &event->dpms.base;
	return &event->base;
}

static void evdi_painter_send_dpms(struct evdi_painter *painter, int mode)
{
	struct drm_pending_event *event = create_dpms_event(mode);

	evdi_painter_send_event(painter, event);
}

static struct drm_pending_event *create_mode_changed_event(
	struct drm_display_mode *current_mode,
	int32_t bits_per_pixel,
	uint32_t pixel_format)
{
	struct evdi_event_mode_changed_pending *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		EVDI_ERROR("Failed to create mode changed event");
		return NULL;
	}

	event->mode_changed.base.type = DRM_EVDI_EVENT_MODE_CHANGED;
	event->mode_changed.base.length = sizeof(event->mode_changed);

	event->mode_changed.hdisplay = current_mode->hdisplay;
	event->mode_changed.vdisplay = current_mode->vdisplay;
	event->mode_changed.vrefresh = drm_mode_vrefresh(current_mode);
	event->mode_changed.bits_per_pixel = bits_per_pixel;
	event->mode_changed.pixel_format = pixel_format;

	event->base.event = &event->mode_changed.base;
	return &event->base;
}

static void evdi_painter_send_mode_changed(
	struct evdi_painter *painter,
	struct drm_display_mode *current_mode,
	int32_t bits_per_pixel,
	uint32_t pixel_format)
{
	struct drm_pending_event *event = create_mode_changed_event(
		current_mode, bits_per_pixel, pixel_format);

	evdi_painter_send_event(painter, event);
}

int evdi_painter_get_num_dirts(struct evdi_painter *painter)
{
	int num_dirts;

	if (painter == NULL) {
		EVDI_WARN("Painter is not connected!");
		return 0;
	}

	painter_lock(painter);

	num_dirts = painter->num_dirts;

	painter_unlock(painter);

	return num_dirts;
}

struct drm_clip_rect evdi_painter_framebuffer_size(
	struct evdi_painter *painter)
{
	struct drm_clip_rect rect = {0, 0, 0, 0};
	struct evdi_framebuffer *efb = NULL;

	if (painter == NULL) {
		EVDI_WARN("Painter is not connected!");
		return rect;
	}

	painter_lock(painter);
	efb = painter->scanout_fb;
	if (!efb) {
		if (painter->is_connected)
			EVDI_DEBUG("Scanout buffer not set.");
		goto unlock;
	}
	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = efb->base.width;
	rect.y2 = efb->base.height;
unlock:
	painter_unlock(painter);
	return rect;
}

void evdi_painter_mark_dirty(struct evdi_device *evdi,
			     const struct drm_clip_rect *dirty_rect)
{
	struct drm_clip_rect rect;
	struct evdi_framebuffer *efb = NULL;
	struct evdi_painter *painter = evdi->painter;

	if (painter == NULL) {
		EVDI_WARN("Painter is not connected!");
		return;
	}

	painter_lock(painter);
	efb = painter->scanout_fb;
	if (!efb) {
		if (painter->is_connected)
			EVDI_DEBUG("(card%d) Skip clip rect. Scanout buffer not set.\n",
			   evdi->dev_index);
		goto unlock;
	}

	rect = evdi_framebuffer_sanitize_rect(efb, dirty_rect);

	EVDI_VERBOSE("(card%d) %d,%d-%d,%d\n", evdi->dev_index, rect.x1,
		     rect.y1, rect.x2, rect.y2);

	if (painter->num_dirts == MAX_DIRTS)
		merge_dirty_rects(&painter->dirty_rects[0],
				  &painter->num_dirts);

	if (painter->num_dirts == MAX_DIRTS)
		collapse_dirty_rects(&painter->dirty_rects[0],
				     &painter->num_dirts);

	memcpy(&painter->dirty_rects[painter->num_dirts], &rect, sizeof(rect));
	painter->num_dirts++;

unlock:
	painter_unlock(painter);
}

static void evdi_send_vblank(struct drm_crtc *crtc,
			     struct drm_pending_vblank_event *vblank)
{
	if (crtc && vblank) {
		unsigned long flags = 0;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, vblank);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void evdi_painter_send_vblank(struct evdi_painter *painter)
{
	EVDI_CHECKPT();

	evdi_send_vblank(painter->crtc, painter->vblank);

	painter->crtc = NULL;
	painter->vblank = NULL;
}

void evdi_painter_set_vblank(
	struct evdi_painter *painter,
	struct drm_crtc *crtc,
	struct drm_pending_vblank_event *vblank)
{
	EVDI_CHECKPT();

	if (painter) {
		painter_lock(painter);

		evdi_painter_send_vblank(painter);

		if (painter->num_dirts > 0 && painter->is_connected) {
			painter->crtc = crtc;
			painter->vblank = vblank;
		} else {
			evdi_send_vblank(crtc, vblank);
		}

		painter_unlock(painter);
	} else {
		evdi_send_vblank(crtc, vblank);
	}
}

void evdi_painter_send_update_ready_if_needed(struct evdi_painter *painter)
{
	EVDI_CHECKPT();
	if (painter) {
		painter_lock(painter);
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE || defined(EL8)
		if (painter->was_update_requested && painter->num_dirts) {
#else
		if (painter->was_update_requested) {
#endif
			evdi_painter_send_update_ready(painter);
			painter->was_update_requested = false;
		}

		painter_unlock(painter);
	} else {
		EVDI_WARN("Painter does not exist!");
	}
}

static const char * const dpms_str[] = { "on", "standby", "suspend", "off" };

void evdi_painter_dpms_notify(struct evdi_device *evdi, int mode)
{
	struct evdi_painter *painter = evdi->painter;
	const char *mode_str;

	if (!painter) {
		EVDI_WARN("(card%d) Painter does not exist!", evdi->dev_index);
		return;
	}

	if (!painter->is_connected)
		return;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		mode_str = dpms_str[mode];
		break;
	default:
		mode_str = "unknown";
	};
	EVDI_INFO("(card%d) Notifying display power state: %s",
		   evdi->dev_index, mode_str);
	evdi_painter_send_dpms(painter, mode);
}

static void evdi_log_pixel_format(uint32_t pixel_format,
		char *buf, size_t size)
{
#if KERNEL_VERSION(5, 14, 0) <= LINUX_VERSION_CODE
	snprintf(buf, size, "pixel format %p4cc", &pixel_format);
#else
	struct drm_format_name_buf format_name;

	drm_get_format_name(pixel_format, &format_name);
	snprintf(buf, size, "pixel format %s", format_name.str);
#endif
}

void evdi_painter_mode_changed_notify(struct evdi_device *evdi,
				      struct drm_display_mode *new_mode)
{
	struct evdi_painter *painter = evdi->painter;
	struct drm_framebuffer *fb;
	int bits_per_pixel;
	uint32_t pixel_format;
	char buf[100];

	if (painter == NULL)
		return;

	fb = &painter->scanout_fb->base;
	if (fb == NULL)
		return;

	bits_per_pixel = fb->format->cpp[0] * 8;
	pixel_format = fb->format->format;


	evdi_log_pixel_format(pixel_format, buf, sizeof(buf));
	EVDI_INFO("(card%d) Notifying mode changed: %dx%d@%d; bpp %d; %s",
		   evdi->dev_index, new_mode->hdisplay, new_mode->vdisplay,
		   drm_mode_vrefresh(new_mode), bits_per_pixel, buf);

	evdi_painter_send_mode_changed(painter,
				       new_mode,
				       bits_per_pixel,
				       pixel_format);
	painter->needs_full_modeset = false;
}

static void evdi_painter_events_cleanup(struct evdi_painter *painter)
{
	struct drm_pending_event *event, *temp;
	unsigned long flags;

	spin_lock_irqsave(&painter->drm_device->event_lock, flags);
	list_for_each_entry_safe(event, temp, &painter->pending_events, link) {
		list_del(&event->link);
		kfree(event);
	}
	spin_unlock_irqrestore(&painter->drm_device->event_lock, flags);

	cancel_delayed_work_sync(&painter->send_events_work);
}

static void evdi_add_i2c_adapter(struct evdi_device *evdi)
{
	struct drm_device *ddev = evdi->ddev;
	struct platform_device *platdev = to_platform_device(ddev->dev);
	int result = 0;

	evdi->i2c_adapter = kzalloc(sizeof(*evdi->i2c_adapter), GFP_KERNEL);

	if (!evdi->i2c_adapter) {
		EVDI_ERROR("(card%d) Failed to allocate for i2c adapter",
			evdi->dev_index);
		return;
	}

	result = evdi_i2c_add(evdi->i2c_adapter, &platdev->dev, ddev->dev_private);

	if (result) {
		kfree(evdi->i2c_adapter);
		evdi->i2c_adapter = NULL;
		EVDI_ERROR("(card%d) Failed to add i2c adapter, error %d",
			evdi->dev_index, result);
		return;
	}

	EVDI_INFO("(card%d) Added i2c adapter bus number %d",
		evdi->dev_index, evdi->i2c_adapter->nr);

	result = sysfs_create_link(&evdi->conn->kdev->kobj,
			&evdi->i2c_adapter->dev.kobj, "ddc");

	if (result) {
		EVDI_ERROR("(card%d) Failed to create sysfs link, error %d",
			evdi->dev_index, result);
		return;
	}
}

static void evdi_remove_i2c_adapter(struct evdi_device *evdi)
{
	if (evdi->i2c_adapter) {
		EVDI_INFO("(card%d) Removing i2c adapter bus number %d",
			evdi->dev_index, evdi->i2c_adapter->nr);

		sysfs_remove_link(&evdi->conn->kdev->kobj, "ddc");

		evdi_i2c_remove(evdi->i2c_adapter);

		kfree(evdi->i2c_adapter);
		evdi->i2c_adapter = NULL;
	}
}

static int
evdi_painter_connect(struct evdi_device *evdi,
		     void const __user *edid_data, unsigned int edid_length,
		     uint32_t sku_area_limit,
		     struct drm_file *file, __always_unused int dev_index)
{
	struct evdi_painter *painter = evdi->painter;
	struct edid *new_edid = NULL;
	unsigned int expected_edid_size = 0;
	char buf[100];

	evdi_log_process(buf, sizeof(buf));

	if (edid_length < sizeof(struct edid)) {
		EVDI_ERROR("Edid length too small\n");
		return -EINVAL;
	}

	if (edid_length > MAX_EDID_SIZE) {
		EVDI_ERROR("Edid length too large\n");
		return -EINVAL;
	}

	new_edid = kzalloc(edid_length, GFP_KERNEL);
	if (!new_edid)
		return -ENOMEM;

	if (copy_from_user(new_edid, edid_data, edid_length)) {
		EVDI_ERROR("(card%d) Failed to read edid\n", evdi->dev_index);
		kfree(new_edid);
		return -EFAULT;
	}

	expected_edid_size = sizeof(struct edid) +
			     new_edid->extensions * EDID_EXT_BLOCK_SIZE;
	if (expected_edid_size != edid_length) {
		EVDI_ERROR("Wrong edid size. Expected %d but is %d\n",
			   expected_edid_size, edid_length);
		kfree(new_edid);
		return -EINVAL;
	}

	if (painter->drm_filp)
		EVDI_WARN("(card%d) Double connect - replacing %p with %p\n",
			  evdi->dev_index, painter->drm_filp, file);

	painter_lock(painter);

	evdi->sku_area_limit = sku_area_limit;
	painter->drm_filp = file;
	kfree(painter->edid);
	painter->edid_length = edid_length;
	painter->edid = new_edid;
	painter->is_connected = true;
	painter->needs_full_modeset = true;

	if (!evdi->i2c_adapter)
		evdi_add_i2c_adapter(evdi);

	painter_unlock(painter);

	EVDI_INFO("(card%d) Connected with %s\n", evdi->dev_index, buf);

	drm_helper_hpd_irq_event(evdi->ddev);

	return 0;
}

static int evdi_painter_disconnect(struct evdi_device *evdi,
	struct drm_file *file)
{
	struct evdi_painter *painter = evdi->painter;
	char buf[100];

	EVDI_CHECKPT();

	painter_lock(painter);

	if (file != painter->drm_filp) {
		painter_unlock(painter);
		return -EFAULT;
	}

	if (painter->scanout_fb) {
		drm_framebuffer_put(&painter->scanout_fb->base);
		painter->scanout_fb = NULL;
	}

	painter->is_connected = false;

	evdi_log_process(buf, sizeof(buf));
	EVDI_INFO("(card%d) Disconnected from %s\n", evdi->dev_index, buf);
	evdi_painter_events_cleanup(painter);

	evdi_painter_send_vblank(painter);

	evdi_cursor_enable(evdi->cursor, false);

	kfree(painter->ddcci_buffer);
	painter->ddcci_buffer = NULL;
	painter->ddcci_buffer_length = 0;

	evdi_remove_i2c_adapter(evdi);

	painter->drm_filp = NULL;

	painter->was_update_requested = false;
	evdi->cursor_events_enabled = false;

	painter_unlock(painter);

	// Signal anything waiting for ddc/ci response with NULL buffer
	complete(&painter->ddcci_response_received);

	drm_helper_hpd_irq_event(evdi->ddev);
	return 0;
}

void evdi_painter_close(struct evdi_device *evdi, struct drm_file *file)
{
	EVDI_CHECKPT();

	if (evdi->painter && file == evdi->painter->drm_filp)
		evdi_painter_disconnect(evdi, file);
}

int evdi_painter_connect_ioctl(struct drm_device *drm_dev, void *data,
			       struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	struct drm_evdi_connect *cmd = data;
	int ret;

	EVDI_CHECKPT();
	if (painter) {
		if (cmd->connected)
			ret = evdi_painter_connect(evdi,
					     cmd->edid,
					     cmd->edid_length,
					     cmd->sku_area_limit,
					     file,
					     cmd->dev_index);
		else
			ret = evdi_painter_disconnect(evdi, file);

		if (ret) {
			EVDI_WARN("(card%d)(pid=%d) disconnect failed\n",
				  evdi->dev_index, (int)task_pid_nr(current));
		}
		return ret;
	}
	EVDI_WARN("(card%d) Painter does not exist!", evdi->dev_index);
	return -ENODEV;
}

int evdi_painter_grabpix_ioctl(struct drm_device *drm_dev, void *data,
			       __always_unused struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	struct drm_evdi_grabpix *cmd = data;
	struct evdi_framebuffer *efb = NULL;
	struct drm_clip_rect dirty_rects[MAX_DIRTS];
	struct drm_crtc *crtc = NULL;
	struct drm_pending_vblank_event *vblank = NULL;
	int err;
	int ret;
	struct dma_buf_attachment *import_attach;

	EVDI_CHECKPT();

	if (cmd->mode != EVDI_GRABPIX_MODE_DIRTY) {
		EVDI_ERROR("Unknown command mode\n");
		return -EINVAL;
	}

	if (cmd->num_rects < 1) {
		EVDI_ERROR("No space for clip rects\n");
		return -EINVAL;
	}

	if (!painter)
		return -ENODEV;

	painter_lock(painter);

	if (painter->was_update_requested) {
		EVDI_WARN("(card%d) Update ready not sent,",
			  evdi->dev_index);
		EVDI_WARN(" but pixels are grabbed.\n");
	}

	if (painter->num_dirts < 0) {
		err = -EAGAIN;
		goto err_painter;
	}

	merge_dirty_rects(&painter->dirty_rects[0],
			  &painter->num_dirts);
	if (painter->num_dirts > cmd->num_rects)
		collapse_dirty_rects(&painter->dirty_rects[0],
				     &painter->num_dirts);

	cmd->num_rects = painter->num_dirts;
	memcpy(dirty_rects, painter->dirty_rects,
	       painter->num_dirts * sizeof(painter->dirty_rects[0]));

	efb = painter->scanout_fb;

	if (!efb) {
		EVDI_ERROR("Scanout buffer not set\n");
		err = -EAGAIN;
		goto err_painter;
	}

	painter->num_dirts = 0;

	drm_framebuffer_get(&efb->base);

	crtc = painter->crtc;
	painter->crtc = NULL;

	vblank = painter->vblank;
	painter->vblank = NULL;


	painter_unlock(painter);

	if (!efb->obj->vmapping) {
		if (evdi_gem_vmap(efb->obj) == -ENOMEM) {
			EVDI_ERROR("Failed to map scanout buffer\n");
			err = -EFAULT;
			goto err_fb;
		}
		if (!efb->obj->vmapping) {
			EVDI_ERROR("Inexistent vmapping\n");
			err = -EFAULT;
			goto err_fb;
		}
	}

	if ((unsigned int)cmd->buf_width != efb->base.width ||
		(unsigned int)cmd->buf_height != efb->base.height) {
		EVDI_ERROR("Invalid buffer dimension\n");
		err = -EINVAL;
		goto err_fb;
	}

	if (copy_to_user(cmd->rects, dirty_rects,
		cmd->num_rects * sizeof(cmd->rects[0]))) {
		err = -EFAULT;
		goto err_fb;
	}

	import_attach = efb->obj->base.import_attach;
	if (import_attach) {
		ret = dma_buf_begin_cpu_access(import_attach->dmabuf,
					       DMA_FROM_DEVICE);
		if (ret) {
			err = -EFAULT;
			goto err_fb;
		}
	}

	err = copy_primary_pixels(efb,
				  cmd->buffer,
				  cmd->buf_byte_stride,
				  cmd->num_rects,
				  dirty_rects,
				  cmd->buf_width,
				  cmd->buf_height);
	if (err == 0 && !evdi->cursor_events_enabled)
		copy_cursor_pixels(efb,
				   cmd->buffer,
				   cmd->buf_byte_stride,
				   evdi->cursor);

	if (import_attach)
		dma_buf_end_cpu_access(import_attach->dmabuf,
				       DMA_FROM_DEVICE);

err_fb:
	evdi_send_vblank(crtc, vblank);

	drm_framebuffer_put(&efb->base);

	return err;

err_painter:
	painter_unlock(painter);
	return err;
}

int evdi_painter_request_update_ioctl(struct drm_device *drm_dev,
				      __always_unused void *data,
				      __always_unused struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	int result = 0;

	if (painter) {
		painter_lock(painter);

		if (painter->was_update_requested) {
			EVDI_WARN
			  ("(card%d) Update was already requested - ignoring\n",
			   evdi->dev_index);
		} else {
			if (painter->num_dirts > 0)
				result = 1;
			else
				painter->was_update_requested = true;
		}

		painter_unlock(painter);

		return result;
	} else {
		return -ENODEV;
	}
}

static void evdi_send_events_work(struct work_struct *work)
{
	struct evdi_painter *painter =
		container_of(work, struct evdi_painter,	send_events_work.work);

	if (evdi_painter_flush_pending_events(painter))
		return;

	schedule_delayed_work(&painter->send_events_work, msecs_to_jiffies(5));
}

int evdi_painter_init(struct evdi_device *dev)
{
	EVDI_CHECKPT();
	dev->painter = kzalloc(sizeof(*dev->painter), GFP_KERNEL);
	if (dev->painter) {
		mutex_init(&dev->painter->lock);
		dev->painter->edid = NULL;
		dev->painter->edid_length = 0;
		dev->painter->needs_full_modeset = true;
		dev->painter->crtc = NULL;
		dev->painter->vblank = NULL;
		dev->painter->drm_device = dev->ddev;
		INIT_LIST_HEAD(&dev->painter->pending_events);
		INIT_DELAYED_WORK(&dev->painter->send_events_work,
			evdi_send_events_work);
		init_completion(&dev->painter->ddcci_response_received);
		return 0;
	}
	return -ENOMEM;
}

void evdi_painter_cleanup(struct evdi_painter *painter)
{
	EVDI_CHECKPT();
	if (!painter) {
		EVDI_WARN("Painter does not exist\n");
		return;
	}

	painter_lock(painter);
	kfree(painter->edid);
	painter->edid_length = 0;
	painter->edid = NULL;

	evdi_painter_send_vblank(painter);

	evdi_painter_events_cleanup(painter);

	painter->drm_device = NULL;
	painter_unlock(painter);
}

void evdi_painter_set_scanout_buffer(struct evdi_painter *painter,
				     struct evdi_framebuffer *newfb)
{
	struct evdi_framebuffer *oldfb = NULL;

	if (newfb)
		drm_framebuffer_get(&newfb->base);

	painter_lock(painter);

	oldfb = painter->scanout_fb;
	painter->scanout_fb = newfb;

	painter_unlock(painter);

	if (oldfb)
		drm_framebuffer_put(&oldfb->base);
}

bool evdi_painter_needs_full_modeset(struct evdi_painter *painter)
{
	return painter ? painter->needs_full_modeset : false;
}


void evdi_painter_force_full_modeset(struct evdi_painter *painter)
{
	if (painter)
		painter->needs_full_modeset = true;
}

static struct drm_pending_event *create_ddcci_data_event(struct i2c_msg *msg)
{
	struct evdi_event_ddcci_data_pending *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event || !msg) {
		EVDI_ERROR("Failed to create ddcci data event");
		return NULL;
	}

	event->ddcci_data.base.type = DRM_EVDI_EVENT_DDCCI_DATA;
	event->ddcci_data.base.length = sizeof(event->ddcci_data);
	// Truncate buffers to a maximum of 64 bytes
	event->ddcci_data.buffer_length = min_t(__u16, msg->len,
		sizeof(event->ddcci_data.buffer));
	memcpy(event->ddcci_data.buffer, msg->buf,
		event->ddcci_data.buffer_length);
	event->ddcci_data.flags = msg->flags;
	event->ddcci_data.address = msg->addr;

	event->base.event = &event->ddcci_data.base;
	return &event->base;
}

static void evdi_painter_ddcci_data(struct evdi_painter *painter, struct i2c_msg *msg)
{
	struct drm_pending_event *event = create_ddcci_data_event(msg);

	reinit_completion(&painter->ddcci_response_received);
	evdi_painter_send_event(painter, event);

	if (wait_for_completion_interruptible_timeout(
		&painter->ddcci_response_received,
		msecs_to_jiffies(DDCCI_TIMEOUT_MS)) > 0) {

		// Match expected buffer length including any truncation
		const uint32_t expected_response_length = min_t(__u16, msg->len,
								DDCCI_BUFFER_SIZE);

		painter_lock(painter);

		if (expected_response_length != painter->ddcci_buffer_length)
			EVDI_WARN("DDCCI buffer length mismatch");
		else if (painter->ddcci_buffer)
			memcpy(msg->buf, painter->ddcci_buffer,
			       painter->ddcci_buffer_length);
		else
			EVDI_WARN("Ignoring NULL DDCCI buffer");

		painter_unlock(painter);
	} else {
		EVDI_WARN("DDCCI response timeout");
	}
}

bool evdi_painter_i2c_data_notify(struct evdi_painter *painter, struct i2c_msg *msg)
{
	if (!evdi_painter_is_connected(painter)) {
		EVDI_WARN("Painter not connected");
		return false;
	}

	if (!msg) {
		EVDI_WARN("Ignored NULL ddc/ci message");
		return false;
	}

	if (msg->addr != I2C_ADDRESS_DDCCI) {
		EVDI_DEBUG("Ignored ddc/ci data for address 0x%x\n", msg->addr);
		return false;
	}

	evdi_painter_ddcci_data(painter, msg);
	return true;
}

int evdi_painter_ddcci_response_ioctl(struct drm_device *drm_dev, void *data,
				__always_unused struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	struct drm_evdi_ddcci_response *cmd = data;
	int result = 0;

	painter_lock(painter);

	// Truncate any read to 64 bytes
	painter->ddcci_buffer_length = min_t(uint32_t, cmd->buffer_length,
					     DDCCI_BUFFER_SIZE);

	kfree(painter->ddcci_buffer);
	painter->ddcci_buffer = kzalloc(painter->ddcci_buffer_length, GFP_KERNEL);
	if (!painter->ddcci_buffer) {
		EVDI_ERROR("DDC buffer allocation failed\n");
		result = -ENOMEM;
		goto unlock;
	}

	if (copy_from_user(painter->ddcci_buffer, cmd->buffer,
		painter->ddcci_buffer_length)) {
		EVDI_ERROR("Failed to read ddcci_buffer\n");
		kfree(painter->ddcci_buffer);
		painter->ddcci_buffer = NULL;
		result = -EFAULT;
		goto unlock;
	}

	complete(&painter->ddcci_response_received);

unlock:
	painter_unlock(painter);
	return result;
}

int evdi_painter_enable_cursor_events_ioctl(struct drm_device *drm_dev, void *data,
					__always_unused struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct drm_evdi_enable_cursor_events *cmd = data;

	evdi->cursor_events_enabled = cmd->enable;

	return 0;
}
