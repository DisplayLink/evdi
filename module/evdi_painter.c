/*
 * Copyright (c) 2013 - 2015 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include "evdi_drv.h"
#include "evdi_ioctl.h"
#include <linux/mutex.h>

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

struct evdi_painter {
  bool is_connected;
  u8 edid[EDID_LENGTH];

  struct mutex lock;
  struct drm_clip_rect dirty_rects[MAX_DIRTS];
  int num_dirts;
  struct evdi_framebuffer* recent_fb;

  struct drm_file* drm_filp;

  bool was_update_requested;
  struct drm_evdi_mode current_mode;
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

static bool contains_rect(const struct drm_clip_rect *outer, const struct drm_clip_rect *inner)
{
  if (inner->x1 >= outer->x1
      && inner->y1 >= outer->y1
      && inner->x2 <= outer->x2
      && inner->y2 <= outer->y2) {
    return true;
  }
  return false;
}

static void merge_dirty_rects(struct drm_clip_rect *rects, int *count)
{
  int a, b;
  for (a = 0; a< *count - 1; ++a) {
    for (b = a + 1; b < *count;) {
      bool eliminate_b = contains_rect(&rects[a], &rects[b]);
      if (!eliminate_b && contains_rect(&rects[b], &rects[a])) {
        rects[a] = rects[b];
        eliminate_b = true;
      }
      if (!eliminate_b) {
        /* collapse to bounding rect if it is less pixels */
        const int area_a = rect_area(&rects[a]);
        const int area_b = rect_area(&rects[b]);
        struct drm_clip_rect bounding_rect = rects[a];
        expand_rect(&bounding_rect, &rects[b]);

        if (rect_area(&bounding_rect) <= area_a + area_b) {
          rects[a] = bounding_rect;
          eliminate_b = true;
        }
      }
      if (eliminate_b) {
        if (b + 1 < *count) {
          memcpy(rects+b, rects + b + 1, (*count - b - 1) * sizeof(*rects));
        }
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

  for (i = 1; i<*count; ++i) {
    expand_rect(&rects[0], &rects[i]);
  }
  *count = 1;
}

static void copy_pixels(struct evdi_framebuffer *ufb,
                        char *buffer,
                        int buf_byte_stride,
                        int num_rects,
                        struct drm_clip_rect* rects)
{
  struct drm_framebuffer *fb = &ufb->base;
  struct drm_clip_rect *r;
  int unused;

  EVDI_CHECKPT();

  for (r = rects; r != rects + num_rects; ++r) {
    const int byte_offset = r->x1 * 4;
    const int byte_span = (r->x2 - r->x1) * 4;
    const int src_offset = fb->pitches[0] * r->y1 + byte_offset;
    const char* src = (char*)ufb->obj->vmapping + src_offset;
    const int dst_offset = buf_byte_stride * r->y1 + byte_offset;
    char* dst = buffer + dst_offset;
    int y = r->y2 - r->y1;

    EVDI_VERBOSE("copy rect %d,%d-%d,%d\n", r->x1, r->y1, r->x2, r->y2);

    for (; y > 0 ;--y) {
      unused = copy_to_user(dst, src, byte_span);
      src += fb->pitches[0];
      dst += buf_byte_stride;
    }
  }
}

static void painter_lock(struct evdi_painter *painter)
{
  EVDI_CHECKPT();
  mutex_lock(&painter->lock);
}

static void painter_unlock(struct evdi_painter *painter)
{
  EVDI_CHECKPT();
  mutex_unlock(&painter->lock);
}

bool evdi_painter_is_connected(struct evdi_device *evdi)
{
  if (evdi && evdi->painter) {
    return evdi->painter->is_connected;
  }
  return false;
}

u8 *evdi_painter_get_edid(struct evdi_device *evdi)
{
  u8 *block = NULL;
  EVDI_CHECKPT();

  painter_lock(evdi->painter);
  if (evdi_painter_is_connected(evdi)) {
    block = kmalloc(EDID_LENGTH, GFP_KERNEL);
    if (block) {
      memcpy(block, &evdi->painter->edid[0], EDID_LENGTH);
      EVDI_DEBUG("(dev=%d) %02x %02x %02x\n", evdi->dev_index, block[0], block[1], block[2]);
    }
  }
  painter_unlock(evdi->painter);
  return block;
}

