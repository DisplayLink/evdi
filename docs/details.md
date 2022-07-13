[TOC]

# API Details

## Functions by group

#### Versioning
    #!c
    evdi_get_lib_version(struct evdi_lib_version device);

Function returns library version.
It uses semantic versioning to mark compatibility changes.
Version consists of 3 components formatted as MAJOR.MINOR.PATCH

 * `MAJOR` number is changed for incompatibile API changes
 * `MINOR` number is changed for backwards-compatible changes
 * `PATCH` number is changed for backwards-compatibile bug fixes

#### Module parameters

User can modify driver behaviour by its parameters that can be set at module load time or changed during runtime.

 * `initial_device_count` Number of evdi devices added at module load time (default: 0)


### EVDI nodes

#### Finding an available EVDI node to use
    #!c
    evdi_device_status evdi_check_device(int device);

Use this function to check if a particular `/dev/dri/cardX` is EVDI or not.

**Arguments:** `device` is a number of card to check, e.g. passing `1` will mean `/dev/dri/card1`.

**Return value:**

* `AVAILABLE` if the device node is EVDI and is available to use.
* `UNRECOGNIZED` when a node has not been created by EVDI kernel module.
* `NOT_PRESENT` in other cases, e.g. when the device does not exist or cannot be opened to check.

#### Adding new EVDI node (pre v1.9.0)
    #!c
	int evdi_add_device()

Use this to tell the kernel module to create a new `cardX` node for your application to use.

**Return value:**
`1` when successful, `0` otherwise.

#### Opening device nodes (pre v1.9.0)
    #!c
	evdi_handle evdi_open(int device);

This function attempts to open a DRM device node with given number as EVDI.
Function performs compatibility check with underlying drm device. If version of the
library and module does not match then the device will not be opened.

**Arguments**: `device` is a number of card to open, e.g. `1` means `/dev/dri/card1`.

**Return value:** On success, a handle to the opened device to be used in following API calls. `EVDI_INVALID_HANDLE` otherwise.

#### Request evdi nodes (since v1.9.0)
    #!c
	evdi_handle evdi_open_attached_to(char *sysfs_parent_device);

This function attempts to add (if necessary) and open a DRM device node attached to given parent device.
Linking with another sysfs device is sometimes useful if it is required to reflect such relationship in sysfs.

The function performs a compatibility check with an underlying drm device. If version of the
library and module does not match, the device will not be opened.

**Arguments**: `sysfs_parent_device` is a string with the following format: `usb:[busNum]-[portNum1].[portNum2].[portNum3]...`, which describes the
device that evdi is linked to. Or `NULL` when evdi device node is not linked with any other device.

**Return value:** On success, a handle to the opened device to be used in following API calls. `EVDI_INVALID_HANDLE` otherwise.



#### Closing devices

    #!c
	void evdi_close(evdi_handle handle);

Closes an opened EVDI handle.

**Arguments**: `handle` to an opened device that is to be closed.

### Connection
#### Opening connections

    #!c
	void evdi_connect(evdi_handle handle,
			  const unsigned char* edid,
			  const unsigned edid_length,
			  const uint32_t pixel_area_limit,
			  const uint32_t pixel_per_second_limit);

Creates a connection between the EVDI and Linux DRM subsystem, resulting in kernel mode driver processing a hot plug event.

**Arguments**:

* `handle` to an opened device
* `edid` should be a pointer to a memory block with contents of an EDID of a monitor that will be exposed to kernel
* `edid_length` is the length of the EDID block (typically 512 bytes, or more if extension blocks are present)
* `pixel_area_limit` is the maximum pixel count (width x height) a connected device can handle
* `pixel_per_second_limit` is the maximum pixel per second rate (width x height x frames per second) a connected device can handle

#### Disconnecting

    #!c
	void evdi_disconnect(evdi_handle handle)

Breaks the connection between the device handle and DRM subsystem - resulting in an unplug event being processed.

**Arguments**: `handle` to an opened device.

### Buffers

