# Extensible Virtual Display Interface

## (Use at own risk) Hack-fixes for slow screen update rate on AMD/DisplayLink

Out of pure curiosity I have been investigating the slow screen update rate on AMD laptops when driving DisplayLink screens. 
More details about my findings I shared in this PR: https://github.com/DisplayLink/evdi/pull/282.

Work around #1 (Gnome/Wayland only): https://github.com/DisplayLink/evdi/pull/282

Work-around #2 
This branch. This has been working fairly OK on Debian 11 on both Gnome/Wayland and Gnome/X11 for two evenings now. I do not have other DE's to try. I am using Thinkpad T14s with AMD 4750U with a 4K DisplayLink dock. I think this could only work on AMD APU's. I thought I could share it in case anyone is willing to try it.

About this hack-fix:
Slow screen update rate is related to the slow texture copy in EVDI. The imported dma-buf from AMDGPU is mapped into virtual address space by the AMDGPU driver and it hits quite slow copy path from GPU to CPU buffer (~500MB/s). Since we have backing physical pages we request additional virtual mapping from the kernel directly ignoring AMDGPU driver's mapping. The mapping is only done for the duration of the copy and then it is released immediately. The new mapping gives (10GB/s rate) and it makes the screen usable. This additional mapping is probably not very legal to do, and there could be problems down the line, but it was nice to see it working:)
  


How to use it.
(make sure you know how to revert it in case it does not work)
1. Install latest DisplayLink driver, get your DisplayLink screen to work (although with slow update rate)
2. Clone this repo and checkout this branch: amd_vmap_texture
3. copy files from module dir into /usr/src/evdi* (highest version) dir
4. Run:
   sudo dkms uninstall evdi/version
   sudo dkms unbuild evdi/version
   sudo dkms build evdi/version
   sudo dkms install evdi/version

5. append "vmap_texture=1" to /etc/modprobe/evdi.conf (the content of the file should look like this "options evdi initial_device_count=4 vmap_texture=1")
6. reboot.



[![Build Status](https://travis-ci.org/DisplayLink/evdi.svg?branch=devel)](https://travis-ci.org/DisplayLink/evdi)

The Extensible Virtual Display Interface (EVDI) is a Linux&reg; kernel module that enables management of multiple screens, allowing user-space programs to take control over what happens with the image. It is essentially a virtual display you can add, remove and receive screen updates for, in an application that uses the `libevdi` library.

The project is part of the DisplayLink Ubuntu development which enables support for DisplayLink USB 3.0 devices on Ubuntu. Please note that **this is NOT a complete driver for DisplayLink devices**. For more information and the full driver package, see [DisplayLink Ubuntu driver](http://www.displaylink.com/downloads/ubuntu.php).

This open-source project includes source code for both the `evdi` kernel module and a wrapper `libevdi` library that can be used by applications like DisplayLink's user mode driver to send and receive information from and to the kernel module.

## How to use

See [libevdi API documentation](https://displaylink.github.io/evdi) for details.

EVDI is a driver compatible with a standard Linux DRM subsystem. Due to this, displays can be controlled by standard tools, eg. `xrandr` or display settings applets in graphical environments eg. Unity, Gnome or KDE.

Minimum supported kernel version required is 4.15. DisplayLink have checked the module compiles and works with Ubuntu variants of kernels up to 5.5. Although other vanilla Linux kernel sources are used for Travis CI job, newer kernels, or kernel variants used by other distributions may require extra development. Please see below to see how you can help.

## Future Development

This is a first release. DisplayLink are open to suggestions and feedback on improving the proposed architecture and will gladly review patches or proposals from the developer community. Please find a current list of areas we identify as requiring attention below.

- Compatibility with distributions other than Ubuntu 18.04/20.04 LTS is not verified. Please let us know if you make it work on other distros - pull requests are welcome!
- The communication between the EVDI kernel module and the wrapper libevdi library is not access-controlled or authenticated. This could be improved in future releases, making it harder to compromise the data EVDI is sending and receiving.
- EVDI kernel module driver is currently a platform_driver, for multiple reasons; most importantly because virtual displays are not discoverable, i.e. cannot be enumerated at the hardware level. EVDI is also a generic device, not tied to any particular kind of device, transport layer or a bus.

## Licensing

Please refer to the LICENSE information in `module` and `library` subfolders of this project.

### More information

For more information, see our [support page](http://support.displaylink.com). Visit [displaylink.com](http://displaylink.com) to learn more about DisplayLink technology.

&copy; Copyright 2015-2020 DisplayLink (UK) Ltd.

Linux is a registered trademark of Linus Torvalds in the U.S. and other countries.