static void evdi_painter_send_event(struct drm_file* drm_filp, struct list_head* event_link)
{
  struct drm_device* dev = drm_filp->minor->dev;
  unsigned long flags;
  spin_lock_irqsave(&dev->event_lock, flags);
  list_add_tail(event_link, &drm_filp->event_list);
  wake_up_interruptible(&drm_filp->event_wait);
  spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void evdi_painter_send_update_ready(struct evdi_painter* painter)
{
  struct evdi_event_update_ready_pending *event;
  if (painter->drm_filp) {
    event = kzalloc(sizeof(*event), GFP_KERNEL);
    event->update_ready.base.type = DRM_EVDI_EVENT_UPDATE_READY;
    event->update_ready.base.length = sizeof(*event);
    event->base.event = &event->update_ready.base;
    event->base.file_priv = painter->drm_filp;
    event->base.destroy = (void (*)(struct drm_pending_event*)) kfree;
    evdi_painter_send_event(painter->drm_filp, &event->base.link);
  }
}

static void evdi_painter_send_dpms(struct evdi_painter* painter, int mode)
{
  struct evdi_event_dpms_pending *event;
  if (painter->drm_filp) {
    event = kzalloc(sizeof(*event), GFP_KERNEL);
    event->dpms.base.type = DRM_EVDI_EVENT_DPMS;
    event->dpms.base.length = sizeof(*event);
    event->dpms.mode = mode;
    event->base.event = &event->dpms.base;
    event->base.file_priv = painter->drm_filp;
    event->base.destroy = (void(*)(struct drm_pending_event*)) kfree;
    evdi_painter_send_event(painter->drm_filp, &event->base.link);
  }
}

static void evdi_painter_send_crtc_state(struct evdi_painter* painter, int state)
{
  struct evdi_event_crtc_state_pending *event;
  if (painter->drm_filp) {
    event = kzalloc(sizeof(*event), GFP_KERNEL);
    event->crtc_state.base.type = DRM_EVDI_EVENT_CRTC_STATE;
    event->crtc_state.base.length = sizeof(*event);
    event->crtc_state.state = state;
    event->base.event = &event->crtc_state.base;
    event->base.file_priv = painter->drm_filp;
    event->base.destroy = (void(*)(struct drm_pending_event*)) kfree;
    evdi_painter_send_event(painter->drm_filp, &event->base.link);
  }
}

static void evdi_painter_fill_mode(const struct drm_display_mode* drm_mode, const struct drm_framebuffer* fb, struct drm_evdi_mode* to_fill)
{
  to_fill->width = fb->width;
  to_fill->height = fb->height;
  to_fill->refresh_rate = drm_mode_vrefresh(drm_mode);
  to_fill->bits_per_pixel = fb->bits_per_pixel;
  to_fill->pixel_format = fb->pixel_format;
}

static bool evdi_painter_modes_equal(const struct drm_evdi_mode* left, const struct drm_evdi_mode* right)
{
  return
    left->width == right->width &&
    left->height == right->height &&
    left->refresh_rate == right->refresh_rate &&
    left->bits_per_pixel == right->bits_per_pixel &&
    left->pixel_format == right->pixel_format;
}

/// @return \c true if the mode was truly replaced/changed (comparing to previously set)
static bool evdi_painter_replace_mode(struct evdi_painter* painter, const struct drm_evdi_mode* new_mode)
{
  struct drm_evdi_mode* old_mode = &painter->current_mode;

  if (evdi_painter_modes_equal(old_mode, new_mode)) {
    return false;
  } else {
    memcpy(old_mode, new_mode, sizeof(struct drm_evdi_mode));
    return true;
  }
}

static void evdi_painter_send_mode_changed(struct evdi_painter *painter,
                                    struct drm_evdi_mode *mode)
{
  struct evdi_event_mode_changed_pending *event;
  if (painter->drm_filp) {
    event = kzalloc(sizeof(*event), GFP_KERNEL);
    event->mode_changed.base.type = DRM_EVDI_EVENT_MODE_CHANGED;
    event->mode_changed.base.length = sizeof(*event);
    memcpy(&event->mode_changed.mode, mode, sizeof(struct drm_evdi_mode));
    event->base.event = &event->mode_changed.base;
    event->base.file_priv = painter->drm_filp;
    event->base.destroy = (void (*)(struct drm_pending_event*)) kfree;
    evdi_painter_send_event(painter->drm_filp, &event->base.link);
  }
}

void evdi_painter_mark_dirty(struct evdi_device *evdi,
                             struct evdi_framebuffer *fb,
                             const struct drm_clip_rect* rect)
{
  struct evdi_painter *painter = evdi->painter;

  painter_lock(evdi->painter);
  EVDI_VERBOSE("(dev=%d) %d,%d-%d,%d\n", evdi->dev_index, rect->x1, rect->y1, rect->x2, rect->y2);

  if (painter->num_dirts == MAX_DIRTS) {
    merge_dirty_rects(&painter->dirty_rects[0], &painter->num_dirts);
  }
  if (painter->num_dirts == MAX_DIRTS) {
    collapse_dirty_rects(&painter->dirty_rects[0], &painter->num_dirts);
  }

  memcpy(&painter->dirty_rects[painter->num_dirts], rect, sizeof(*rect));
  painter->num_dirts++;

  if (painter->recent_fb != fb) {
    if (painter->recent_fb) {
      drm_framebuffer_unreference(&painter->recent_fb->base);
    }
    drm_framebuffer_reference(&fb->base);
    painter->recent_fb = fb;
  }

  if (painter->was_update_requested) {
      evdi_painter_send_update_ready(painter);
      painter->was_update_requested = false;
  }
  
  painter_unlock(evdi->painter);
}

void evdi_painter_dpms_notify(struct evdi_device *evdi, int mode)
{
  struct evdi_painter *painter = evdi->painter;
  if (painter) {
    EVDI_DEBUG("(dev=%d) Notifying dpms mode: %d\n", evdi->dev_index, mode);
    evdi_painter_send_dpms(painter, mode);
  }
}

void evdi_painter_crtc_state_notify(struct evdi_device *evdi, int state)
{
  struct evdi_painter *painter = evdi->painter;
  if (painter) {
    EVDI_DEBUG("(dev=%d) Notifying crtc state: %d\n", evdi->dev_index, state);
    evdi_painter_send_crtc_state(painter, state);
  }
}

void evdi_painter_mode_changed_notify(struct evdi_device *evdi,
                                      struct drm_framebuffer* fb,
                                      struct drm_display_mode *mode)
{
  struct evdi_painter *painter = evdi->painter;
  struct drm_evdi_mode new_mode;

  evdi_painter_fill_mode(mode, fb, &new_mode);

  if (evdi_painter_replace_mode(painter, &new_mode)) {
    EVDI_DEBUG(
      "(dev=%d) Notifying mode changed: %dx%d@%d; bpp %d; pixel format %d\n",
      evdi->dev_index,
      new_mode.width,
      new_mode.height,
      new_mode.refresh_rate,
      new_mode.bits_per_pixel,
      new_mode.pixel_format
    );

    evdi_painter_send_mode_changed(painter, &new_mode);
  } else {
    EVDI_WARN("(dev=%d) Change mode duplicated - ignoring\n", evdi->dev_index);
  }
}

void evdi_painter_connect(struct evdi_device* evdi, void const* edid, struct drm_file* file, int dev_index)
{
  struct evdi_painter* painter = evdi->painter;
  EVDI_CHECKPT();

  painter_lock(painter);

  if (painter->drm_filp) {
    EVDI_ERROR("(dev=%d) Double connect - replacing %p with %p\n", dev_index, painter->drm_filp, file);
  }

  painter->drm_filp = file;
  evdi->dev_index = dev_index;
  EVDI_DEBUG("(dev=%d) Connected with %p\n", evdi->dev_index, painter->drm_filp);

  memcpy(painter->edid, edid, EDID_LENGTH);
  EVDI_DEBUG("(dev=%d) Edid (3 bytes): %02x %02x %02x\n", evdi->dev_index, painter->edid[0], painter->edid[1], painter->edid[2]);

  painter->is_connected = true;

  painter_unlock(painter);

  drm_helper_hpd_irq_event(evdi->ddev);
}

void evdi_painter_disconnect(struct evdi_device* evdi, struct drm_file* file)
{
  struct evdi_painter* painter = evdi->painter;

  EVDI_CHECKPT();

  painter_lock(painter);

  if (file != painter->drm_filp) {
    EVDI_WARN("(dev=%d) An unknown connection to %p tries to close us - ignoring\n", evdi->dev_index, file);

    painter_unlock(painter);
    return;
  }

  if (painter->recent_fb) {
    drm_framebuffer_unreference(&painter->recent_fb->base);
    painter->recent_fb = NULL;
  }

  painter->is_connected = false;

  EVDI_DEBUG("(dev=%d) Disconnected from %p\n", evdi->dev_index, painter->drm_filp);
  painter->drm_filp = NULL;
  evdi->dev_index = -1;

  memset(&painter->current_mode, '\0', sizeof(struct drm_evdi_mode));
  painter->was_update_requested = false;

  painter_unlock(painter);

  drm_helper_hpd_irq_event(evdi->ddev);
}

void evdi_painter_close(struct evdi_device* evdi, struct drm_file* file)
{
  EVDI_CHECKPT();

  if (evdi->painter) {
    evdi_painter_disconnect(evdi, file);
  }
}

int evdi_painter_connect_ioctl(struct drm_device *drm_dev, void *data, struct drm_file *file)
{
  struct evdi_device *evdi = drm_dev->dev_private;
  struct evdi_painter *painter = evdi->painter;
  struct drm_evdi_connect *cmd = data;

  EVDI_CHECKPT();

  if (painter) {
    if (cmd->connected) {
      evdi_painter_connect(evdi, cmd->edid, file, cmd->dev_index);
    } else {
      evdi_painter_disconnect(evdi, file);
    }

    return 0;
  }

  return -ENODEV;
}

int evdi_painter_grabpix_ioctl(struct drm_device *drm_dev, void *data, struct drm_file *file)
{
  struct evdi_device *evdi = drm_dev->dev_private;
  struct evdi_painter *painter = evdi->painter;
  struct drm_evdi_grabpix *cmd = data;

  if (painter) {
    EVDI_CHECKPT();

    if (!painter->recent_fb) {
      EVDI_CHECKPT();
      return -EAGAIN;
    }

    {
      struct drm_framebuffer *fb;
      int err = 0;

      painter_lock(evdi->painter);

      if (painter->was_update_requested) {
        EVDI_WARN("(dev=%d) Update ready not sent, but pixels are grabbed.\n", evdi->dev_index);
      }

      fb = &painter->recent_fb->base;

      if (cmd->buf_width != fb->width || cmd->buf_height != fb->height || cmd->num_rects < 1) {
        EVDI_CHECKPT();
        err = -EINVAL;
      } else if (cmd->mode == EVDI_GRABPIX_MODE_DIRTY) {
        EVDI_CHECKPT();
        if (painter->num_dirts < 0) {
          err = -EAGAIN;
        } else {
          unsigned long ret = 0;
          merge_dirty_rects(&painter->dirty_rects[0], &painter->num_dirts);
          if (painter->num_dirts > cmd->num_rects) {
            collapse_dirty_rects(&painter->dirty_rects[0], &painter->num_dirts);
          }
          cmd->num_rects = painter->num_dirts;
          ret = copy_to_user(cmd->rects, painter->dirty_rects, cmd->num_rects * sizeof(cmd->rects[0]));

          copy_pixels(painter->recent_fb,
                      cmd->buffer,
                      cmd->buf_byte_stride,
                      painter->num_dirts,
                      painter->dirty_rects);

          painter->num_dirts = 0;
        }
      }

      painter_unlock(evdi->painter);
      return err;
    }

  }
  return -ENODEV;
}

int evdi_painter_request_update_ioctl(struct drm_device *drm_dev, void *data, struct drm_file *file)
{
  struct evdi_device *evdi = drm_dev->dev_private;
  struct evdi_painter *painter = evdi->painter;
  int result = 0;

  if (painter) {
    painter_lock(painter);

    if (painter->was_update_requested) {
      EVDI_WARN("(dev=%d) Update was already requested - ignoring\n", evdi->dev_index);
    } else {
      if (painter->num_dirts > 0) {
        result = 1;
      } else {
        painter->was_update_requested = true;
      }
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
    return 0;
  }
  return -ENOMEM;
}

