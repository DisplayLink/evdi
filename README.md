# Extensible Virtual Display Interface


The Extensible Virtual Display Interface (EVDI) is a Linux&reg; kernel module that enables management of multiple screens, allowing user-space programs to take control over what happens with the image. It is essentially a virtual display you can add, remove and receive screen updates for, in an application that uses the `libevdi` library.

This open-source project includes source code for both the `evdi` kernel module and a wrapper `libevdi` library that can be used by applications like DisplayLink's user mode driver to send and receive information from and to the kernel module.

The `pyevdi` library is a python wrapper for `libevdi`.

## How to use

See [libevdi API documentation](https://displaylink.github.io/evdi) for details.

EVDI is a driver compatible with a standard Linux DRM subsystem. Due to this, displays can be controlled by standard tools, eg. `xrandr` or display settings applets in graphical environments eg. Unity, Gnome or KDE.
Virtual displays can also be created with the help of `pyevdi`.

### Installation and packages

For detailed installation instructions refer to [module/README.md](module/README.md). Minimum supported kernel version required is 4.15. DisplayLink has verified the module compiles and works with Ubuntu variants of kernels up to 6.15. Although other vanilla Linux kernel sources are used for the CI jobs, newer kernels, or kernel variants used by other distributions may require extra development. Please see below to see how you can help.

EVDI is usually combined with the DisplayLink driver, we release it as a deb package or in a form of standalone installer paired with the driver, visit the [DisplayLink page](https://www.synaptics.com/products/displaylink-graphics/downloads/ubuntu) for the latest release. **EVDI is not a complete driver for DisplayLink devices** and will require the driver for the full functionality  .

There is an community driven GitHub project at [DisplayLink RPM](https://github.com/displaylink-rpm/displaylink-rpm) which is generating RPM package for Fedora, CentOS Stream, Rocky Linux and AlmaLinux OS. It uses our code as the basis to create the RPM packages.

There is also an [AUR package](https://aur.archlinux.org/packages/evdi) maintained by the community.

## Contributing

We welcome all contributions. There are many ways you can contribute to the project.

- Submit bugs or feature requests.
- Help us ensure that EVDI works on other distributions than Ubuntu.
- Report issues.
- Help with future development.

### Future development

There are several topics we plan to cover in the future, including:

- The communication between the EVDI kernel module and the wrapper libevdi library is not access-controlled or authenticated. This could be enhanced in future releases, making it more difficult to compromise the data that EVDI sends and receives.
- EVDI kernel module driver is currently a platform_driver, for multiple reasons; most importantly because virtual displays are not discoverable, i.e. cannot be enumerated at the hardware level. EVDI is also a generic device, not tied to any particular kind of device, transport layer or a bus.

## Licensing

Elements of this project are licensed under various licenses. In particular, the `module` and `library` are licensed
under GPL v2 and LGPL v2.1 respectively - consult separate `LICENSE` files in subfolders. Remaining files and subfolders (unless
a separate `LICENSE` file states otherwise) are licensed under MIT license.


&copy; Copyright 2015-2025 DisplayLink (UK) Ltd.

Linux is a registered trademark of Linus Torvalds in the U.S. and other countries.
