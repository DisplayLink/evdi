// SPDX-License-Identifier: LGPL-2.1-only
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// ********************* Private part **************************

#define MAX_DIRTS           16

#define EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR 1
#define EVDI_MODULE_COMPATIBILITY_VERSION_MINOR 6
#define EVDI_MODULE_COMPATIBILITY_VERSION_PATCHLEVEL 0

#define evdi_log(...) do {						\
	if (g_evdi_logging.function) {					\
		g_evdi_logging.function(g_evdi_logging.user_data,	\
					__VA_ARGS__);			\
	} else {							\
		printf("[libevdi] " __VA_ARGS__);			\
		printf("\n");						\
	}								\
} while (0)

struct evdi_logging g_evdi_logging = {
	.function = NULL,
	.user_data = NULL
};

struct evdi_frame_buffer_node {
	struct evdi_buffer frame_buffer;
	struct evdi_frame_buffer_node *next;
};

struct evdi_device_context {
	int fd;
	int bufferToUpdate;
	struct evdi_frame_buffer_node *frameBuffersListHead;
	int device_index;
};

static int do_ioctl(int fd, unsigned int request, void *data, const char *msg)
{
	const int err = ioctl(fd, request, data);

	if (err < 0)
		evdi_log("Ioctl %s error: %s", msg, strerror(errno));
	return err;
}

static struct evdi_frame_buffer_node *findBuffer(evdi_handle context, int id)
{
	struct evdi_frame_buffer_node *node = NULL;

	assert(context);

	for (node = context->frameBuffersListHead;
	     node != NULL;
	     node = (struct evdi_frame_buffer_node *)node->next) {
		if (node->frame_buffer.id == id)
			return node;
	}

	return NULL;
}

static void addFrameBuffer(evdi_handle context,
			   struct evdi_buffer const *frame_buffer)
{
	struct evdi_frame_buffer_node **node = NULL;

	for (node = &context->frameBuffersListHead;
	     ;
	     node = (struct evdi_frame_buffer_node **)&(*node)->next) {
		if (*node)
			continue;

		*node = calloc(1, sizeof(struct evdi_frame_buffer_node));
		assert(node);
		memcpy(*node, frame_buffer, sizeof(struct evdi_buffer));
		return;
	}
}

/*
 * @brief Removes all frame buffers matching the given id
 * @param id of frame buffer to remove, NULL matches every buffer, thus all
 * will be removed
 * @return number of buffers removed
 * @todo Return value doesn't seem to be used anywhere
 */
