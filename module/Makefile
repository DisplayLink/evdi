#
# Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
#
# This file is subject to the terms and conditions of the GNU General Public
# License v2. See the file COPYING in the main directory of this archive for
# more details.
#
ifneq ($(DKMS_BUILD),)

# DKMS

KERN_DIR := /lib/modules/$(KERNELRELEASE)/build

ccflags-y := -Iinclude/drm
evdi-y := evdi_drv.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_main.o evdi_fb.o evdi_gem.o evdi_painter.o evdi_params.o evdi_cursor.o evdi_debug.o evdi_i2c.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
obj-m := evdi.o

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
CONFIG_DRM_EVDI := m
LINUXINCLUDE := $(subst -I,-isystem,$(LINUXINCLUDE))
ccflags-y := -isystem include/drm $(CFLAGS)
evdi-y := evdi_drv.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_main.o evdi_fb.o evdi_gem.o evdi_painter.o evdi_params.o evdi_cursor.o evdi_debug.o evdi_i2c.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
obj-$(CONFIG_DRM_EVDI) := evdi.o

else

# kbuild against specified or current kernel
CP ?= cp
DKMS ?= dkms
RM ?= rm

MODVER=1.8.0

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
	$(RM) -rf *.o *.a *.ko .tmp* .*.*.cmd Module.symvers evdi.mod.c modules.order

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
