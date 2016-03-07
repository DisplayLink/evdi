/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2016 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>

#include <drm/drmP.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
#include <drm/drm_atomic.h>
#endif
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
#include <drm/drm_plane_helper.h>
#endif
#include "evdi_drm.h"
#include "evdi_drv.h"
#include "evdi_cursor.h"

#define EVDI_CURSOR_W 64
#define EVDI_CURSOR_H 64
#define EVDI_CURSOR_BUF (EVDI_CURSOR_W * EVDI_CURSOR_H)

struct evdi_flip_queue
{
	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work work;
	struct drm_crtc *crtc;
	struct drm_pending_vblank_event *event;
	u64 flip_time;  /* in jiffies */
	u64 vblank_interval;  /* in jiffies */
};

static void evdi_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct evdi_device *evdi = crtc->dev->dev_private;

	evdi_painter_dpms_notify(evdi, mode);
}

static bool evdi_crtc_mode_fixup(struct drm_crtc *crtc,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	return true;
}


static int evdi_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode,
			      int x, int y, struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = NULL;
	struct evdi_device *evdi = NULL;
	struct evdi_framebuffer *efb = NULL;
	struct evdi_flip_queue *flip_queue = NULL;

	if (NULL == crtc->primary) {
		EVDI_DEBUG("evdi_crtc_mode_set primary plane is NULL");
		return 0;
	}

	EVDI_ENTER();

	efb = to_evdi_fb(crtc->primary->fb);
	if (old_fb) {
		struct evdi_framebuffer *eold_fb = to_evdi_fb(old_fb);

		eold_fb->active = false;
	}
	efb->active = true;

	dev = efb->base.dev;
	evdi = dev->dev_private;
	evdi_painter_mode_changed_notify(evdi, &efb->base, adjusted_mode);

	/* update flip queue vblank interval */
	flip_queue = evdi->flip_queue;
	if (flip_queue) {
		mutex_lock(&flip_queue->lock);
		flip_queue->vblank_interval = HZ / drm_mode_vrefresh(mode);
		mutex_unlock(&flip_queue->lock);
	}

	/* damage all of it */
	evdi_handle_damage(efb, 0, 0, efb->base.width, efb->base.height);
	EVDI_EXIT();
	return 0;
}

static void evdi_crtc_disable(struct drm_crtc *crtc)
{
	struct evdi_device *evdi = crtc->dev->dev_private;

	EVDI_CHECKPT();
	evdi_painter_crtc_state_notify(evdi, DRM_MODE_DPMS_OFF);
}

static void evdi_crtc_destroy(struct drm_crtc *crtc)
{
	EVDI_CHECKPT();
	drm_crtc_cleanup(crtc);
	kfree(crtc);
}

static void evdi_sched_page_flip(struct work_struct *work)
{
	struct evdi_flip_queue *flip_queue =
		container_of(container_of(work, struct delayed_work, work),
			struct evdi_flip_queue, work);
	struct drm_crtc *crtc;
	struct drm_device *dev;
	struct drm_pending_vblank_event *event;
	struct drm_framebuffer *fb;

	mutex_lock(&flip_queue->lock);
	crtc = flip_queue->crtc;
	dev = crtc->dev;
	event = flip_queue->event;
	fb = crtc->primary->fb;
	flip_queue->event = NULL;
	mutex_unlock(&flip_queue->lock);

	EVDI_CHECKPT();
	if (fb)
		evdi_handle_damage(to_evdi_fb(fb), 0, 0, fb->width, fb->height);
	if (event) {
		unsigned long flags = 0;

		spin_lock_irqsave(&dev->event_lock, flags);
		drm_send_vblank_event(dev, 0, event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}
}

static int evdi_crtc_page_flip(struct drm_crtc *crtc,
			       struct drm_framebuffer *fb,
			       struct drm_pending_vblank_event *event,
			       uint32_t page_flip_flags)
{
	struct drm_device *dev = crtc->dev;
	struct evdi_device *evdi = dev->dev_private;
	struct evdi_flip_queue *flip_queue = evdi->flip_queue;

	if (!flip_queue || !flip_queue->wq) {
		DRM_ERROR("Uninitialized page flip queue\n");
		return -ENOMEM;
	}

	mutex_lock(&flip_queue->lock);

	EVDI_CHECKPT();
	atomic_inc(&evdi->frame_count);
	flip_queue->crtc = crtc;
	if (fb) {
		struct evdi_framebuffer *efb = to_evdi_fb(fb);
		struct drm_framebuffer *old_fb = crtc->primary->fb;

		if (old_fb) {
			struct evdi_framebuffer *eold_fb = to_evdi_fb(old_fb);

			eold_fb->active = false;
		}
		efb->active = true;
		crtc->primary->fb = fb;
	}
	if (event) {
		if (flip_queue->event) {
			unsigned long flags = 0;

			spin_lock_irqsave(&dev->event_lock, flags);
			drm_send_vblank_event(dev, 0, flip_queue->event);
			spin_unlock_irqrestore(&dev->event_lock, flags);
		}
		flip_queue->event = event;
	}
	if (!delayed_work_pending(&flip_queue->work)) {
		u64 now = jiffies;
		u64 next_flip =
			flip_queue->flip_time + flip_queue->vblank_interval;
		flip_queue->flip_time = (next_flip < now) ? now : next_flip;
		queue_delayed_work(flip_queue->wq, &flip_queue->work,
			flip_queue->flip_time - now);
	}

	mutex_unlock(&flip_queue->lock);

	return 0;
}

static int evdi_crtc_cursor_set(struct drm_crtc *crtc,
				struct drm_file *file,
				uint32_t handle,
				uint32_t width,
				uint32_t height)
{
	struct drm_device *dev = crtc->dev;
	struct evdi_device *evdi = dev->dev_private;
	int ret;

	EVDI_CHECKPT();
	mutex_lock(&dev->struct_mutex);
	ret = evdi_cursor_set(crtc, file, handle, width, height, evdi->cursor);
	mutex_unlock(&dev->struct_mutex);
	EVDI_DEBUG("evdi_crtc_cursor_set unlock\n");
	if (ret) {
		DRM_ERROR("Failed to set evdi cursor\n");
		return ret;
	}

	/* For now we don't care whether the application wanted the mouse set,
	   or not. */
	return evdi_crtc_page_flip(crtc, NULL, NULL, 0);
}

static int evdi_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct evdi_device *evdi = dev->dev_private;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);
	if (!evdi_cursor_enabled(evdi->cursor))
		goto error;
		ret = evdi_cursor_move(crtc, x, y, evdi->cursor);
		if (ret) {
			DRM_ERROR("Failed to move evdi cursor\n");
			goto error;
		}
	mutex_unlock(&dev->struct_mutex);
	return evdi_crtc_page_flip(crtc, NULL, NULL, 0);
error:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

static void evdi_crtc_prepare(struct drm_crtc *crtc)
{
}

static void evdi_crtc_commit(struct drm_crtc *crtc)
{
	struct evdi_device *evdi = crtc->dev->dev_private;

	EVDI_CHECKPT();
	evdi_painter_crtc_state_notify(evdi, DRM_MODE_DPMS_ON);
}

static struct drm_crtc_helper_funcs evdi_helper_funcs = {
	.dpms = evdi_crtc_dpms,
	.mode_fixup = evdi_crtc_mode_fixup,
	.mode_set = evdi_crtc_mode_set,
	.prepare = evdi_crtc_prepare,
	.commit = evdi_crtc_commit,
	.disable = evdi_crtc_disable,
};

static const struct drm_crtc_funcs evdi_crtc_funcs = {
	.set_config = drm_crtc_helper_set_config,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
	.set_property = drm_atomic_crtc_set_property,
#endif
	.destroy = evdi_crtc_destroy,
	.page_flip = evdi_crtc_page_flip,
	.cursor_set = evdi_crtc_cursor_set,
	.cursor_move = evdi_crtc_cursor_move,
};

static int evdi_crtc_init(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	int status = 0;

	EVDI_CHECKPT();
	crtc = kzalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (crtc == NULL)
		return -ENOMEM;

	status = drm_crtc_init(dev, crtc, &evdi_crtc_funcs);
	EVDI_DEBUG("drm_crtc_init: %d\n", status);
	drm_crtc_helper_add(crtc, &evdi_helper_funcs);

	return 0;
}

static void evdi_flip_workqueue_init(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;
	struct evdi_flip_queue *flip_queue =
		kzalloc(sizeof(struct evdi_flip_queue), GFP_KERNEL);

	EVDI_CHECKPT();
	BUG_ON(!flip_queue);
	mutex_init(&flip_queue->lock);
	flip_queue->wq = create_singlethread_workqueue("flip");
	BUG_ON(!flip_queue->wq);
	INIT_DELAYED_WORK(&flip_queue->work, evdi_sched_page_flip);
	flip_queue->flip_time = jiffies;
	flip_queue->vblank_interval = HZ / 60;
	evdi->flip_queue = flip_queue;
}

static void evdi_flip_workqueue_cleanup(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;
	struct evdi_flip_queue *flip_queue = evdi->flip_queue;

	if (!flip_queue)
		return;

	EVDI_CHECKPT();
	if (flip_queue->wq) {
		flush_workqueue(flip_queue->wq);
		destroy_workqueue(flip_queue->wq);
	}
	mutex_destroy(&flip_queue->lock);
	kfree(flip_queue);
}

static const struct drm_mode_config_funcs evdi_mode_funcs = {
	.fb_create = evdi_fb_user_fb_create,
	.output_poll_changed = NULL,
};

int evdi_modeset_init(struct drm_device *dev)
{
	struct drm_encoder *encoder;

	EVDI_CHECKPT();
	drm_mode_config_init(dev);

	dev->mode_config.min_width = 640;
	dev->mode_config.min_height = 480;

	dev->mode_config.max_width = 3840;
	dev->mode_config.max_height = 2160;

	dev->mode_config.prefer_shadow = 0;
	dev->mode_config.preferred_depth = 24;

	dev->mode_config.funcs = &evdi_mode_funcs;

	drm_mode_create_dirty_info_property(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0)
	drm_dev_set_unique(dev, dev_name(dev->dev));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	drm_dev_set_unique(dev, "%s", dev_name(dev->dev));
#endif
	evdi_crtc_init(dev);

	encoder = evdi_encoder_init(dev);

	evdi_connector_init(dev, encoder);

	evdi_flip_workqueue_init(dev);

	return 0;
}

void evdi_modeset_cleanup(struct drm_device *dev)
{
	EVDI_CHECKPT();
	evdi_flip_workqueue_cleanup(dev);
	drm_mode_config_cleanup(dev);
}
