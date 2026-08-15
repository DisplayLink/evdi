#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#
# This file is subject to the terms and conditions of the GNU General Public
# License v2. See the file COPYING in the main directory of this archive for
# more details.
#

# Kernel API differences are detected at build time by conftest.sh (which
# test-compiles probes against the target kernel headers) rather than guessed
# from the distribution. See conftest.sh and the EVDI_HAVE_* macros it emits
# into the generated evdi_detect.h.
#
# Generate evdi_detect.h, force-include it into every object, and make every
# object depend on it. Used from the kbuild-context branches below, where the
# kernel compiler ($(CC)) and its include/cflags are available. The header is
# regenerated each build but only rewritten when its contents change, so
# incremental builds and kernel switches both behave correctly.
define evdi_conftest
ccflags-y += -include $$(obj)/evdi_detect.h
clean-files := evdi_detect.h evdi_detect.h.tmp
# Explicit (not pattern) prerequisite: GNU make merges prerequisites from
# explicit rules with kbuild's own $(obj)/%.o pattern rule, so the header is
# generated before any object is compiled. Cover the optional COMPAT object too.
$$(addprefix $$(obj)/, $$(evdi-y) evdi_ioc32.o): $$(obj)/evdi_detect.h
$$(obj)/evdi_detect.h: $$(src)/conftest.sh FORCE
	$$(Q)$$(CONFIG_SHELL) $$(src)/conftest.sh "$$(CC)" $$@.tmp \
		$$(NOSTDINC_FLAGS) $$(LINUXINCLUDE) $$(KBUILD_CPPFLAGS) $$(KBUILD_CFLAGS) -DMODULE
	$$(Q)cmp -s $$@.tmp $$@ 2>/dev/null && rm -f $$@.tmp || mv $$@.tmp $$@
endef

ifneq ($(DKMS_BUILD),)

# DKMS

KERN_DIR := /lib/modules/$(KERNELRELEASE)/build

ccflags-y := -Iinclude/uapi/drm -Iinclude/drm
evdi-y := evdi_platform_drv.o evdi_platform_dev.o evdi_sysfs.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_drm_drv.o evdi_fb.o evdi_gem.o evdi_painter.o evdi_params.o evdi_cursor.o evdi_color.o evdi_debug.o evdi_i2c.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
obj-m := evdi.o
$(eval $(call evdi_conftest))

KBUILD_VERBOSE ?= 1

all:
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) M=$(CURDIR) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) CONFIG_MODULE_SIG= -C $(KERN_DIR) modules

clean:
	@echo $(KERN_DIR)
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) M=$(CURDIR) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) -C $(KERN_DIR) clean

else

# Not DKMS

ifneq ($(KERNELRELEASE),)

# inside kbuild
# Note: this can be removed once it is in kernel tree and Kconfig is properly used
ccflags-y := -isystem include/uapi/drm $(CFLAGS)
evdi-y := evdi_platform_drv.o evdi_platform_dev.o evdi_sysfs.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_drm_drv.o evdi_fb.o evdi_gem.o evdi_painter.o evdi_params.o evdi_cursor.o evdi_color.o evdi_debug.o evdi_i2c.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
CONFIG_DRM_EVDI ?= m
obj-$(CONFIG_DRM_EVDI) := evdi.o
obj-y += tests/
$(eval $(call evdi_conftest))

else

# kbuild against specified or current kernel
CP ?= cp
DKMS ?= dkms
RM ?= rm

MODVER=1.15.0

ifeq ($(KVER),)
	KVER := $(shell uname -r)
endif

ifneq ($(RUN_DEPMOD),)
	DEPMOD := /sbin/depmod -a
else
	DEPMOD := true
endif

ifeq ($(KDIR),)
	KDIR := /lib/modules/$(KVER)/build
endif

MOD_KERNEL_PATH := /kernel/drivers/gpu/drm/evdi

default: module

module:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(RM) -rf *.o *.a *.ko .tmp* .*.*.cmd Module.symvers evdi.mod.c modules.order evdi_detect.h evdi_detect.h.tmp

install:
	$(MAKE) -C $(KDIR) M=$$PWD INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MOD_KERNEL_PATH) modules_install
	$(DEPMOD)

uninstall:
	$(RM) -rf $(DESTDIR)/lib/modules/$(KVER)/$(MOD_KERNEL_PATH)
	$(DEPMOD)

install_dkms:
	$(DKMS) install .

uninstall_dkms:
	$(DKMS) remove evdi/$(MODVER) --all
	$(RM) -rf /usr/src/evdi-$(MODVER)

endif # ifneq ($(KERNELRELEASE),)

endif # ifneq ($(DKMS_BUILD),)
