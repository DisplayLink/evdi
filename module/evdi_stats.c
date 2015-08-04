// Copyright (c) 2015 DisplayLink (UK) Ltd.

#include "evdi_drv.h"

static struct evdi_device *g_evdi = NULL;

static ssize_t frame_count_show(struct device *dev, struct device_attribute *a, char *buf) {
  if (g_evdi) {
    return snprintf(buf, PAGE_SIZE, "%u\n", atomic_read(&g_evdi->frame_count));
  }
  return 0;
}

static struct device_attribute evdi_device_attributes[] = {
  __ATTR_RO(frame_count),
};

void evdi_stats_init(struct evdi_device *evdi)
{
  int i, retval;

  g_evdi = evdi;

  DRM_INFO("evdi: evdi_stats_init\n");

  for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++) {
    retval = device_create_file(evdi->ddev->primary->kdev, &evdi_device_attributes[i]);
    if (retval) {
      DRM_ERROR("evdi: device_create_file failed %d\n", retval);
    }
  }
}

void evdi_stats_cleanup(struct evdi_device *evdi)
{
  int i;

  g_evdi = NULL;

  DRM_INFO("evdi: evdi_stats_cleanup\n");

  for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++) {
    device_remove_file(evdi->ddev->primary->kdev, &evdi_device_attributes[i]);
  }
}

