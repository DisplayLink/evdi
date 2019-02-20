[TOC]

# API Details

## Functions by group

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

#### Adding new EVDI node
    #!c
	int evdi_add_device()

Use this to tell the kernel module to create a new `cardX` node for your application to use.

**Return value:**
`1` when successful, `0` otherwise.

#### Opening device nodes
    #!c
	evdi_handle evdi_open(int device);

This function attempts to open a DRM device node with given number as EVDI.

**Arguments**: `device` is a number of card to open, e.g. `1` means `/dev/dri/card1`.

**Return value:** an opened device handle to use in following API calls if opening was successful, `EVDI_INVALID_HANDLE` otherwise.

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
			  const uint32_t sku_area_limit);

Creates a connection between the EVDI and Linux DRM subsystem, resulting in kernel mode driver processing a hot plug event.

**Arguments**:

* `handle` to an opened device
* `edid` should be a pointer to a memory block with contents of an EDID of a monitor that will be exposed to kernel
* `edid_length` is the length of the EDID block (typically 512 bytes, or more if extension blocks are present)
* `sku_area_limit` is maximum supported pixel count for connected device

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
* `bufferId` is an indentifier for a buffer that should be updated.

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

### Events and handlers

#### DPMS mode change

    #!c
	void (*dpms_handler)(int dpms_mode, void* user_data);

This notification is sent when a DPMS mode changes.
The possible modes are as defined by the standard, and values are bit-compatible with DRM and Xorg:

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

#### CRTC state change

	#!c
	void (*crtc_state_handler)(int state, void* user_data);

Sent when DRM's CRTC changes state. The `state` is a value that's forwarded from the kernel.

## Types

### evdi_handle

This is a handle to an opened device node that you get from an `evdi_open` call,
and use in all following API calls to indicate which EVDI device you communicate with.

### evdi_selectable

A typedef denoting a file descriptor you can watch to know when there are events being signalled from the kernel module.
Each opened EVDI device handle has its own descriptor to watch.
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
but you can use larger value to indicate extra space/padding between them.

Last two structure members, `rects` and `rect_counts` are updated during grabbing pixels to inform about the number and coordinates of areas that are changed from the last update.

### evdi_event_context

    #!c
	typedef struct {
	  void (*dpms_handler)(int dpms_mode, void* user_data);
	  void (*mode_changed_handler)(evdi_mode mode, void* user_data);
	  void (*update_ready_handler)(int buffer_to_be_updated, void* user_data);
	  void (*crtc_state_handler)(int state, void* user_data);
	  void* user_data;
	} evdi_event_context;

The `evdi_device_context` structure is used for holding pointers to handlers for all notifications that the application may receive from
the kernel module. The `user_data` member is a value that the library will use while dispatching the call back.
See [Events and handlers](details.md#events-and-handlers) for more information.
