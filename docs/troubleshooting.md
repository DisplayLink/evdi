# Troubleshooting notes (community)

These notes are for end-users and packagers diagnosing DisplayLink docks that use EVDI. They are not part of the libevdi API.

## Recognizing a DisplayLink dock

```bash
lsusb | grep -iE '17e9|DisplayLink'
```

Example (Dell Universal Dock UD22):

* `413c:f6d0` — Dell Universal Dock UD22
* `17e9:6013` — DisplayLink Dell Universal Hybrid Video Dock

EVDI alone is **not** a complete DisplayLink driver. You also need DisplayLink's userspace driver (`displaylink-driver` / `DisplayLinkManager`).

## Hybrid docks and misleading MST symptoms

Some docks (including Dell UD22) are **hybrid**: USB-C may negotiate DP Alt Mode / MST on the laptop GPU **and** expose DisplayLink over USB.

If DisplayLink userspace / EVDI is missing:

* Monitors may appear as GPU `DP-*` connectors with valid EDIDs
* One panel may be blank; another may flash or only offer 640×480 in the compositor
* Kernel logs may show MST errors (`failed to lookup MSTB`, DPCD read failures)

This is often mistaken for an i915/amdgpu/nvidia bug. Install and start DisplayLink, then look for EVDI connectors (`DVI-I-*` under `/sys/class/drm/`).

## Custom kernels / Pop!_OS

Prebuilt distro packages such as Ubuntu `linux-modules-evdi-*` only match specific kernel ABIs. On custom kernels (for example Pop!_OS / System76 `7.0.x-7607…`), build EVDI via DKMS from the Synaptics package or from this repository.

## Dual monitors, only one EVDI sink

If only one external monitor appears on EVDI:

* Check whether the second panel is still attached to a GPU `DP-*` MST connector
* Try increasing `initial_device_count` (see module parameters)
* Confirm `DisplayLinkManager` is running and both physical dock video ports have signal

## Wayland

Modern DisplayLink + EVDI releases can work on Wayland compositors (verified with Pop!_OS COSMIC). If displays are blank only on Wayland, compare with an X11 session and check compositor multi-GPU / multi-DRM-card support.