static int removeFrameBuffer(evdi_handle context, int const *id)
{
	struct evdi_frame_buffer_node *current = NULL;
	struct evdi_frame_buffer_node *next = NULL;
	struct evdi_frame_buffer_node **prev = NULL;
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

static int is_evdi_compatible(int fd)
{
	struct drm_version ver = { 0 };

	evdi_log("LibEvdi version (%d.%d.%d)",
		 LIBEVDI_VERSION_MAJOR,
		 LIBEVDI_VERSION_MINOR,
		 LIBEVDI_VERSION_PATCHLEVEL);

	if (do_ioctl(fd, DRM_IOCTL_VERSION, &ver, "version") != 0)
		return 0;

	evdi_log("Evdi version (%d.%d.%d)",
		 ver.version_major,
		 ver.version_minor,
		 ver.version_patchlevel);

	if (ver.version_major == EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR &&
	    ver.version_minor == EVDI_MODULE_COMPATIBILITY_VERSION_MINOR)
		return 1;

	evdi_log("Doesn't match LibEvdi compatibility one (%d.%d.%d)",
		 EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR,
		 EVDI_MODULE_COMPATIBILITY_VERSION_MINOR,
		 EVDI_MODULE_COMPATIBILITY_VERSION_PATCHLEVEL);

#ifdef NDEBUG
	return 0;
#else
	return 1;
#endif
}

static int is_evdi(int fd)
{
	char name[64] = { 0 }, date[64] = { 0 }, desc[64] = { 0 };
	struct drm_version ver = {
		.name_len = sizeof(name),
		.name = name,
		.date_len = sizeof(date),
		.date = date,
		.desc_len = sizeof(desc),
		.desc = desc,
	};
	if (do_ioctl(fd, DRM_IOCTL_VERSION, &ver, "version") == 0
	    && strcmp(name, "evdi") == 0) {
		return 1;
	}
	return 0;
}

static int path_exists(const char *path)
{
	struct stat buf;

	return stat(path, &buf) == 0;
}

static int device_exists(int device)
{
	char dev[PATH_MAX] = "";

	snprintf(dev, PATH_MAX, "/dev/dri/card%d", device);
	return path_exists(dev);
}

static int does_path_links_to(const char *link, const char *substr)
{
	char real_path[PATH_MAX];
	ssize_t r;

	r = readlink(link, real_path, sizeof(real_path));
	if (r < 0)
		return 0;
	real_path[r] = '\0';

	return (strstr(real_path, substr) != NULL);
}

static int process_opened_device(const char *pid, const char *device_file_path)
{
	char maps_path[PATH_MAX];
	FILE *maps = NULL;
	char line[BUFSIZ];
	int result = 0;

	snprintf(maps_path, PATH_MAX, "/proc/%s/maps", pid);

	maps = fopen(maps_path, "r");
	if (maps == NULL)
		return 0;

	while (fgets(line, BUFSIZ, maps)) {
		if (strstr(line, device_file_path)) {
			result = 1;
			break;
		}
	}

	fclose(maps);
	return result;
}

static int process_opened_files(const char *pid, const char *device_file_path)
{
	char fd_path[PATH_MAX];
	DIR *fd_dir;
	struct dirent *fd_entry;
	int result = 0;

	snprintf(fd_path, PATH_MAX, "/proc/%s/fd", pid);

	fd_dir = opendir(fd_path);
	if (fd_dir == NULL)
		return 0;

	while ((fd_entry = readdir(fd_dir)) != NULL) {
		char *d_name = fd_entry->d_name;
		char path[PATH_MAX];

		snprintf(path, PATH_MAX, "/proc/%s/fd/%s", pid, d_name);

		if (does_path_links_to(path, device_file_path)) {
			result = 1;
			break;
		}
	}

	closedir(fd_dir);
	return result;
}

static int device_has_master(const char *device_file_path)
{
	pid_t myself = getpid();
	DIR *proc_dir;
	struct dirent *proc_entry;
	int result = 0;

	proc_dir = opendir("/proc");
	if (proc_dir == NULL)
		return 0;

	while ((proc_entry = readdir(proc_dir)) != NULL) {
		char *d_name = proc_entry->d_name;

		if (d_name[0] < '0'
		    || d_name[0] > '9'
		    || myself == atoi(d_name)) {
			continue;
		}

		if (process_opened_files(d_name, device_file_path)) {
			result = 1;
			break;
		}

		if (process_opened_device(d_name, device_file_path)) {
			result = 1;
			break;
		}
	}

	closedir(proc_dir);
	return result;
}

static void wait_for_master(const char *device_path)
{
	const unsigned int TOTAL_WAIT_US = 5000000L;
	const unsigned int SLEEP_INTERVAL_US = 100000L;

	unsigned int cnt = TOTAL_WAIT_US / SLEEP_INTERVAL_US;

	int has_master = 0;

	while ((has_master = device_has_master(device_path)) == 0 && cnt--)
		usleep(SLEEP_INTERVAL_US);

	if (!has_master)
		evdi_log("Wait for master timed out");
}

static int wait_for_device(const char *device_path)
{
	const unsigned int TOTAL_WAIT_US = 5000000L;
	const unsigned int SLEEP_INTERVAL_US = 100000L;

	unsigned int cnt = TOTAL_WAIT_US / SLEEP_INTERVAL_US;

	int fd = 0;

	while ((fd = open(device_path, O_RDWR)) < 0 && cnt--)
		usleep(SLEEP_INTERVAL_US);

	if (fd < 0)
		evdi_log("Failed to open a device: %s", strerror(errno));
	return fd;
}

static int open_device(int device)
{
	char dev[PATH_MAX] = "";
	int fd = 0;

	snprintf(dev, PATH_MAX, "/dev/dri/card%d", device);

#ifndef CHROMEOS
	wait_for_master(dev);
#endif

	fd = wait_for_device(dev);

	if (fd >= 0) {
		const int err = ioctl(fd, DRM_IOCTL_DROP_MASTER, NULL);

		if (err == 0)
			evdi_log("Dropped master on %s", dev);
	}

	return fd;
}

// ********************* Public part **************************

evdi_handle evdi_open(int device)
{
	int fd;
	evdi_handle h = EVDI_INVALID_HANDLE;

	fd = open_device(device);
	if (fd > 0) {
		if (is_evdi(fd) && is_evdi_compatible(fd)) {
			h = calloc(1, sizeof(struct evdi_device_context));
			if (h) {
				h->fd = fd;
				h->device_index = device;
			}
		}
		if (h == EVDI_INVALID_HANDLE)
			close(fd);
	}
	return h;
}

enum evdi_device_status evdi_check_device(int device)
{
	struct dirent *fd_entry;
	DIR *fd_dir;
	enum evdi_device_status status = UNRECOGNIZED;
	char path[PATH_MAX];

	if (!device_exists(device))
		return NOT_PRESENT;

	fd_dir = opendir("/sys/devices/platform");
	if (fd_dir == NULL) {
		evdi_log("Failed to list platform devices");
		return NOT_PRESENT;
	}

	while ((fd_entry = readdir(fd_dir)) != NULL) {
		if (strncmp(fd_entry->d_name, "evdi", 4) != 0)
			continue;

		snprintf(path, PATH_MAX,
			"/sys/devices/platform/%s/drm/card%d",
			fd_entry->d_name,
			device);
		if (path_exists(path)) {
			status = AVAILABLE;
			break;
		}
	}

	closedir(fd_dir);
	return status;
}

int evdi_add_device(void)
{
	FILE *add_devices = fopen("/sys/devices/evdi/add", "w");
	int written = 0;

	if (add_devices != NULL) {
		static const char devices_to_add[] = "1";
		const size_t elem_bytes = 1;

		written = fwrite(devices_to_add,
				 elem_bytes,
				 sizeof(devices_to_add),
				 add_devices);
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
		  const unsigned char *edid,
		  const unsigned int edid_length,
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

void evdi_grab_pixels(evdi_handle handle,
		      struct evdi_rect *rects,
		      int *num_rects)
{
	struct drm_clip_rect kernelDirts[MAX_DIRTS] = { { 0, 0, 0, 0 } };
	struct evdi_frame_buffer_node *destinationNode = NULL;
	struct evdi_buffer *destinationBuffer = NULL;

	destinationNode = findBuffer(handle, handle->bufferToUpdate);

	if (!destinationNode) {
		evdi_log("Buffer %d not found. Not grabbing.",
			 handle->bufferToUpdate);
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

	if (do_ioctl(
		handle->fd, DRM_IOCTL_EVDI_GRABPIX, &grab, "grabpix") == 0) {
		/*
		 * Buffer was filled by ioctl
		 * now we only have to fill the dirty rects
		 */
		int r = 0;

		for (; r < grab.num_rects; ++r) {
			rects[r].x1 = kernelDirts[r].x1;
			rects[r].y1 = kernelDirts[r].y1;
			rects[r].x2 = kernelDirts[r].x2;
			rects[r].y2 = kernelDirts[r].y2;
		}

		*num_rects = grab.num_rects;
	} else {
		int id = destinationBuffer->id;

		evdi_log("Grabbing pixels for buffer %d failed.", id);
		evdi_log("Ignore if caused by change of mode.");
		*num_rects = 0;
	}
}

void evdi_register_buffer(evdi_handle handle, struct evdi_buffer buffer)
{
	assert(handle);
	assert(!findBuffer(handle, buffer.id));

	addFrameBuffer(handle, &buffer);
}

void evdi_unregister_buffer(evdi_handle handle, int bufferId)
{
	struct evdi_buffer *bufferToRemove = NULL;

	assert(handle);

	bufferToRemove = &findBuffer(handle, bufferId)->frame_buffer;
	assert(bufferToRemove);

	removeFrameBuffer(handle, &bufferId);
}

bool evdi_request_update(evdi_handle handle, int bufferId)
{
	assert(handle);
	handle->bufferToUpdate = bufferId;
	{
		struct drm_evdi_request_update cmd;
		const int requestResult = do_ioctl(
			handle->fd,
			DRM_IOCTL_EVDI_REQUEST_UPDATE,
			&cmd,
			"request_update");
		const bool grabImmediately = requestResult == 1;

		return grabImmediately;
	}
}

static struct evdi_mode to_evdi_mode(struct drm_evdi_event_mode_changed *event)
{
	struct evdi_mode mode;

	mode.width = event->hdisplay;
	mode.height = event->vdisplay;
	mode.refresh_rate = event->vrefresh;
	mode.bits_per_pixel = event->bits_per_pixel;
	mode.pixel_format = event->pixel_format;

	return mode;
}

static uint64_t evdi_get_dumb_offset(evdi_handle ehandle, uint32_t handle)
{
	struct drm_mode_map_dumb map_dumb = { 0 };

	map_dumb.handle = handle;
	do_ioctl(ehandle->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb,
		 "DRM_MODE_MAP_DUMB");

	return map_dumb.offset;
}

static struct evdi_cursor_set to_evdi_cursor_set(
		evdi_handle handle, struct drm_evdi_event_cursor_set *event)
{
	struct evdi_cursor_set cursor_set;

	cursor_set.hot_x = event->hot_x;
	cursor_set.hot_y = event->hot_y;
	cursor_set.width =  event->width;
	cursor_set.height = event->height;
	cursor_set.enabled = event->enabled;
	cursor_set.buffer_length = event->buffer_length;
	cursor_set.buffer = NULL;
	cursor_set.pixel_format = event->pixel_format;
	cursor_set.stride = event->stride;

	if (event->enabled) {
		size_t size = event->buffer_length;
		uint64_t offset =
			evdi_get_dumb_offset(handle, event->buffer_handle);
		void *ptr = mmap(0, size, PROT_READ,
				 MAP_SHARED, handle->fd, offset);

		if (ptr != MAP_FAILED) {
			cursor_set.buffer = malloc(size);
			memcpy(cursor_set.buffer, ptr, size);
			munmap(ptr, size);
		}
	}

	return cursor_set;
}

static struct evdi_cursor_move to_evdi_cursor_move(
		struct drm_evdi_event_cursor_move *event)
{
	struct evdi_cursor_move cursor_move;

	cursor_move.x = event->x;
	cursor_move.y = event->y;

	return cursor_move;
}

static void evdi_handle_event(evdi_handle handle,
			      struct evdi_event_context *evtctx,
			      struct drm_event *e)
{
	switch (e->type) {
	case DRM_EVDI_EVENT_UPDATE_READY:
		if (evtctx->update_ready_handler)
			evtctx->update_ready_handler(handle->bufferToUpdate,
						     evtctx->user_data);
		break;

	case DRM_EVDI_EVENT_DPMS:
		if (evtctx->dpms_handler) {
			struct drm_evdi_event_dpms *event =
				(struct drm_evdi_event_dpms *) e;

			evtctx->dpms_handler(event->mode,
					     evtctx->user_data);
		}
		break;

	case DRM_EVDI_EVENT_MODE_CHANGED:
		if (evtctx->mode_changed_handler) {
			struct drm_evdi_event_mode_changed *event =
				(struct drm_evdi_event_mode_changed *) e;

			evtctx->mode_changed_handler(to_evdi_mode(event),
						     evtctx->user_data);
		}
		break;

	case DRM_EVDI_EVENT_CRTC_STATE:
		if (evtctx->crtc_state_handler) {
			struct drm_evdi_event_crtc_state *event =
				(struct drm_evdi_event_crtc_state *) e;

			evtctx->crtc_state_handler(event->state,
						   evtctx->user_data);
		}
		break;

	case DRM_EVDI_EVENT_CURSOR_SET:
		if (evtctx->cursor_set_handler) {
			struct drm_evdi_event_cursor_set *event =
				(struct drm_evdi_event_cursor_set *) e;

			evtctx->cursor_set_handler(to_evdi_cursor_set(handle,
								      event),
						   evtctx->user_data);
		}
		break;

	case DRM_EVDI_EVENT_CURSOR_MOVE:
		if (evtctx->cursor_move_handler) {
			struct drm_evdi_event_cursor_move *event =
				(struct drm_evdi_event_cursor_move *) e;

			evtctx->cursor_move_handler(to_evdi_cursor_move(event),
						    evtctx->user_data);
		}
		break;

	default:
		evdi_log("Warning: Unhandled event");
	}
}

void evdi_handle_events(evdi_handle handle, struct evdi_event_context *evtctx)
{
	char buffer[1024];
	int i = 0;

	int bytesRead = read(handle->fd, buffer, sizeof(buffer));

	if (!evtctx) {
		evdi_log("Error: Event context is null!");
		return;
	}

	while (i < bytesRead) {
		struct drm_event *e = (struct drm_event *) &buffer[i];

		evdi_handle_event(handle, evtctx, e);

		i += e->length;
	}
}

evdi_selectable evdi_get_event_ready(evdi_handle handle)
{
	return handle->fd;
}

void evdi_get_lib_version(struct evdi_lib_version *version)
{
	if (version != NULL) {
		version->version_major = LIBEVDI_VERSION_MAJOR;
		version->version_minor = LIBEVDI_VERSION_MINOR;
		version->version_patchlevel = LIBEVDI_VERSION_PATCHLEVEL;
	}
}

void evdi_set_logging(struct evdi_logging evdi_logging)
{
	g_evdi_logging = evdi_logging;
}
