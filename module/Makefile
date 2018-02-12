#
# Copyright (c) 2015 - 2017 DisplayLink (UK) Ltd.
#
# This file is subject to the terms and conditions of the GNU General Public
# License v2. See the file COPYING in the main directory of this archive for
# more details.
#
ifneq ($(DKMS_BUILD),)

# DKMS

KERN_DIR := /lib/modules/$(KERNELRELEASE)/build

ccflags-y := -Iinclude/drm
evdi-y := evdi_drv.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_main.o evdi_fb.o evdi_gem.o evdi_stats.o evdi_painter.o evdi_params.o evdi_cursor.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
obj-m := evdi.o

KBUILD_VERBOSE ?= 1

all:
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) CONFIG_MODULE_SIG= -C $(KERN_DIR) modules

clean:
	@echo $(KERN_DIR)
	$(MAKE) KBUILD_VERBOSE=$(KBUILD_VERBOSE) SUBDIRS=$(CURDIR) SRCROOT=$(CURDIR) -C $(KERN_DIR) clean

else

# Not DKMS

ifneq ($(KERNELRELEASE),)

# inside kbuild
# Note: this can be removed once it is in kernel tree and Kconfig is properly used
CONFIG_DRM_EVDI := m
LINUXINCLUDE := $(subst -I,-isystem,$(LINUXINCLUDE))
ccflags-y := -isystem include/drm $(CFLAGS)
evdi-y := evdi_drv.o evdi_modeset.o evdi_connector.o evdi_encoder.o evdi_main.o evdi_fb.o evdi_gem.o evdi_stats.o evdi_painter.o evdi_params.o evdi_cursor.o
evdi-$(CONFIG_COMPAT) += evdi_ioc32.o
obj-$(CONFIG_DRM_EVDI) := evdi.o

else

# kbuild against specified or current kernel
ifeq ($(KVER),)
	KVER := $(shell uname -r)
endif

ifeq ($(KDIR),)
	KDIR := /lib/modules/$(KVER)/build
endif

default: module

module:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -rf *.o *.ko .tmp* .*.*.cmd Module.symvers evdi.mod.c modules.order


endif # ifneq ($(KERNELRELEASE),)

endif # ifneq ($(DKMS_BUILD),)
