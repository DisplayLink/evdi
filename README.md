# Extensible Virtual Display Interface

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
