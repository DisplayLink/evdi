# evdi kernel module
When installing DisplayLink software using the provided installer or via distro-provided scripts some required configuration is written. When working with the module manually that configuration will not be created automatically. This is likely to cause failure of evdi to discover external DisplayLink devices and lead to much confusion.

See the shell script `dkms_install.sh` and its `evdi_add_mod_options()` function for the steps required.

In summary:

  1. Add module options (nmost importantly `initial_device_count=` )
  2. Auto-load the module

## Module options
Typically `/etc/modprobe.d/evdi.conf` will contain two lines, the second being generated and system-specific:
```
options evdi initial_device_count=4
softdep evdi pre: drm_kms_helper i915
```
Note: `softdep` above shows the host has an integrated Intel GPU (`i915`).

## Auto-loading the module
The  `dkms_install.sh` script assumes the host is using systemd and adds the auto-load config in `/etc/modules-load.d/` that is used by `systemd-modules-load.service`. For hosts without systemd the convention for loading modules at start-up is via an entry in `/etc/modules`. `modules-load.d` usually has a symbolic link to this file.

Adding `evdi` to `/etc/modules` therefore should work for a broader range of possible init system scenarios.

## Usage
It is not obvious that `evdi` (as of v1.14.1) does not auto-detect and configure DisplayLink devices - something one might expect it to do. For that reason the module option `initial_device_count` is required for automatic discovery at load-time.

Alternatively one can manually add outputs with:
```
# echo 1 > /sys/devices/evdi/add
```
and confirm with:
```
$ cat /sys/devivces/evdi/count
```
and when in an Xorg session:
```
$ xrandr --listproviders
Providers: number : 2
Provider 0: id: 0x43 cap: 0x9, Source Output, Sink Offload crtcs: 3 outputs: 2 associated providers: 5 name:modesetting
Provider 1: id: 0x789 cap: 0x2, Sink Output crtcs: 1 outputs: 1 associated providers: 1 name:modesetting
```
Here `Provider 1` is the new output and if things work correctly a new output should appear:
```
$ xrandr --query
...
DVI-I-1-1 disconnected (normal left inverted right x axis y axis)
```
Once a monitor is detected the resolutions will be reported and GUI tools that manage displays should update to show the newly connected display.