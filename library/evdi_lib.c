// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.

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
#define EVDI_INVALID_DEVICE_INDEX -1

#define EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR 1
#define EVDI_MODULE_COMPATIBILITY_VERSION_MINOR 9
#define EVDI_MODULE_COMPATIBILITY_VERSION_PATCH 0

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

#define EVDI_USAGE_LEN 64
static evdi_handle card_usage[EVDI_USAGE_LEN];

static int drm_ioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	return ret;
}

static int drm_auth_magic(int fd, drm_magic_t magic)
{
	drm_auth_t auth;

	memset(&auth, 0, sizeof(auth));
	auth.magic = magic;
	if (drm_ioctl(fd, DRM_IOCTL_AUTH_MAGIC, &auth))
		return -errno;
	return 0;
}

static int drm_is_master(int fd)
{
	/* Detect master by attempting something that requires master.
	 *
	 * Authenticating magic tokens requires master and 0 is an
	 * internal kernel detail which we could use. Attempting this on
	 * a master fd would fail therefore fail with EINVAL because 0
	 * is invalid.
	 *
	 * A non-master fd will fail with EACCES, as the kernel checks
	 * for master before attempting to do anything else.
	 *
	 * Since we don't want to leak implementation details, use
	 * EACCES.
	 */
	return drm_auth_magic(fd, 0) != -EACCES;
}

static int do_ioctl(int fd, unsigned long request, void *data, const char *msg)
{
	const int err = drm_ioctl(fd, request, data);

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
		 LIBEVDI_VERSION_PATCH);

	if (do_ioctl(fd, DRM_IOCTL_VERSION, &ver, "version") != 0)
		return 0;

	evdi_log("Evdi version (%d.%d.%d)",
		 ver.version_major,
		 ver.version_minor,
		 ver.version_patchlevel);

	if (ver.version_major == EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR &&
	    ver.version_minor >= EVDI_MODULE_COMPATIBILITY_VERSION_MINOR)
		return 1;

	evdi_log("Doesn't match LibEvdi compatibility one (%d.%d.%d)",
		 EVDI_MODULE_COMPATIBILITY_VERSION_MAJOR,
		 EVDI_MODULE_COMPATIBILITY_VERSION_MINOR,
		 EVDI_MODULE_COMPATIBILITY_VERSION_PATCH);

	return 0;
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

static int open_as_slave(const char *device_path)
{
	int fd = 0;
	int err = 0;

	fd = open(device_path, O_RDWR);
	if (fd < 0)
		return -1;

	if (drm_is_master(fd)) {
		evdi_log("Process has master on %s, err: %s",
			 device_path, strerror(errno));
		err = drm_ioctl(fd, DRM_IOCTL_DROP_MASTER, NULL);
	}

	if (err < 0) {
		evdi_log("Drop master on %s failed, err: %s",
			 device_path, strerror(errno));
		close(fd);
		return err;
	}

	if (drm_is_master(fd)) {
		evdi_log("Drop master on %s failed, err: %s",
			 device_path, strerror(errno));
		close(fd);
		return -1;
	}

	evdi_log("Opened %s as slave drm device", device_path);
	return fd;
}

static int wait_for_device(const char *device_path)
{
	const unsigned int TOTAL_WAIT_US = 5000000L;
	const unsigned int SLEEP_INTERVAL_US = 100000L;

	unsigned int cnt = TOTAL_WAIT_US / SLEEP_INTERVAL_US;

	int fd = 0;

	while ((fd = open_as_slave(device_path)) < 0 && cnt--)
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
		const int err = drm_ioctl(fd, DRM_IOCTL_DROP_MASTER, NULL);

		if (err == 0)
			evdi_log("Dropped master on %s", dev);
	}

	return fd;
}


static int write_add_device(const char *buffer, size_t buffer_length)
{
	FILE *add_devices = fopen("/sys/devices/evdi/add", "w");
	int written = 0;

	if (add_devices != NULL) {
		written = fwrite(buffer,
				 1,
				 buffer_length,
				 add_devices);
		fclose(add_devices);
	}

	return written;
}

static int get_drm_device_index(const char *evdi_sysfs_drm_dir)
{
	struct dirent *fd_entry;
	DIR *fd_dir;
	int dev_index = EVDI_INVALID_DEVICE_INDEX;

	fd_dir = opendir(evdi_sysfs_drm_dir);
	if (fd_dir == NULL) {
		evdi_log("Failed to open dir %s", evdi_sysfs_drm_dir);
		return dev_index;
	}

	while ((fd_entry = readdir(fd_dir)) != NULL) {
		if (strncmp(fd_entry->d_name, "card", 4) == 0)
			dev_index = strtol(&fd_entry->d_name[4], NULL, 10);
	}
	closedir(fd_dir);

	return dev_index;
}

