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

#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/version.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include "evdi_drv.h"


struct evdi_fbdev {
	struct drm_fb_helper helper;
	struct evdi_framebuffer ufb;
	struct list_head fbdev_list;
	int fb_count;
};

int evdi_handle_damage(struct evdi_framebuffer *fb,
		       int x, int y, int width, int height)
{
	const struct drm_clip_rect rect = { x, y, x + width, y + height };
	struct drm_device *dev = fb->base.dev;
	struct evdi_device *evdi = dev->dev_private;
	int line_offset = 0;
	int byte_offset = 0;
	char *pix = NULL;


	EVDI_CHECKPT();

	if (!fb->active)
		return 0;

	if (!fb->obj->vmapping) {
		if (evdi_gem_vmap(fb->obj) == -ENOMEM) {
			DRM_ERROR("failed to vmap fb\n");
			return 0;
		}
		if (!fb->obj->vmapping) {
			DRM_ERROR("failed to vmapping\n");
			return 0;
		}
	}

	line_offset = fb->base.pitches[0] * y;
	byte_offset = line_offset + (x * 4);	/*RG24*/
	pix = (char *)fb->obj->vmapping + byte_offset;

	EVDI_VERBOSE
	    ("%p %d,%d-%dx%d %02x%02x%02x%02x%02x%02x%02x%02x\n", fb, x,
	     y, width, height, ((int)(pix[0]) & 0xff),
	     ((int)(pix[1]) & 0xff), ((int)(pix[2]) & 0xff),
	     ((int)(pix[3]) & 0xff), ((int)(pix[4]) & 0xff),
	     ((int)(pix[5]) & 0xff), ((int)(pix[6]) & 0xff),
	     ((int)(pix[7]) & 0xff));

	evdi_painter_mark_dirty(evdi, fb, &rect);

	return 0;
}

static int evdi_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (offset + size > info->fix.smem_len)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;

	pr_notice("mmap() framebuffer addr:%lu size:%lu\n", pos, size);

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;
}

static void evdi_fb_fillrect(struct fb_info *info,
			     const struct fb_fillrect *rect)
{
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_fillrect(info, rect);
	evdi_handle_damage(&ufbdev->ufb, rect->dx, rect->dy, rect->width,
			   rect->height);
}

static void evdi_fb_copyarea(struct fb_info *info,
			     const struct fb_copyarea *region)
{
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_copyarea(info, region);
	evdi_handle_damage(&ufbdev->ufb, region->dx, region->dy, region->width,
			   region->height);
}

static void evdi_fb_imageblit(struct fb_info *info,
			      const struct fb_image *image)
{
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_imageblit(info, image);
	evdi_handle_damage(&ufbdev->ufb, image->dx, image->dy, image->width,
			   image->height);
}

/*
 * It's common for several clients to have framebuffer open simultaneously.
 * e.g. both fbcon and X. Makes things interesting.
 * Assumes caller is holding info->lock (for open and release at least)
 */
static int evdi_fb_open(struct fb_info *info, int user)
{
	struct evdi_fbdev *ufbdev = info->par;
	struct drm_device *dev = ufbdev->ufb.base.dev;
	struct evdi_device *evdi = dev->dev_private;

	/* If the USB device is gone, we don't accept new opens */
	if (drm_device_is_unplugged(evdi->ddev))
		return -ENODEV;

	ufbdev->fb_count++;

	pr_notice("open /dev/fb%d user=%d fb_info=%p count=%d\n",
		  info->node, user, info, ufbdev->fb_count);

	return 0;
}

/*
 * Assumes caller is holding info->lock mutex (for open and release at least)
 */
static int evdi_fb_release(struct fb_info *info, int user)
{
	struct evdi_fbdev *ufbdev = info->par;

	ufbdev->fb_count--;

	pr_warn("released /dev/fb%d user=%d count=%d\n",
		info->node, user, ufbdev->fb_count);

	return 0;
}

static struct fb_ops evdifb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = evdi_fb_fillrect,
	.fb_copyarea = evdi_fb_copyarea,
	.fb_imageblit = evdi_fb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
	.fb_mmap = evdi_fb_mmap,
	.fb_open = evdi_fb_open,
	.fb_release = evdi_fb_release,
};

