// Copyright (c) 2015 - 2017 DisplayLink (UK) Ltd.

#ifndef EVDI_LIB_H
#define EVDI_LIB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBEVDI_VERSION_MAJOR 1
#define LIBEVDI_VERSION_MINOR 4
#define LIBEVDI_VERSION_PATCHLEVEL 0

#define EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR 1
#define EVDI_MODULE_COMPATIBILITY_VERSION_MINOR 4
#define EVDI_MODULE_COMPATIBILITY_VERSION_PATCHLEVEL 0

struct evdi_lib_version {
	int version_major;
	int version_minor;
	int version_patchlevel;
};

struct evdi_device_context;
typedef struct evdi_device_context *evdi_handle;
typedef int evdi_selectable;

enum evdi_device_status {
	AVAILABLE,
	UNRECOGNIZED,
	NOT_PRESENT
};

struct evdi_rect {
	int x1, y1, x2, y2;
};

struct evdi_mode {
	int width;
	int height;
	int refresh_rate;
	int bits_per_pixel;
	unsigned int pixel_format;
};

struct evdi_buffer {
	int id;
	void *buffer;
	int width;
	int height;
	int stride;

	struct evdi_rect *rects;
	int rect_count;
};

struct evdi_event_context {
	void (*dpms_handler)(int dpms_mode, void *user_data);
	void (*mode_changed_handler)(struct evdi_mode mode, void *user_data);
	void (*update_ready_handler)(int buffer_to_be_updated, void *user_data);
	void (*crtc_state_handler)(int state, void *user_data);
	void *user_data;
};

#define EVDI_INVALID_HANDLE NULL

enum evdi_device_status evdi_check_device(int device);
evdi_handle evdi_open(int device);
int evdi_add_device(void);
void evdi_close(evdi_handle handle);
void evdi_connect(evdi_handle handle, const unsigned char *edid,
		  const unsigned int edid_length,
		  const uint32_t sku_area_limit);
void evdi_disconnect(evdi_handle handle);
void evdi_grab_pixels(evdi_handle handle,
		      struct evdi_rect *rects,
		      int *num_rects);
void evdi_register_buffer(evdi_handle handle, struct evdi_buffer buffer);
void evdi_unregister_buffer(evdi_handle handle, int bufferId);
bool evdi_request_update(evdi_handle handle, int bufferId);

void evdi_handle_events(evdi_handle handle, struct evdi_event_context *evtctx);
evdi_selectable evdi_get_event_ready(evdi_handle handle);
void evdi_get_lib_version(struct evdi_lib_version *version);

#ifdef __cplusplus
}
#endif

#endif