Managing memory for frame buffers is left to the client applications.
The `evdi_buffer` structure is used to let the library know details about the frame buffer your application is working with.
For more details, see [struct evdi_buffer](details.md#evdi_buffer) description.

#### Registering
    #!c
	void evdi_register_buffer(evdi_handle handle, evdi_buffer buffer);

This function allows to register a `buffer` of type `evdi_buffer` with an opened EVDI device `handle`.

!!! warning
	Registering a buffer does not allocate memory for the frame.

#### Unregistering

    #!c
	void evdi_unregister_buffer(evdi_handle handle, int bufferId);

This function unregisters a buffer with a given `bufferId` from an opened EVDI device `handle`.

!!! warning
	Unregistering a buffer does not deallocate memory for the frame.


### Screen updates

#### Requesting an update
    #!c
	bool evdi_request_update(evdi_handle handle, int bufferId);

Requests an update for a buffer with a given `bufferId`. The buffer must be already registered with the library.

**Arguments**:

* `handle` to an opened device.
* `bufferId` is an identifier for a buffer that should be updated.

**Return value:**

The function can return `true` if the data for the buffer is ready to be grabbed immediately after the call.
If `false` is returned, then an update is not yet ready to grab and the application should wait until it gets
notified by the kernel module - see [Events and handlers](details.md#events-and-handlers).

#### Grabbing pixels

    #!c
	void evdi_grab_pixels(evdi_handle handle, evdi_rect *rects, int *num_rects);

Grabs pixels following the most recent update request (see [Requesting an update](details.md#requesting-an-update)).

This should be called either after a call to `evdi_request_update` (if it returns `true` which means pixels can be grabbed immediately),
or while handling the `update_ready` notification.

**Arguments**:

* `handle` to an opened device.
* `rects` is a pointer to the first `evdi_rect` that the library fills, based on what the kernel tells.

!!! note
    It is expected that this pointer is a beginning of an array of `evdi_rect`s, and current implementation assumes
    the array does not contain more than 16 slots for rects.

* `num_rects` is a pointer to an integer that will be modified to tell how many dirty rectangles are valid in the list,
   and the client should only care about as many. In particular, a failed grab will be indicated by `0` valid rectangles
   to take into account (this can happen when there was a mode change between the request and the grab).

### DDC/CI response

    #!c
	void evdi_ddcci_response(evdi_handle handle, const unsigned char *buffer,
		const uint32_t buffer_length, const bool result);

Pass back DDC/CI data following the most recent DDC/CI request to the EVDI kernel driver (see [DDC/CI data notification](details.md#ddcci-data-notification)).

**Arguments**:

* `handle` to an opened device.
* `buffer` a pointer to the response buffer. This will be copied into kernel space.
* `buffer_length` the length of the response buffer.
* `result` the boolean result. The caller should set `result` to true if the most recent DDC/CI request was successful and false if it was unsuccessful.  If false, `buffer` and `buffer_length` are ignored.

!!! note
	The `buffer_length` will be truncated to 64 bytes (`DDCCI_BUFFER_SIZE`).

### Events and handlers

#### DPMS mode change

    #!c
	void (*dpms_handler)(int dpms_mode, void* user_data);

This notification is sent when a DPMS mode changes.
The possible modes are as defined by the standard, and values are bit-compatible with DRM interface:

```
/* DPMS flags */
#define DRM_MODE_DPMS_ON        0
#define DRM_MODE_DPMS_STANDBY   1
#define DRM_MODE_DPMS_SUSPEND   2
#define DRM_MODE_DPMS_OFF       3
```

*[DPMS]: Display Power Management Signaling

#### Mode change notification

    #!c
	void (*mode_changed_handler)(evdi_mode mode, void* user_data);

This notification is sent when a display mode changes. Details of the new mode are sent in the `mode` argument.
See [evdi_mode](details.md#evdi_mode) for description of the structure.

#### Update ready notification

	#!c
	void (*update_ready_handler)(int buffer_to_be_updated, void* user_data);

This notification is sent when an update for a buffer, that had been earlier requested is ready to be consumed.
The buffer number to be updated is `buffer_to_be_updated`.

#### Cursor change notification

	#!c
	void (*cursor_set_handler)(struct evdi_cursor_set cursor_set, void* user_data);

This notification is sent for an update of cursor buffer or shape. It is also raised when cursor is enabled or disabled.
Such situation happens when cursor is moved on and off the screen respectively.

#### Cursor move notification

	#!c
	void (*cursor_move_handler)(struct evdi_cursor_move cursor_move, void* user_data);

This notification is sent for a cursor position change. It is raised only when cursor is positioned on virtual screen.

#### CRTC state change

	#!c
	void (*crtc_state_handler)(int state, void* user_data);

This event is deprecated. Please use DPMS mode change event instead.
Sent when DRM's CRTC changes state. The `state` is a value that's forwarded from the kernel.

#### DDC/CI data notification

	#!c
	void (*ddcci_data_handler)(struct evdi_ddcci_data ddcci_data, void *user_data);

This notification is sent when an i2c request has been made to the DDC/CI address (0x37).

The module will wait for a maximum of DDCCI_TIMEOUT_MS (50ms - The default DDC request timeout) for a response to this request to be passed back via `evdi_ddcci_response`.

### Logging

Client can register their own callback to be used for logging instead of default `printf`.

	#!c
	void evdi_set_logging(struct evdi_logging evdi_logging);

For more on argument see [struct evdi_logging](details.md#Types).

## Types

### evdi_handle

This is a handle to an opened device node that you get from an `evdi_open` call,
and use in all following API calls to indicate which EVDI device you communicate with.

### evdi_selectable

A typedef denoting a file descriptor you can watch to know when there are events being signalled from the kernel module.
Each opened EVDI device handle has its own descriptor to watch, which you can get with `evdi_get_event_ready`.
When the descriptor becomes ready to read from, the application should call `evdi_handle_events` to dispatch notifications to its handlers.

### evdi_device_status

An enumerated type used while finding the DRM device node that is EVDI. Possible values are `AVAILABLE`, `UNRECOGNIZED` and `NOT_PRESENT`.

### evdi_rect

A simple structure used by the library to represent a rectangular area of a screen. Top left coordinates of the rectangle are `x1` and `y1`,
bottom right are `x2` and `y2`.

### evdi_mode

A structure used to describe a video mode that's set for a display. Contains details of resolution set (`width`, `height`), refresh rate (`refresh_rate`),
and details of a pixel format used to encode color value (`bits_per_pixel` and `pixel_format` - which are forwarded from kernel's DRM).

### evdi_buffer

A structure holding details about a buffer.

    #!c
	typedef struct {
		int id;
		void* buffer;
		int width;
		int height;
		int stride;

		evdi_rect* rects;
		int rect_count;
	} evdi_buffer;

Buffers have IDs, which can be arbitrarily chosen integer numbers - but typically a simple sequence of numbers starting
from `0` is used in client applications. The pointer to the beginning of an already allocated memory block should be assigned
to the `buffer` member of the structure. This memory will be filled by the kernel module when handling requests to grab pixels.

`width`, `height` and `stride` are properties of the buffer - the first two indicate what the size of the frame is,
and `stride` is a width stride - tells what is the increment in bytes between data for lines in memory.

Stride can be equal to width of a single line multiplied by the number of bytes necessary for encoding color value for one pixel (e.g. 4 for RGB32) if the data for lines are contigous in the memory,
but you can use larger value to indicate extra space/padding between them, e.g. oftentimes an additional requirement for the value of stride is it being divisbile by 8;
note that those values might be specific to particular hardware/graphic drivers.
Please consult documentation of your GPU for details.

Last two structure members, `rects` and `rect_counts` are updated during grabbing pixels to inform about the number and coordinates of areas that are changed from the last update.

### evdi_event_context

    #!c
	typedef struct {
	  void (*dpms_handler)(int dpms_mode, void* user_data);
	  void (*mode_changed_handler)(evdi_mode mode, void* user_data);
	  void (*update_ready_handler)(int buffer_to_be_updated, void* user_data);
	  void (*crtc_state_handler)(int state, void* user_data);
	  void (*cursor_set_handler)(struct evdi_cursor_set cursor_set, void *user_data);
	  void (*cursor_move_handler)(struct evdi_cursor_move cursor_move, void *user_data);
	  void (*ddcci_data_handler)(struct evdi_ddcci_data ddcci_data, void *user_data);
	  void* user_data;
	} evdi_event_context;

The `evdi_device_context` structure is used for holding pointers to handlers for all notifications that the application may receive from
the kernel module. The `user_data` member is a value that the library will use while dispatching the call back.
See [Events and handlers](details.md#events-and-handlers) for more information.

### evdi_lib_version

    #!c
	struct evdi_lib_version {
		int version_major;
		int version_minor;
		int version_patchlevel;
	};

The `evdi_lib_version` structure contains libevdi version.
Version can be used to check compatibility between library and a client application.

### evdi_cursor_set

    #!c
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

The `evdi_cursor_set` structure contains cursor state information. `hot_x` and `hot_y` define hotspot information.
`enabled` parameter is true when cursor bitmap is available and cursor is visible on virtual display.
Parameters `width` and `height` define size of the cursor bitmap stored in a `buffer` memory area of size `buffer_length`.

!!! warning
	Event handler or library user has to free buffer memory when it is not using it.

Remaining `stride` and `pixel_format` describe data organization in the buffer. `stride` is a size of a single line in a buffer.
Usually it is width of the cursor multiplied by bytes per pixel value plus additional extra padding. It ensures proper alignment of subsequent pixel rows.
Pixel encoding is described by FourCC code in `pixel_format` field.

### evdi_cursor_move

    #!c
	struct evdi_cursor_move {
		int32_t x;
		int32_t y;
	};

The `evdi_cursor_move` structure contains current cursor position.
It is defined as top left corner of the cursor bitmap.

### evdi_ddcci_data

    #!c
	struct evdi_ddcci_data {
		uint16_t address;
		uint16_t flags;
		uint32_t buffer_length;
		uint8_t *buffer;
	};

The `evdi_ddcci_data` structure contains:

* `address` i2c address, will always be 0x37.
* `flags` read/write flags.  Read = 1, Write = 0.
* `buffer_length` the length of the buffer.
* `buffer` pointer to the ddc/ci buffer.  For both read and write this will be truncated to 64 bytes (`DDCCI_BUFFER_SIZE`).

!!! warning
	Although the DDC spec advices the maximum buffer length is 32 bytes, we have identified monitors which support bigger buffers.

### evdi_logging

	#!c
	struct evdi_logging {
		void (*function)(void *user_data, const char *fmt, ...);
		void *user_data;
	};

Structure contains two fields:

* `function` which is a pointer to the actual callback. The `fmt` and `...` are the same as in case of `printf`.
* `user_data` a pointer provided by the client when registering callback

!!! note
    By setting `function` to NULL libevdi will switch to default behaviour of using `printf`.