static int evdi_user_framebuffer_dirty(struct drm_framebuffer *fb,
				       struct drm_file *file,
				       unsigned flags,
				       unsigned color,
				       struct drm_clip_rect *clips,
				       unsigned num_clips)
{
	struct evdi_framebuffer *ufb = to_evdi_fb(fb);
	struct drm_device *dev = ufb->base.dev;
	struct evdi_device *evdi = dev->dev_private;
	int i;
	int ret = 0;

	EVDI_CHECKPT();

	drm_modeset_lock_all(fb->dev);

	if (!ufb->active)
		goto unlock;

	if (ufb->obj->base.import_attach) {
		ret =
		    dma_buf_begin_cpu_access(ufb->obj->base.import_attach->
					     dmabuf, 0, ufb->obj->base.size,
					     DMA_FROM_DEVICE);
		if (ret)
			goto unlock;
	}

	for (i = 0; i < num_clips; i++) {
		ret = evdi_handle_damage(ufb, clips[i].x1, clips[i].y1,
					 clips[i].x2 - clips[i].x1,
					 clips[i].y2 - clips[i].y1);
		if (ret)
			goto unlock;
	}

	if (ufb->obj->base.import_attach)
		dma_buf_end_cpu_access(ufb->obj->base.import_attach->dmabuf,
				       0, ufb->obj->base.size, DMA_FROM_DEVICE);
	atomic_add(1, &evdi->frame_count);
 unlock:
	drm_modeset_unlock_all(fb->dev);
	return ret;
}

static void evdi_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct evdi_framebuffer *ufb = to_evdi_fb(fb);

	EVDI_CHECKPT();
	if (ufb->obj->vmapping)
		evdi_gem_vunmap(ufb->obj);

	if (ufb->obj)
		drm_gem_object_unreference_unlocked(&ufb->obj->base);

	drm_framebuffer_cleanup(fb);
	kfree(ufb);
}

static const struct drm_framebuffer_funcs evdifb_funcs = {
	.destroy = evdi_user_framebuffer_destroy,
	.dirty = evdi_user_framebuffer_dirty,
};

static int
evdi_framebuffer_init(struct drm_device *dev,
		      struct evdi_framebuffer *ufb,
		      struct drm_mode_fb_cmd2 *mode_cmd,
		      struct evdi_gem_object *obj)
{
	ufb->obj = obj;
	drm_helper_mode_fill_fb_struct(&ufb->base, mode_cmd);
	return drm_framebuffer_init(dev, &ufb->base, &evdifb_funcs);
}

static int evdifb_create(struct drm_fb_helper *helper,
			 struct drm_fb_helper_surface_size *sizes)
{
	struct evdi_fbdev *ufbdev = (struct evdi_fbdev *)helper;
	struct drm_device *dev = ufbdev->helper.dev;
	struct fb_info *info;
	struct device *device = dev->dev;
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct evdi_gem_object *obj;
	uint32_t size;
	int ret = 0;

	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = mode_cmd.width * ((sizes->surface_bpp + 7) / 8);

	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);

	obj = evdi_gem_alloc_object(dev, size);
	if (!obj)
		goto out;

	ret = evdi_gem_vmap(obj);
	if (ret) {
		DRM_ERROR("failed to vmap fb\n");
		goto out_gfree;
	}

	info = framebuffer_alloc(0, device);
	if (!info) {
		ret = -ENOMEM;
		goto out_gfree;
	}
	info->par = ufbdev;

	ret = evdi_framebuffer_init(dev, &ufbdev->ufb, &mode_cmd, obj);
	if (ret)
		goto out_gfree;

	fb = &ufbdev->ufb.base;

	ufbdev->helper.fb = fb;
	ufbdev->helper.fbdev = info;

	strcpy(info->fix.id, "evdidrmfb");

	info->screen_base = ufbdev->ufb.obj->vmapping;
	info->fix.smem_len = size;
	info->fix.smem_start = (unsigned long)ufbdev->ufb.obj->vmapping;

	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
	info->fbops = &evdifb_ops;
	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &ufbdev->helper, sizes->fb_width,
			       sizes->fb_height);

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_gfree;
	}

	DRM_DEBUG_KMS("allocated %dx%d vmal %p\n",
		      fb->width, fb->height, ufbdev->ufb.obj->vmapping);

	return ret;
 out_gfree:
	drm_gem_object_unreference(&ufbdev->ufb.obj->base);
 out:
	return ret;
}

