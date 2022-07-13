/* SPDX-License-Identifier: LGPL-2.1-only
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 */

#ifndef EVDI_LIB_H
#define EVDI_LIB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBEVDI_VERSION_MAJOR 1
#define LIBEVDI_VERSION_MINOR 12
#define LIBEVDI_VERSION_PATCH 0

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

struct evdi_cursor_set {
	int32_t hot_x;
	int32_t hot_y;
	uint32_t width;
	uint32_t height;
	uint8_t enabled;
	uint32_t buffer_length;
	uint32_t *buffer;
	uint32_t pixel_format;
	uint32_t stride;
};

struct evdi_cursor_move {
	int32_t x;
	int32_t y;
};

struct evdi_ddcci_data {
	uint16_t address;
	uint16_t flags;
	uint32_t buffer_length;
	uint8_t *buffer;
};

struct evdi_event_context {
	void (*dpms_handler)(int dpms_mode, void *user_data);
	void (*mode_changed_handler)(struct evdi_mode mode, void *user_data);
	void (*update_ready_handler)(int buffer_to_be_updated, void *user_data);
	void (*crtc_state_handler)(int state, void *user_data);
	void (*cursor_set_handler)(struct evdi_cursor_set cursor_set,
				   void *user_data);
	void (*cursor_move_handler)(struct evdi_cursor_move cursor_move,
				    void *user_data);
	void (*ddcci_data_handler)(struct evdi_ddcci_data ddcci_data,
				   void *user_data);
	void *user_data;
};

struct evdi_logging {
	void (*function)(void *user_data, const char *fmt, ...);
	void *user_data;
};

#define EVDI_INVALID_HANDLE NULL

enum evdi_device_status evdi_check_device(int device);
evdi_handle evdi_open(int device);
int evdi_add_device(void);
evdi_handle evdi_open_attached_to(const char *sysfs_parent_device);

void evdi_close(evdi_handle handle);
void evdi_connect(evdi_handle handle, const unsigned char *edid,
		  const unsigned int edid_length,
		  const uint32_t pixel_area_limit,
		  const uint32_t pixel_per_second_limit);
void evdi_disconnect(evdi_handle handle);
void evdi_enable_cursor_events(evdi_handle handle, bool enable);

void evdi_grab_pixels(evdi_handle handle,
		      struct evdi_rect *rects,
		      int *num_rects);
void evdi_register_buffer(evdi_handle handle, struct evdi_buffer buffer);
void evdi_unregister_buffer(evdi_handle handle, int bufferId);
bool evdi_request_update(evdi_handle handle, int bufferId);
void evdi_ddcci_response(evdi_handle handle, const unsigned char *buffer,
		const uint32_t buffer_length,
		const bool result);

void evdi_handle_events(evdi_handle handle, struct evdi_event_context *evtctx);
evdi_selectable evdi_get_event_ready(evdi_handle handle);
void evdi_get_lib_version(struct evdi_lib_version *version);
void evdi_set_logging(struct evdi_logging evdi_logging);

#ifdef __cplusplus
}
#endif

#endif
