// Copyright (c) 2015 - 2017 DisplayLink (UK) Ltd.
#include <stddef.h>
#include <stdint.h>
#include <libdrm/drm.h>
#ifndef __user
#  define __user
#endif
#include "evdi_drm.h"
#include "evdi_lib.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>
#include <errno.h>

// ********************* Private part **************************

#define SLEEP_INTERVAL_US   100000L
#define OPEN_TOTAL_WAIT_US  5000000L
#define MAX_FILEPATH        256
#define MAX_DIRTS       16

typedef struct _evdi_frame_buffer_node {
  evdi_buffer frame_buffer;
  bool isInvalidated;
  struct _evdi_frame_buffer_node* next;
} evdi_frame_buffer_node;

struct evdi_device_context
{
  int fd;
  int bufferToUpdate;
  evdi_frame_buffer_node* frameBuffersListHead;
  int device_index;
};

static int do_ioctl(int fd, unsigned int request, void* data, const char* msg)
{
  const int err = ioctl(fd, request, data);
  if (err < 0) {
    printf("[libevdi] ioctl: %s error=%d\n", msg, err);
  }
  return err;
}

static evdi_frame_buffer_node* findBuffer(evdi_handle context, int id)
{
  evdi_frame_buffer_node* node = NULL;

  assert(context);

  for (node = context->frameBuffersListHead; node != NULL; node = (evdi_frame_buffer_node*)node->next) {
    if (node->frame_buffer.id == id) {
      return node;
    }
  }

  return NULL;
}

static void addFrameBuffer(evdi_handle context, evdi_buffer const* frame_buffer) {
  evdi_frame_buffer_node** node = NULL;

  for (node = &context->frameBuffersListHead;; node = (evdi_frame_buffer_node**)&(*node)->next) {
    if (!(*node)) {
      *node = calloc(1, sizeof(evdi_frame_buffer_node));
      assert(node);
      memcpy(*node, frame_buffer, sizeof(evdi_buffer));
      // next is cleared by calloc
      return;
    }
  }
}

/// @brief Removes all frame buffers matching the given id
/// @param id of frame buffer to remove, NULL matches every buffer, thus all
/// will be removed
/// @return number of buffers removed
/// @todo Return value doesn't seem to be used anywhere
static int removeFrameBuffer(
  evdi_handle context,
  int const* id)
{
  evdi_frame_buffer_node* current = NULL;
  evdi_frame_buffer_node* next = NULL;
  evdi_frame_buffer_node** prev = NULL;
  int removedCount = 0;

  current = context->frameBuffersListHead;
  prev = &context->frameBuffersListHead;
  while (current) {
    next = current->next;

    if (!id || current->frame_buffer.id == *id) {
      free(current);
      ++removedCount;
      *prev = next;
    } else {
      prev = &current->next;
    }

    current = next;
  }

  return removedCount;
}

static int is_evdi(int fd)
{
  char name[64]={ 0 }, date[64]={ 0 }, desc[64]={ 0 };
  struct drm_version ver = {
    .name_len = sizeof(name),
    .name = name,
    .date_len = sizeof(date),
    .date = date,
    .desc_len = sizeof(desc),
    .desc = desc,
  };
  if (do_ioctl(fd, DRM_IOCTL_VERSION, &ver, "version") == 0
      && strcmp(name,"evdi") == 0) {
    return 1;
  }
  return 0;
}

static void invalidate(evdi_handle handle)
{
  evdi_frame_buffer_node* node = NULL;

  for (node = handle->frameBuffersListHead;
       node != NULL;
       node = (evdi_frame_buffer_node*)node->next)
  {
    node->isInvalidated = true;
  }
}

static int device_exists(int device)
{
  char dev[32] = "";
  struct stat buf;

  snprintf(dev, 31, "/dev/dri/card%d", device);
  return stat(dev, &buf) == 0 ? 1 : 0;
}

static int process_opened_device(const char* pid, const char* device_file_path)
{
  char maps_file_path[MAX_FILEPATH];
  char line[BUFSIZ];
  FILE* maps = NULL;
  int found = 0;

  snprintf(maps_file_path, MAX_FILEPATH, "/proc/%s/maps", pid);
  maps = fopen(maps_file_path, "r");
  if (maps == NULL) {
    return 0;
  }

  while (fgets(line, BUFSIZ, maps)) {
    if (strstr(line, device_file_path)) {
      found = 1;
      break;
    }
  }

  fclose(maps);
  return found;
}