static bool is_correct_parent_device(const char *dirname, const char *parent_device)
{
	char link_path[PATH_MAX];

	snprintf(link_path, PATH_MAX - 7, "%s/device", dirname);

	if (parent_device == NULL)
		return access(link_path, F_OK) != 0;

	char link_resolution[PATH_MAX];
	const ssize_t link_resolution_len = readlink(link_path, link_resolution, PATH_MAX);

	if (link_resolution_len == -1 || link_resolution_len == PATH_MAX)
		return false;

	link_resolution[link_resolution_len] = '\0';
	char *parent_device_token = strrchr(link_resolution, '/');

	if (strlen(parent_device) < 2)
		return false;

	parent_device_token++;
	size_t len = strlen(parent_device_token);

	bool is_same_device = strlen(parent_device) == len && strncmp(parent_device_token, parent_device, len) == 0;

	return is_same_device;
}

static int find_unused_card_for(const char *parent_device)
{
	char evdi_platform_root[] = "/sys/bus/platform/devices";
	struct dirent *fd_entry;
	DIR *fd_dir;
	int device_index = EVDI_INVALID_DEVICE_INDEX;

	fd_dir = opendir(evdi_platform_root);
	if (fd_dir == NULL) {
		evdi_log("Failed to open dir %s", evdi_platform_root);
		return device_index;
	}

	while ((fd_entry = readdir(fd_dir)) != NULL) {
		if (strncmp(fd_entry->d_name, "evdi", 4) != 0)
			continue;

		char evdi_path[PATH_MAX];

		snprintf(evdi_path, PATH_MAX, "%s/%s", evdi_platform_root, fd_entry->d_name);

		if (!is_correct_parent_device(evdi_path, parent_device))
			continue;

		char evdi_drm_path[PATH_MAX];

		snprintf(evdi_drm_path, PATH_MAX - strlen(evdi_path), "%s/drm", evdi_path);
		int dev_index = get_drm_device_index(evdi_drm_path);

		assert(dev_index < EVDI_USAGE_LEN && dev_index >= 0);

		if (card_usage[dev_index] == EVDI_INVALID_HANDLE) {
			device_index = dev_index;
			break;
		}
	}
	closedir(fd_dir);

	return device_index;
}

static int get_generic_device(void)
{
	int device_index = EVDI_INVALID_DEVICE_INDEX;

	device_index = find_unused_card_for(NULL);
	if (device_index == EVDI_INVALID_DEVICE_INDEX) {
		evdi_log("Creating card in /sys/devices/platform");
		write_add_device("1", 1);
		device_index = find_unused_card_for(NULL);
	}

	return device_index;
}

static int get_device_attached_to_usb(const char *sysfs_parent_device)
{
	int device_index = EVDI_INVALID_DEVICE_INDEX;
	const char *parent_device = &sysfs_parent_device[4];

	device_index = find_unused_card_for(parent_device);
	if (device_index == EVDI_INVALID_DEVICE_INDEX) {
		evdi_log("Creating card for usb device %s in /sys/bus/platform/devices", parent_device);
		const size_t len = strlen(sysfs_parent_device);

		write_add_device(sysfs_parent_device, len);
		device_index = find_unused_card_for(parent_device);
	}

	return device_index;
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
				card_usage[device] = h;
				evdi_log("Using /dev/dri/card%d", device);
			}
		}
		if (h == EVDI_INVALID_HANDLE)
			close(fd);
	}
	return h;
}

static enum evdi_device_status evdi_device_to_platform(int device, char *path)
{
	struct dirent *fd_entry;
	DIR *fd_dir;
	enum evdi_device_status status = UNRECOGNIZED;
	char card_path[PATH_MAX];

	if (!device_exists(device))
		return NOT_PRESENT;

	fd_dir = opendir("/sys/bus/platform/devices");
	if (fd_dir == NULL) {
		evdi_log("Failed to list platform devices");
		return NOT_PRESENT;
	}

	while ((fd_entry = readdir(fd_dir)) != NULL) {
		if (strncmp(fd_entry->d_name, "evdi", 4) != 0)
			continue;

		snprintf(path, PATH_MAX,
			"/sys/bus/platform/devices/%s", fd_entry->d_name);
		snprintf(card_path, PATH_MAX, "%s/drm/card%d", path, device);
		if (path_exists(card_path)) {
			status = AVAILABLE;
			break;
		}
	}