static struct drm_fb_helper_funcs evdi_fb_helper_funcs = {
	.fb_probe = evdifb_create,
};

static void evdi_fbdev_destroy(struct drm_device *dev,
			       struct evdi_fbdev *ufbdev)
{
	struct fb_info *info;

	if (ufbdev->helper.fbdev) {
		info = ufbdev->helper.fbdev;
		unregister_framebuffer(info);

		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}
	drm_fb_helper_fini(&ufbdev->helper);
	drm_framebuffer_unregister_private(&ufbdev->ufb.base);
	drm_framebuffer_cleanup(&ufbdev->ufb.base);
	drm_gem_object_unreference_unlocked(&ufbdev->ufb.obj->base);
}

int evdi_fbdev_init(struct drm_device *dev)
{
	struct evdi_device *evdi;
	struct evdi_fbdev *ufbdev;
	int ret;

	evdi = dev->dev_private;
	ufbdev = kzalloc(sizeof(struct evdi_fbdev), GFP_KERNEL);
	if (!ufbdev)
		return -ENOMEM;

	evdi->fbdev = ufbdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	drm_fb_helper_prepare(dev, &ufbdev->helper, &evdi_fb_helper_funcs);
#else
	ufbdev->helper.funcs = &evdi_fb_helper_funcs;
#endif

	ret = drm_fb_helper_init(dev, &ufbdev->helper, 1, 1);
	if (ret) {
		kfree(ufbdev);
		return ret;
	}

	drm_fb_helper_single_add_all_connectors(&ufbdev->helper);

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	ret = drm_fb_helper_initial_config(&ufbdev->helper, 32);
	if (ret) {
		drm_fb_helper_fini(&ufbdev->helper);
		kfree(ufbdev);
	}
	return ret;
}

void evdi_fbdev_cleanup(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;

	if (!evdi->fbdev)
		return;

	evdi_fbdev_destroy(dev, evdi->fbdev);
	kfree(evdi->fbdev);
	evdi->fbdev = NULL;
}

void evdi_fbdev_unplug(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;
	struct evdi_fbdev *ufbdev;

	if (!evdi->fbdev)
		return;

	ufbdev = evdi->fbdev;
	if (ufbdev->helper.fbdev) {
		struct fb_info *info;

		info = ufbdev->helper.fbdev;
		unlink_framebuffer(info);
	}
}

struct drm_framebuffer *evdi_fb_user_fb_create(struct drm_device *dev,
					       struct drm_file *file,
					       struct drm_mode_fb_cmd2
					       *mode_cmd)
{
	struct drm_gem_object *obj;
	struct evdi_framebuffer *ufb;
	int ret;
	uint32_t size;

	obj = drm_gem_object_lookup(dev, file, mode_cmd->handles[0]);
	if (obj == NULL)
		return ERR_PTR(-ENOENT);

	size = mode_cmd->pitches[0] * mode_cmd->height;
	size = ALIGN(size, PAGE_SIZE);

	if (size > obj->size) {
		DRM_ERROR("object size not sufficient for fb %d %zu %d %d\n",
			  size, obj->size, mode_cmd->pitches[0],
			  mode_cmd->height);
		goto err_no_mem;
	}

	ufb = kzalloc(sizeof(*ufb), GFP_KERNEL);
	if (ufb == NULL)
		goto err_no_mem;

	ret = evdi_framebuffer_init(dev, ufb, mode_cmd, to_evdi_bo(obj));
	if (ret)
		goto err_inval;
	drm_gem_object_unreference(obj);
	return &ufb->base;

 err_no_mem:
	drm_gem_object_unreference(obj);
	return ERR_PTR(-ENOMEM);
 err_inval:
	kfree(ufb);
	drm_gem_object_unreference(obj);
	return ERR_PTR(-EINVAL);
}
