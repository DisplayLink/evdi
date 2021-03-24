[TOC]

# Quick Start

This section explains how to write a basic client for EVDI. Details of API calls are omitted here for brevity.

# Typical application

Applications using EVDI will typically:

* find a free EVDI node, or add a new node if none was found; then open it
* connect to the EVDI node, letting the DRM subsystem know what is the monitor that the application drives
* allocate memory for, and register buffer(s) that will be used to receive screen updates
* request and consume updates and other notifications in a loop whenever the kernel [signals updates are ready](details.md#evdi_selectable)

## EVDI nodes

EVDI reuses DRM subsystem's `cardX` nodes for passing messages between the kernel and userspace.
In order to distinguish non-EVDI nodes from a node that's created by EVDI kernel module, `evdi_check_device` function should be used.

The library only allows to connect to DRM nodes that are created by EVDI.
Attempts to connect to other nodes (e.g. related to a built-in GPU) will fail.

### Adding new nodes (pre v1.9.0)

!!! note
    Requires administrative rights. To call this your application needs to
    have been run with `sudo`, or by root.

In order to create a new EVDI `cardX` node, call `evdi_add_device` function.
A single call adds one additional DRM card node that can later be used to connect to.

At the moment, every extra screen that you want to manage needs a separate node.

### Opening EVDI node (pre v1.9.0)

Once an available EVDI node is identified, your application should call `evdi_open`, passing a number of `cardX` that you want to open.
This returns an `evdi_handle` that you will use for following API calls, or `EVDI_INVALID_HANDLE` if opening failed.

### Requesting EVDI node (since v1.9.0)

!!! note
    Requires administrative rights. To call this your application needs to
    have been run with `sudo`, or by root.

Adding and opening evdi devices is easier since libevdi v1.9.0. It's sufficient to call `evdi_open_attached_to(NULL)` in order to add a new evdi node and open it.

It is possible to bind evdi devices with usb devices if it is necessary to show such relationship in sysfs.
It is done via `const char *sysfs_parent_device` parameter of `evdi_open_attached_to` function.
USB parent device is described by a string with the following format: `usb:[busNum]-[portNum1].[portNum2].[portNum3]...`

e.g.
A `evdi_open_attached_to("usb:2-2.1")` call will link `/sys/bus/usb/devices/2-2.1/evdi.0` to
`/sys/bus/platform/devices/evdi.0` which is the first available evdi node.

If an available device exists calling this does not require administrative
rights. Otherwise, administrative rights are needed to create a new device.
You can ensure a device is available by
[configuring the kernel module](details.md#module-parameters) to create devices
when it is loaded.

### Closing EVDI node

In order to close the handle, call `evdi_close`.

### Removing EVDI nodes

!!! note
    Requires administrative rights. To write to this file your application
    needs to have been run with `sudo`, or by root.

Write to `/sys/devices/evdi/remove_all`. For example:

```bash
echo 1 | sudo tee /sys/devices/evdi/remove_all
```

## Connecting and disconnecting

Connecting to EVDI tells DRM subsystem that there is a monitor connected, and from this moment the system is aware of an extra display.
Connection also lets DRM know what is the [EDID](https://en.wikipedia.org/wiki/Extended_Display_Identification_Data) of the monitor that a particular EVDI node handles.
Think of this as something similar to plugging a monitor with a cable to a port of a graphics card.

Similarly, disconnecting indicates that the display is no longer there - like physically pulling cable out from the graphics adapter port.

To connect or disconnect, use `evdi_connect` and `evdi_disconnect`, respectively.

## Frame buffers

To know what the contents of the screen is, your application will use a block of memory that it can read pixel data from.

The library itself does _not_ allocate any memory for buffers - this is to allow more control in the client application.
Therefore, before you request screen updates for the screens you're managing, an appropriate amount of memory must be allocated to hold screen data within your application.
The application can register as many buffers as you like, and subsequent update requests can refer to any buffer that was previously registered.

Allocated memory is made available for EVDI library to use by calling `evdi_register_buffer`. Symmetrically, `evdi_unregister_buffer` is used to tell the library not to use the buffer anymore.

## Cursor

Mouse cursor is an important part of the desktop. Because of this, evdi provides special control over it.

There are two ways to handle cursor:

 * Automatic cursor compositing on framebuffer(default). Every cursor change causes `update_ready` event to be raised. In the following grab pixels operation evdi will compose cursor
on the user supplied framebuffer.
 * Cursor change notifications. Controlled with `evdi_enable_cursor_events` function call.
In that mode the responsibility for cursor blending is passed to the library client. Instead of `update_ready` event the `cursor_set` and `cursor_move` notifications are sent.

## DDC/CI

As part of creating an EVDI node, the module also creates an i2c adapter. This can be used to pass DDC/CI buffers to and from the connected monitor to adjust brightness and contrast.
Data requests to this adapter for DDC/CI (on address 0x37) are passed to userspace as DDC/CI data notifications via `ddcci_data_handler` and responses are passed back using `evdi_ddcci_response`.

## Running loop

You are expected to promptly handle events and to
[request updates](details.md#requesting-an-update) and
[grab pixels](details.md#grabbing-pixels) regularly for any virtual monitor you
have connected. If you fail to do so the device may become unresponsive.

## Events and notifications

Due to its design and split of responsibilities between the kernel and userspace code, EVDI's working model is an asynchronous one.
Therefore, your application should monitor a file descriptor exposed by `evdi_get_event_ready` function, and whenever it becomes ready to read,
call `evdi_handle_events` to dispatch events that are being signalled to the right handlers.

The handlers are defined in your application and are shared with the library through a `evdi_event_context` structure that `evdi_handle_events` uses for dispatching the call.

### Types of events

The notifications your application can (and should) be handling, are:

* Update ready notification (sent once a request to update a buffer is handled by kernel)
* Mode changed notification (sent from DRM after screen mode is changed)
* DPMS notifications (telling the new power state of a connector)
* CRTC state change event (exposing DRM CRTC state)
* Cursor events (send when cursor position or state changes)
* DDC/CI notification (sent when an i2c request for DDC/CI data is made)

You will start receiving first notifications from the kernel module right after connecting to EVDI.
Your application should use this information before you ask for screen updates to make sure the buffers are the right size.

## Logging

By default libevdi uses `printf` to print messages to stdout. Client application can provide its own callback which will be used instead by calling `evdi_set_logging`.
The same function can be used to switch back to default behaviour (by setting callback to `NULL`);
