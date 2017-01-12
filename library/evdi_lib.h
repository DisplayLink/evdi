// Copyright (c) 2015 - 2017 DisplayLink (UK) Ltd.

#ifndef EVDI_LIB_H
#define EVDI_LIB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct evdi_device_context;
typedef struct evdi_device_context *evdi_handle;
typedef int evdi_selectable;

typedef enum {
  AVAILABLE,
  UNRECOGNIZED,
  NOT_PRESENT
} evdi_device_status;

typedef struct {
  int x1, y1, x2, y2;
} evdi_rect;

typedef struct {
  int width;
  int height;
  int refresh_rate;
  int bits_per_pixel;
  unsigned int pixel_format;
} evdi_mode;

typedef struct {
    int id;
    void* buffer;
    int width;
    int height;
    int stride;

    evdi_rect* rects;
    int rect_count;
} evdi_buffer;

typedef struct {
  void (*dpms_handler)(int dpms_mode, void* user_data);
  void (*mode_changed_handler)(evdi_mode mode, void* user_data);
  void (*update_ready_handler)(int buffer_to_be_updated, void* user_data);
  void (*crtc_state_handler)(int state, void* user_data);
  void* user_data;
} evdi_event_context;

#define EVDI_INVALID_HANDLE NULL

evdi_device_status evdi_check_device(int device);
evdi_handle evdi_open(int device);
int evdi_add_device();
void evdi_close(evdi_handle handle);
void evdi_connect(evdi_handle handle, const unsigned char* edid, const unsigned edid_length, const evdi_mode* modes, const int modes_length);
void evdi_disconnect(evdi_handle handle);
void evdi_grab_pixels(evdi_handle handle, evdi_rect *rects, int *num_rects);
void evdi_register_buffer(evdi_handle handle, evdi_buffer buffer);
void evdi_unregister_buffer(evdi_handle handle, int bufferId);
bool evdi_request_update(evdi_handle handle, int bufferId);

void evdi_handle_events(evdi_handle handle, evdi_event_context* evtctx);
evdi_selectable evdi_get_event_ready(evdi_handle handle);

#ifdef __cplusplus
}
#endif

#endif