	closedir(fd_dir);
	return status;
}

enum evdi_device_status evdi_check_device(int device)
{
	char path[PATH_MAX];

	return evdi_device_to_platform(device, path);
}

int evdi_add_device(void)
{
	return write_add_device("1", 1);
}

evdi_handle evdi_open_attached_to(const char *sysfs_parent_device)
{
	int device_index = EVDI_INVALID_DEVICE_INDEX;

	if (sysfs_parent_device == NULL)
		device_index = get_generic_device();

	if (sysfs_parent_device != NULL && strncmp(sysfs_parent_device, "usb:", 4) == 0 && strlen(sysfs_parent_device) > 4)
		device_index = get_device_attached_to_usb(sysfs_parent_device);

	if (device_index >= 0 && device_index < EVDI_USAGE_LEN)  {
		evdi_handle handle = evdi_open(device_index);
		return handle;
	}

	return EVDI_INVALID_HANDLE;
}


void evdi_close(evdi_handle handle)
{
	if (handle != EVDI_INVALID_HANDLE) {
		close(handle->fd);
		free(handle);
		for (int device_index = 0; device_index < EVDI_USAGE_LEN; device_index++) {
			if (card_usage[device_index] == handle) {
				card_usage[device_index] = EVDI_INVALID_HANDLE;
				evdi_log("Marking /dev/dri/card%d as unused", device_index);
			}
		}
	}
}

void evdi_connect(evdi_handle handle,
		  const unsigned char *edid,
		  const unsigned int edid_length,
		  const uint32_t pixel_area_limit,
		  const uint32_t pixel_per_second_limit)
{
	struct drm_evdi_connect cmd = {
		.connected = 1,
		.dev_index = handle->device_index,
		.edid = edid,
		.edid_length = edid_length,
		.pixel_area_limit = pixel_area_limit,
		.pixel_per_second_limit = pixel_per_second_limit,
	};

	do_ioctl(handle->fd, DRM_IOCTL_EVDI_CONNECT, &cmd, "connect");
}

void evdi_disconnect(evdi_handle handle)
{
	struct drm_evdi_connect cmd = { 0, 0, 0, 0, 0, 0 };

	do_ioctl(handle->fd, DRM_IOCTL_EVDI_CONNECT, &cmd, "disconnect");
}

void evdi_enable_cursor_events(evdi_handle handle, bool enable)
{
	struct drm_evdi_enable_cursor_events cmd = {
		.enable = enable,
	};

	evdi_log("%s cursor events on /dev/dri/card%d",
		(enable ? "Enabling" : "Disabling"),
		handle->device_index);

	do_ioctl(handle->fd, DRM_IOCTL_EVDI_ENABLE_CURSOR_EVENTS, &cmd, "enable cursor events");
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

void evdi_ddcci_response(evdi_handle handle, const unsigned char *buffer,
			const uint32_t buffer_length,
			const bool result)
{
	struct drm_evdi_ddcci_response cmd = {
		.buffer = buffer,
		.buffer_length = buffer_length,
		.result = result,
	};

	do_ioctl(handle->fd, DRM_IOCTL_EVDI_DDCCI_RESPONSE, &cmd,
		"ddcci_response");
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
		} else {
			evdi_log("Error: mmap failed with error: %s", strerror(errno));
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

static struct evdi_ddcci_data to_evdi_ddcci_data(
		struct drm_evdi_event_ddcci_data *event)
{
	struct evdi_ddcci_data ddcci_data;

	ddcci_data.address = event->address;
	ddcci_data.flags = event->flags;
	ddcci_data.buffer = &event->buffer[0];
	ddcci_data.buffer_length = event->buffer_length;

	return ddcci_data;
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
			struct evdi_cursor_set cursor_set = to_evdi_cursor_set(handle, event);

			if (cursor_set.enabled && cursor_set.buffer == NULL)
				evdi_log("Error: Cursor buffer is null!");
			else
				evtctx->cursor_set_handler(cursor_set,
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

	case DRM_EVDI_EVENT_DDCCI_DATA:
		if (evtctx->ddcci_data_handler) {
			struct drm_evdi_event_ddcci_data *event =
				(struct drm_evdi_event_ddcci_data *) e;

			evtctx->ddcci_data_handler(to_evdi_ddcci_data(event),
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
		version->version_patchlevel = LIBEVDI_VERSION_PATCH;
	}
}

void evdi_set_logging(struct evdi_logging evdi_logging)
{
	g_evdi_logging = evdi_logging;
}