static int device_has_master(const char* device_file_path)
{
  pid_t myself = getpid();
  DIR* proc_dir = opendir("/proc");
  struct dirent* process_dir = NULL;

  if (proc_dir == NULL) {
    return 0;
  }

  while ((process_dir = readdir(proc_dir)) != NULL) {
    if (process_dir->d_name[0] < '0' || process_dir->d_name[0] > '9' || myself == atoi(process_dir->d_name)) {
      continue;
    }
    if (process_opened_device(process_dir->d_name, device_file_path)) {
      return 1;
    }
  }

  return 0;
}

static int wait_for_master(const char *device_path)
{
  int cnt = OPEN_TOTAL_WAIT_US / SLEEP_INTERVAL_US;
  int has_master = device_has_master(device_path);

  while (!has_master && cnt--) {
    usleep(SLEEP_INTERVAL_US);
    has_master = device_has_master(device_path);
  }

  return has_master;
}

static int open_device(int device)
{
  char dev[32] = "";
  int dev_fd = 0;

  snprintf(dev, 31, "/dev/dri/card%d", device);

#ifndef CHROMEOS
  if (!wait_for_master(dev)) {
    return -EAGAIN;
  }
#endif

  dev_fd = open(dev, O_RDWR);
  if (dev_fd >= 0) {
    do_ioctl(dev_fd, DRM_IOCTL_DROP_MASTER, NULL, "drop_master");
  }

  return dev_fd;
}

// ********************* Public part **************************

evdi_handle evdi_open(int device)
{
  int fd;
  evdi_handle h = EVDI_INVALID_HANDLE;

  fd = open_device(device);
  if (fd > 0) {
    if (is_evdi(fd)) {
      h = calloc(1, sizeof(struct evdi_device_context));
      if (h) {
        h->fd = fd;
        h->device_index = device;
      }
    }
    if (h == EVDI_INVALID_HANDLE) {
      close(fd);
    }
  }
  return h;
}

evdi_device_status evdi_check_device(int device)
{
  evdi_device_status status = NOT_PRESENT;
  int fd = device_exists(device) ? open_device(device) : -1;

  if (fd > 0) {
    status = is_evdi(fd) ? AVAILABLE : UNRECOGNIZED;
    close(fd);
  }

  return status;
}

int evdi_add_device()
{
  FILE* add_devices = fopen("/sys/devices/evdi/add", "w");
  int written = 0;

  if (add_devices != NULL) {
    const char devices_to_add[] = "1";
    const size_t elem_bytes = 1;
    written = fwrite(devices_to_add, elem_bytes, sizeof(devices_to_add), add_devices);
    fclose(add_devices);
  }

  return written;
}

void evdi_close(evdi_handle handle)
{
  if (handle != EVDI_INVALID_HANDLE) {
    close(handle->fd);
    free(handle);
  }
}

void evdi_connect(evdi_handle handle,
		  const unsigned char* edid,
		  const unsigned edid_length,
		  const uint32_t sku_area_limit)
{
  struct drm_evdi_connect cmd = {
    .connected = 1,
    .dev_index = handle->device_index,
    .edid = edid,
    .edid_length = edid_length,
    .sku_area_limit = sku_area_limit,
  };

  do_ioctl(handle->fd, DRM_IOCTL_EVDI_CONNECT, &cmd, "connect");
}

void evdi_disconnect(evdi_handle handle)
{
  struct drm_evdi_connect cmd = { 0, 0, 0, 0, 0 };
  do_ioctl(handle->fd, DRM_IOCTL_EVDI_CONNECT, &cmd, "disconnect");
}

