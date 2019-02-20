[TOC]

# Quick Start

This section explains how to write a basic client for EVDI. Details of API calls are ommitted here for brevity.

# Typical application

Applications using EVDI will typically:

* find a free EVDI node, or add a new node if none was found; then open it
* connect to the EVDI node, letting the DRM subsystem know what is the monitor that the application drives
* allocate memory for, and register buffer(s) that will be used to receive screen updates
* request and consume updates and other notifications in a loop

## EVDI nodes

EVDI reuses DRM subsystem's `cardX` nodes for passing messages between the kernel and userspace.
In order to distinguish non-EVDI nodes from a node that's created by EVDI kernel module, `evdi_check_device` function should be used.

The library only allows to connect to DRM nodes that are created by EVDI.
Attempts to connect to other nodes (e.g. related to a built-in GPU) will fail.

!!! note
    Using EVDI nodes currently requires administrative rights, so applications must be run with `sudo`, or by root.

### Adding new nodes

In order to create a new EVDI `cardX` node, call `evdi_add_device` function.
A single call adds one additional DRM card node that can later be used to connect to.

At the moment, every extra screen that you want to manage needs a separate node.

### Opening and closing EVDI node

Once an available EVDI node is identified, your application should call `evdi_open`, passing a number of `cardX` that you want to open.
This returns an `evdi_handle` that you will use for following API calls, or `EVDI_INVALID_HANDLE` if opening failed.

In order to close the handle, call `evdi_close`.

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

Allocated memory is made available for EVDI library to use by calling `evdi_register_buffer`. Symetrically, `evdi_unregister_buffer` is used to tell the library not to use the buffer anymore.

## Running loop

After registering buffers, the application should start requesting updates for them. This is done using `evdi_request_update`.
You should call it when you intend to consume pixels for the screen.

Once the request to update buffer is handled by the kernel module, you can use `evdi_grab_pixels` to get the data in your app.
This also includes finding out which areas of the buffer are in fact modified, compared to a previous update.

## Events and notifications

Due to its design and split of responsibilities between the kernel and userspace code, EVDI's working model is an asynchronous.
Therefore, your application should monitor a file descriptor exposed by `evdi_get_event_ready` function, and once it becomes ready to read,
call `evdi_handle_events` to dispatch events that are being signalled to the right handlers.

The handlers are defined in your application and are shared with the library through a `evdi_event_context` structure that `evdi_handle_events` uses for dispatching the call.

### Types of events

The notifications your application can (and should) be handling, are:

* Update ready notification (sent once a request to update a buffer is handled by kernel)
* Mode changed notification (sent from DRM after screen mode is changed)
* DPMS notifications (telling the new power state of a connector)
* CRTC state change event (exposing DRM CRTC state)

You will start receiving first notifications from the kernel module right after connecting to EVDI.
Your application should use this information before you ask for screen updates to make sure the buffers are the right size.