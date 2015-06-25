// Copyright (c) 2015 DisplayLink (UK) Ltd.

#ifndef __KERNEL__
#include <stddef.h>
#include <stdint.h>
#endif
#ifdef CHROMEOS
#include <libdrm/drm.h>
#else
#include <drm/drm.h>
#endif

#define MAX_DIRTS 16

// Output events sent from driver to evdi lib
#define DRM_EVDI_EVENT_UPDATE_READY  0x80000000
#define DRM_EVDI_EVENT_DPMS          0x80000001
#define DRM_EVDI_EVENT_MODE_CHANGED  0x80000002
#define DRM_EVDI_EVENT_CRTC_STATE    0x80000003

struct drm_evdi_event_update_ready {
  struct drm_event base;
};

struct drm_evdi_event_dpms {
  struct drm_event base;
  int mode;
};

struct drm_evdi_mode {
  int width;
  int height;
  int refresh_rate;
  int bits_per_pixel;
  uint32_t pixel_format;
};

struct drm_evdi_event_mode_changed {
  struct drm_event base;
  struct drm_evdi_mode mode;
};

enum drm_evdi_crtc_state {
  EVDI_CRTC_DISABLED, 
  EVDI_CRTC_ENABLED,
};

struct drm_evdi_event_crtc_state {
  struct drm_event base;
  int state;
};

struct drm_evdi_connect {
  int connected;
  int dev_index;
  unsigned char edid[128];
};

struct drm_evdi_mapfifo {
  int reserved;
};

struct drm_evdi_request_update {
  int reserved;
};

enum drm_evdi_grabpix_mode {
  EVDI_GRABPIX_MODE_RECTS,
  EVDI_GRABPIX_MODE_DIRTY,
};

struct drm_evdi_grabpix {
  int mode;
  int buf_width;
  int buf_height;
  int buf_byte_stride;
  unsigned char* buffer;
  int num_rects;
  struct drm_clip_rect* rects;
};

// Input ioctls from evdi lib to driver
#define DRM_EVDI_CONNECT          0x00
#define DRM_EVDI_REQUEST_UPDATE   0x01
#define DRM_EVDI_GRABPIX          0x02
// LAST_IOCTL 0x5F -- 96 driver specific ioctls to use

#define DRM_IOCTL_EVDI_CONNECT DRM_IOWR(DRM_COMMAND_BASE +  \
  DRM_EVDI_CONNECT, struct drm_evdi_connect)
#define DRM_IOCTL_EVDI_REQUEST_UPDATE DRM_IOWR(DRM_COMMAND_BASE +  \
  DRM_EVDI_REQUEST_UPDATE, struct drm_evdi_request_update)
#define DRM_IOCTL_EVDI_GRABPIX DRM_IOWR(DRM_COMMAND_BASE +  \
  DRM_EVDI_GRABPIX, struct drm_evdi_grabpix)