void evdi_grab_pixels(evdi_handle handle, evdi_rect *rects, int *num_rects)
{
  struct drm_clip_rect kernelDirts[MAX_DIRTS] = { { 0, 0, 0, 0 } };
  evdi_frame_buffer_node* destinationNode = NULL;
  evdi_buffer* destinationBuffer = NULL;

  destinationNode = findBuffer(handle, handle->bufferToUpdate);

  if (!destinationNode || destinationNode->isInvalidated) {
    printf("[libevdi] Buffer was invalidated due to mode change. Not grabbing.\n");
    *num_rects = 0;
    return;
  }

  destinationBuffer = &destinationNode->frame_buffer;

  struct drm_evdi_grabpix grab = {
    EVDI_GRABPIX_MODE_DIRTY,
    destinationBuffer->width,
    destinationBuffer->height,
    destinationBuffer->stride,
    destinationBuffer->buffer,
    MAX_DIRTS,
    kernelDirts
  };

  if (do_ioctl(handle->fd, DRM_IOCTL_EVDI_GRABPIX, &grab, "grabpix") == 0) {
    // Buffer was filled by ioctl, now we only have to fill the dirty rects
    int r = 0;

    for (; r < grab.num_rects; ++r) {
      rects[r].x1 = kernelDirts[r].x1;
      rects[r].y1 = kernelDirts[r].y1;
      rects[r].x2 = kernelDirts[r].x2;
      rects[r].y2 = kernelDirts[r].y2;
    }

    *num_rects = grab.num_rects;
  } else {
    printf("[libevdi] Grabbing pixels for buffer %d failed. Should be ignored if caused by change of mode in kernel.\n", destinationBuffer->id);
    *num_rects = 0;
  }
}

void evdi_register_buffer(evdi_handle handle, evdi_buffer buffer)
{
  assert(handle);
  assert(!findBuffer(handle, buffer.id));

  addFrameBuffer(handle, &buffer);
}

void evdi_unregister_buffer(evdi_handle handle, int bufferId)
{
  evdi_buffer* bufferToRemove = NULL;
  assert(handle);

  bufferToRemove = &findBuffer(handle, bufferId)->frame_buffer;
  assert(bufferToRemove);

  removeFrameBuffer(handle, &bufferId);
}

bool evdi_request_update(evdi_handle handle, int bufferId)
{
  evdi_frame_buffer_node* front_buffer = NULL;

  assert(handle);

  front_buffer = findBuffer(handle, bufferId);
  if (!front_buffer) {
    printf("[libevdi] Buffer %d is not registered! Ignoring update request.\n", bufferId);
    return false;
  }

  if (front_buffer->isInvalidated) {
    printf("[libevdi] Buffer %d was invalidated due to mode change! Ignoring update request.\n", bufferId);
    return false;
  }

  handle->bufferToUpdate = bufferId;

  {
    struct drm_evdi_request_update cmd;
    const int requestResult = do_ioctl(handle->fd, DRM_IOCTL_EVDI_REQUEST_UPDATE, &cmd, "request_update");
    const bool grabImmediately = requestResult == 1;

    return grabImmediately;
  }
}

evdi_mode to_evdi_mode(struct drm_evdi_event_mode_changed* event)
{
  evdi_mode e;
  e.width = event->hdisplay;
  e.height = event->vdisplay;
  e.refresh_rate = event->vrefresh;
  e.bits_per_pixel =  event->bits_per_pixel;
  e.pixel_format = event->pixel_format;
  return e;
}

void evdi_handle_events(evdi_handle handle, evdi_event_context* evtctx)
{
  char buffer[1024];
  int i = 0;

  int bytesRead = read(handle->fd, buffer, sizeof buffer);

  while (i < bytesRead) {
    struct drm_event *e = (struct drm_event *) &buffer[i];
    switch (e->type) {
    case DRM_EVDI_EVENT_UPDATE_READY:
      if (evtctx && evtctx->update_ready_handler) {
          evtctx->update_ready_handler(handle->bufferToUpdate, evtctx->user_data);
      }
      break;
    case DRM_EVDI_EVENT_DPMS:
      if (evtctx && evtctx->dpms_handler) {
        struct drm_evdi_event_dpms* dpms = (struct drm_evdi_event_dpms*) e;
        evtctx->dpms_handler(dpms->mode, evtctx->user_data);
      }
      break;
    case DRM_EVDI_EVENT_MODE_CHANGED:
      {
      struct drm_evdi_event_mode_changed* event = (struct drm_evdi_event_mode_changed*) e;

      invalidate(handle);
      if (evtctx && evtctx->mode_changed_handler) {
        evtctx->mode_changed_handler(to_evdi_mode(event), evtctx->user_data);
      }

      break;
      }
    case DRM_EVDI_EVENT_CRTC_STATE:
      {
      if (evtctx && evtctx->crtc_state_handler) {
        struct drm_evdi_event_crtc_state* event = (struct drm_evdi_event_crtc_state*)e;
        evtctx->crtc_state_handler(event->state, evtctx->user_data);
      }
      break;
      }
    } // switch

    i += e->length;
  }
}

evdi_selectable evdi_get_event_ready(evdi_handle handle)
{
  return handle->fd;
}

