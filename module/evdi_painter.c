/*
 * Copyright (c) 2013 - 2017 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include "evdi_drm.h"
#include "evdi_drv.h"
#include "evdi_cursor.h"
#include <linux/mutex.h>
#include <linux/compiler.h>
#include <linux/list.h>

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

#define MAX_DIRTS 16
#define EDID_EXT_BLOCK_SIZE 128
#define MAX_EDID_SIZE (255 * EDID_EXT_BLOCK_SIZE + sizeof(struct edid))

struct evdi_painter {
	bool is_connected;
	struct edid *edid;
	unsigned int edid_length;

	struct mutex lock;
	struct drm_clip_rect dirty_rects[MAX_DIRTS];
	int num_dirts;
	struct evdi_framebuffer *new_scanout_fb;
	struct evdi_framebuffer *scanout_fb;

	struct drm_file *drm_filp;

	bool was_update_requested;
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

	EVDI_CHECKPT();
	EVDI_WARN("Not enough space for clip rects! Rects will be collapsed");

	for (i = 1; i < *count; ++i)
		expand_rect(&rects[0], &rects[i]);

	*count = 1;
}

static int copy_pixels(struct evdi_framebuffer *ufb,
			char __user *buffer,
			int buf_byte_stride,
			int num_rects, struct drm_clip_rect *rects,
			int const max_x,
			int const max_y,
			struct evdi_cursor *cursor_copy)
{
	struct drm_framebuffer *fb = &ufb->base;
	struct drm_clip_rect *r;

	EVDI_CHECKPT();

	for (r = rects; r != rects + num_rects; ++r) {
		const int byte_offset = r->x1 * 4;
		const int byte_span = (r->x2 - r->x1) * 4;
		const int src_offset = fb->pitches[0] * r->y1 + byte_offset;
		const char *src = (char *)ufb->obj->vmapping + src_offset;
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

	return evdi_cursor_composing_and_copy(cursor_copy,
				       ufb,
				       buffer,
				       buf_byte_stride,
				       max_x, max_y);
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

bool evdi_painter_is_connected(struct evdi_device *evdi)
{
	if (evdi && evdi->painter)
		return evdi->painter->is_connected;
	return false;
}

u8 *evdi_painter_get_edid_copy(struct evdi_device *evdi)
{
	u8 *block = NULL;

	EVDI_CHECKPT();

	painter_lock(evdi->painter);
	if (evdi_painter_is_connected(evdi) &&
		evdi->painter->edid &&
		evdi->painter->edid_length) {
		block = kmalloc(evdi->painter->edid_length, GFP_KERNEL);
		if (block) {
			memcpy(block,
			       evdi->painter->edid,
			       evdi->painter->edid_length);
			EVDI_DEBUG("(dev=%d) %02x %02x %02x\n", evdi->dev_index,
				   block[0], block[1], block[2]);
		}
	}
	painter_unlock(evdi->painter);
	return block;
}

static void evdi_painter_send_event(struct drm_file *drm_filp,
				    struct list_head *event_link)
{
	list_add_tail(event_link, &drm_filp->event_list);
	wake_up_interruptible(&drm_filp->event_wait);
}

static void evdi_painter_send_update_ready(struct evdi_painter *painter)
{
	struct evdi_event_update_ready_pending *event;

	if (painter->drm_filp) {
		event = kzalloc(sizeof(*event), GFP_KERNEL);
		event->update_ready.base.type = DRM_EVDI_EVENT_UPDATE_READY;
		event->update_ready.base.length = sizeof(event->update_ready);
		event->base.event = &event->update_ready.base;
		event->base.file_priv = painter->drm_filp;
#if KERNEL_VERSION(4, 8, 0) > LINUX_VERSION_CODE
		event->base.destroy =
		    (void (*)(struct drm_pending_event *))kfree;
#endif

		evdi_painter_send_event(painter->drm_filp, &event->base.link);
	} else {
		EVDI_WARN("Painter is not connected!");
	}
}

static void evdi_painter_send_dpms(struct evdi_painter *painter, int mode)
{
	struct evdi_event_dpms_pending *event;

	if (painter->drm_filp) {
		event = kzalloc(sizeof(*event), GFP_KERNEL);
		event->dpms.base.type = DRM_EVDI_EVENT_DPMS;
		event->dpms.base.length = sizeof(event->dpms);
		event->dpms.mode = mode;
		event->base.event = &event->dpms.base;
		event->base.file_priv = painter->drm_filp;
#if KERNEL_VERSION(4, 8, 0) > LINUX_VERSION_CODE
		event->base.destroy =
		    (void (*)(struct drm_pending_event *))kfree;
#endif
		evdi_painter_send_event(painter->drm_filp, &event->base.link);
	} else {
		EVDI_WARN("Painter is not connected!");
	}
}

static void evdi_painter_send_crtc_state(struct evdi_painter *painter,
					 int state)
{
	struct evdi_event_crtc_state_pending *event;

	if (painter->drm_filp) {
		event = kzalloc(sizeof(*event), GFP_KERNEL);
		event->crtc_state.base.type = DRM_EVDI_EVENT_CRTC_STATE;
		event->crtc_state.base.length = sizeof(event->crtc_state);
		event->crtc_state.state = state;
		event->base.event = &event->crtc_state.base;
		event->base.file_priv = painter->drm_filp;
#if KERNEL_VERSION(4, 8, 0) > LINUX_VERSION_CODE
		event->base.destroy =
		    (void (*)(struct drm_pending_event *))kfree;
#endif
		evdi_painter_send_event(painter->drm_filp, &event->base.link);
	} else {
		 EVDI_WARN("Painter is not connected!");
	}
}

static void evdi_painter_send_mode_changed(
	struct evdi_painter *painter,
	struct drm_display_mode *current_mode,
	int32_t bits_per_pixel,
	uint32_t pixel_format)
{
	struct evdi_event_mode_changed_pending *event;

	if (painter->drm_filp) {
		event = kzalloc(sizeof(*event), GFP_KERNEL);
		event->mode_changed.base.type = DRM_EVDI_EVENT_MODE_CHANGED;
		event->mode_changed.base.length = sizeof(event->mode_changed);

		event->mode_changed.hdisplay = current_mode->hdisplay;
		event->mode_changed.vdisplay = current_mode->vdisplay;
		event->mode_changed.vrefresh =
			drm_mode_vrefresh(current_mode);
		event->mode_changed.bits_per_pixel = bits_per_pixel;
		event->mode_changed.pixel_format = pixel_format;

		event->base.event = &event->mode_changed.base;
		event->base.file_priv = painter->drm_filp;
#if KERNEL_VERSION(4, 8, 0) > LINUX_VERSION_CODE
		event->base.destroy =
		    (void (*)(struct drm_pending_event *))kfree;
#endif
		evdi_painter_send_event(painter->drm_filp, &event->base.link);
	} else {
		 EVDI_WARN("Painter is not connected!");
	}
}

void evdi_painter_mark_dirty(struct evdi_device *evdi,
			     const struct drm_clip_rect *dirty_rect)
{
	struct drm_clip_rect rect;
	struct evdi_framebuffer *efb = NULL;
	struct evdi_painter *painter = evdi->painter;

	painter_lock(evdi->painter);
	efb = evdi->painter->scanout_fb;
	if (!efb) {
		EVDI_WARN("Skip clip rect. Scanout buffer not set.\n");
		goto unlock;
	}

	rect = evdi_framebuffer_sanitize_rect(efb, dirty_rect);

	EVDI_VERBOSE("(dev=%d) %d,%d-%d,%d\n", evdi->dev_index, rect.x1,
		     rect.y1, rect.x2, rect.y2);

	if (painter->num_dirts == MAX_DIRTS)
		merge_dirty_rects(&painter->dirty_rects[0],
				  &painter->num_dirts);

	if (painter->num_dirts == MAX_DIRTS)
		collapse_dirty_rects(&painter->dirty_rects[0],
				     &painter->num_dirts);

	memcpy(&painter->dirty_rects[painter->num_dirts], &rect, sizeof(rect));
	painter->num_dirts++;

	if (painter->was_update_requested) {
		evdi_painter_send_update_ready(painter);
		painter->was_update_requested = false;
	}

unlock:
	painter_unlock(evdi->painter);
}

void evdi_painter_dpms_notify(struct evdi_device *evdi, int mode)
{
	struct evdi_painter *painter = evdi->painter;

	if (painter) {
		EVDI_DEBUG("(dev=%d) Notifying dpms mode: %d\n",
			   evdi->dev_index, mode);
		evdi_painter_send_dpms(painter, mode);
	} else {
		EVDI_WARN("Painter does not exist!");
	}
}

void evdi_painter_crtc_state_notify(struct evdi_device *evdi, int state)
{
	struct evdi_painter *painter = evdi->painter;

	if (painter) {
		EVDI_DEBUG("(dev=%d) Notifying crtc state: %d\n",
			   evdi->dev_index, state);
		evdi_painter_send_crtc_state(painter, state);
	} else {
		EVDI_WARN("Painter does not exist!");
	}
}

void evdi_painter_mode_changed_notify(struct evdi_device *evdi,
				      struct drm_framebuffer *fb,
				      struct drm_display_mode *new_mode)
{
	struct evdi_painter *painter = evdi->painter;

	EVDI_DEBUG(
		"(dev=%d) Notifying mode changed: %dx%d@%d; bpp %d; ",
		evdi->dev_index, new_mode->hdisplay, new_mode->vdisplay,
		drm_mode_vrefresh(new_mode), fb->bits_per_pixel);
	EVDI_DEBUG("pixel format %d\n", fb->pixel_format);

	evdi_painter_send_mode_changed(painter,
				       new_mode,
				       fb->bits_per_pixel,
				       fb->pixel_format);
}

int
evdi_painter_connect(struct evdi_device *evdi,
		     void const __user *edid_data, unsigned int edid_length,
		     uint32_t sku_area_limit,
		     struct drm_file *file, int dev_index)
{
	struct evdi_painter *painter = evdi->painter;
	struct edid *new_edid = NULL;
	int expected_edid_size = 0;

	EVDI_CHECKPT();

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
		EVDI_ERROR("(dev=%d) Failed to read edid\n", dev_index);
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
		EVDI_WARN("(dev=%d) Double connect - replacing %p with %p\n",
			  dev_index, painter->drm_filp, file);

	painter_lock(painter);

	evdi->dev_index = dev_index;
	evdi->sku_area_limit = sku_area_limit;
	painter->drm_filp = file;
	kfree(painter->edid);
	painter->edid_length = edid_length;
	painter->edid = new_edid;
	painter->is_connected = true;

	painter_unlock(painter);

	EVDI_DEBUG("(dev=%d) Connected with %p\n", evdi->dev_index,
			painter->drm_filp);

	drm_helper_hpd_irq_event(evdi->ddev);
	drm_helper_resume_force_mode(evdi->ddev);

	return 0;
}

void evdi_painter_disconnect(struct evdi_device *evdi, struct drm_file *file)
{
	struct evdi_painter *painter = evdi->painter;

	EVDI_CHECKPT();

	painter_lock(painter);

	if (file != painter->drm_filp) {
		EVDI_WARN
		    ("(dev=%d) An unknown connection to %p tries to close us",
		     evdi->dev_index, file);
		EVDI_WARN(" - ignoring\n");


		painter_unlock(painter);
		return;
	}

	if (painter->new_scanout_fb) {
		drm_framebuffer_unreference(&painter->new_scanout_fb->base);
		painter->new_scanout_fb = NULL;
	}

	if (painter->scanout_fb) {
		drm_framebuffer_unreference(&painter->scanout_fb->base);
		painter->scanout_fb = NULL;
	}

	painter->is_connected = false;

	EVDI_DEBUG("(dev=%d) Disconnected from %p\n", evdi->dev_index,
		   painter->drm_filp);
	painter->drm_filp = NULL;
	evdi->dev_index = -1;

	painter->was_update_requested = false;

	painter_unlock(painter);

	drm_helper_hpd_irq_event(evdi->ddev);
}

void evdi_painter_close(struct evdi_device *evdi, struct drm_file *file)
{
	EVDI_CHECKPT();

	if (evdi->painter)
		evdi_painter_disconnect(evdi, file);
	else
		EVDI_WARN("Painter does not exist!");
}

int evdi_painter_connect_ioctl(struct drm_device *drm_dev, void *data,
			       struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	struct drm_evdi_connect *cmd = data;

	EVDI_CHECKPT();
	if (painter) {
		if (cmd->connected)
			evdi_painter_connect(evdi,
					     cmd->edid,
					     cmd->edid_length,
					     cmd->sku_area_limit,
					     file,
					     cmd->dev_index);
		else
			evdi_painter_disconnect(evdi, file);

		return 0;
	}
	EVDI_WARN("Painter does not exist!");
	return -ENODEV;
}

int evdi_painter_grabpix_ioctl(struct drm_device *drm_dev, void *data,
			       __always_unused struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;
	struct evdi_painter *painter = evdi->painter;
	struct drm_evdi_grabpix *cmd = data;
	struct drm_framebuffer *fb = NULL;
	struct evdi_framebuffer *efb = NULL;
	struct evdi_cursor *cursor_copy = NULL;
	int err = 0;

	EVDI_CHECKPT();

	if (!painter)
		return -ENODEV;

	mutex_lock(&drm_dev->struct_mutex);
	if (evdi_cursor_alloc(&cursor_copy) == 0)
		evdi_cursor_copy(cursor_copy, evdi->cursor);
	mutex_unlock(&drm_dev->struct_mutex);

	painter_lock(painter);

	efb = painter->scanout_fb;

	if (!efb) {
		EVDI_ERROR("Scanout buffer not set\n");
		err = -EAGAIN;
		goto unlock;
	}

	if (painter->was_update_requested) {
		EVDI_WARN("(dev=%d) Update ready not sent,",
			  evdi->dev_index);
		EVDI_WARN(" but pixels are grabbed.\n");
	}

	fb = &efb->base;
	if (!efb->obj->vmapping) {
		if (evdi_gem_vmap(efb->obj) == -ENOMEM) {
			EVDI_ERROR("Failed to map scanout buffer\n");
			err = -EFAULT;
			goto unlock;
		}
		if (!efb->obj->vmapping) {
			EVDI_ERROR("Failed to map scanout buffer\n");
			err = -EFAULT;
			goto unlock;
		}
	}

	if (cmd->buf_width != fb->width ||
		cmd->buf_height != fb->height) {
		EVDI_ERROR("Invalid buffer dimension\n");
		err = -EINVAL;
		goto unlock;
	}

	if (cmd->num_rects < 1) {
		EVDI_ERROR("No space for clip rects\n");
		err = -EINVAL;
		goto unlock;
	}

	if (cmd->mode == EVDI_GRABPIX_MODE_DIRTY) {
		if (painter->num_dirts < 0) {
			err = -EAGAIN;
			goto unlock;
		}
		merge_dirty_rects(&painter->dirty_rects[0],
				  &painter->num_dirts);
		if (painter->num_dirts > cmd->num_rects)
			collapse_dirty_rects(&painter->dirty_rects[0],
						 &painter->num_dirts);

		cmd->num_rects = painter->num_dirts;

		if (copy_to_user(cmd->rects, painter->dirty_rects,
			cmd->num_rects * sizeof(cmd->rects[0])))
			err = -EFAULT;
		else
			err = copy_pixels(efb,
					  cmd->buffer,
					  cmd->buf_byte_stride,
					  painter->num_dirts,
					  painter->dirty_rects,
					  cmd->buf_width,
					  cmd->buf_height,
					  cursor_copy);

		painter->num_dirts = 0;
	}
unlock:
	painter_unlock(painter);
	if (cursor_copy)
		evdi_cursor_free(cursor_copy);

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
			  ("(dev=%d) Update was already requested - ignoring\n",
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

int evdi_painter_init(struct evdi_device *dev)
{
	EVDI_CHECKPT();
	dev->painter = kzalloc(sizeof(*dev->painter), GFP_KERNEL);
	if (dev->painter) {
		mutex_init(&dev->painter->lock);
		dev->painter->edid = NULL;
		dev->painter->edid_length = 0;
		return 0;
	}
	return -ENOMEM;
}

void evdi_painter_cleanup(struct evdi_device *evdi)
{
	struct evdi_painter *painter = evdi->painter;

	EVDI_CHECKPT();
	if (painter) {
		painter_lock(painter);
		kfree(painter->edid);
		painter->edid_length = 0;
		painter->edid = 0;
		painter_unlock(painter);
	} else {
		EVDI_WARN("Painter does not exist\n");
	}
}

void evdi_set_new_scanout_buffer(struct evdi_device *evdi,
				 struct evdi_framebuffer *efb)
{
	struct evdi_painter *painter = evdi->painter;

	if (efb)
		drm_framebuffer_reference(&efb->base);

	if (painter->new_scanout_fb)
		drm_framebuffer_unreference(&painter->new_scanout_fb->base);

	painter->new_scanout_fb = efb;
}

void evdi_flip_scanout_buffer(struct evdi_device *evdi)
{
	struct evdi_painter *painter = evdi->painter;

	painter_lock(painter);
	if (painter->new_scanout_fb)
		drm_framebuffer_reference(&painter->new_scanout_fb->base);

	if (painter->scanout_fb)
		drm_framebuffer_unreference(&painter->scanout_fb->base);

	painter->scanout_fb = painter->new_scanout_fb;
	painter_unlock(painter);
}
