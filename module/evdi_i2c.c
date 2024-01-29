// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include "evdi_i2c.h"
#include "evdi_debug.h"
#include "evdi_drm_drv.h"

static int dli2c_access_master(struct i2c_adapter *adapter,
	struct i2c_msg *msgs, int num)
{
	int i = 0, result = 0;
	struct evdi_device *evdi = adapter->algo_data;
	struct evdi_painter *painter = evdi->painter;

	for (i = 0; i < num; i++) {
		if (evdi_painter_i2c_data_notify(painter, &msgs[i]))
			result++;
	}

	return result;
}

static u32 dli2c_func(__always_unused struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static struct i2c_algorithm dli2c_algorithm = {
	.master_xfer = dli2c_access_master,
	.functionality = dli2c_func,
};

int evdi_i2c_add(struct i2c_adapter *adapter, struct device *parent,
	void *ddev)
{
	adapter->owner  = THIS_MODULE;
#if KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
	adapter->class  = I2C_CLASS_DDC;
#endif
	adapter->algo   = &dli2c_algorithm;
	strscpy(adapter->name, "DisplayLink I2C Adapter", sizeof(adapter->name));
	adapter->dev.parent = parent;
	adapter->algo_data = ddev;

	return i2c_add_adapter(adapter);
}

void evdi_i2c_remove(struct i2c_adapter *adapter)
{
	i2c_del_adapter(adapter);
}
