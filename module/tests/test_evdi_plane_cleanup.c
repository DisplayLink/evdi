// SPDX-License-Identifier: GPL-2.0-only
/* BUG-019: registered planes must release their containing allocations. */
#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <linux/file.h>
#include <trace/events/kmem.h>
#include "evdi_drm_drv.h"
#include "evdi_test.h"

struct plane_free_watch {
	const void *planes[2];
	atomic_t frees[2];
	unsigned int count;
};

static void record_plane_free(void *data,
			      unsigned long __always_unused call_site,
			      const void *ptr)
{
	struct plane_free_watch *watch = data;

	if (!ptr)
		return;
	/* Retire the identity before its address can be reused. */
	for (unsigned int i = 0; i < watch->count; i++)
		if (ptr == cmpxchg(&watch->planes[i], ptr, NULL))
			atomic_inc(&watch->frees[i]);
}

static void stop_plane_watch(void *data)
{
	unregister_trace_kfree(record_plane_free, data);
	tracepoint_synchronize_unregister();
}

static void watch_plane(struct kunit *test, struct plane_free_watch *watch,
			struct drm_plane *plane)
{
	KUNIT_ASSERT_LT(test, watch->count, ARRAY_SIZE(watch->planes));
	atomic_set(&watch->frees[watch->count], 0);
	watch->planes[watch->count++] = plane;
}

static void start_plane_watch(struct kunit *test, struct drm_device *dev,
			      struct plane_free_watch *watch)
{
	struct drm_plane *plane;

	drm_for_each_plane(plane, dev)
		watch_plane(test, watch, plane);
	KUNIT_ASSERT_GT(test, watch->count, 0U);
	KUNIT_ASSERT_EQ(test, register_trace_kfree(record_plane_free, watch),
			0);
	KUNIT_ASSERT_EQ(
		test, kunit_add_action_or_reset(test, stop_plane_watch, watch),
		0);
}

static void close_test_file(void *data)
{
	fput(data);
	flush_delayed_fput();
}

static void expect_plane_query(struct kunit *test, struct file *file,
			       void __user *buffer, u32 id, long expected)
{
	struct drm_mode_get_plane query = { .plane_id = id };

	KUNIT_ASSERT_EQ(test, copy_to_user(buffer, &query, sizeof(query)), 0UL);
	KUNIT_EXPECT_EQ(test,
			drm_ioctl(file, DRM_IOCTL_MODE_GETPLANE,
				  (unsigned long)buffer),
			expected);
}

static u32 invalid_plane_id(u32 *seed)
{
	*seed = *seed * 1664525U + 1013904223U;
	return 0x80000000U + (*seed >> 1);
}

static void fuzz_invalid_plane_ids(struct kunit *test, struct drm_device *dev)
{
	struct file *file = mock_drm_getfile(dev->primary, O_RDWR);
	void __user *buffer;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file);
	KUNIT_ASSERT_EQ(test,
			kunit_add_action_or_reset(test, close_test_file, file),
			0);
	buffer = evdi_kunit_alloc_usermem(test,
					  sizeof(struct drm_mode_get_plane));
	struct drm_plane *plane;

	drm_for_each_plane(plane, dev)
		expect_plane_query(test, file, buffer, plane->base.id, 0);
	expect_plane_query(test, file, buffer, 0, -ENOENT);
	expect_plane_query(test, file, buffer, U32_MAX, -ENOENT);
	u32 seed = 0x45564449;

	for (unsigned int i = 0; i < 128; i++)
		expect_plane_query(test, file, buffer, invalid_plane_id(&seed),
				   -ENOENT);
	kunit_release_action(test, close_test_file, file);
}

static void remove_test_device(void *data)
{
	struct evdi_test_data *base = data;

	evdi_drm_device_remove(base->dev);
	base->dev = NULL;
	evdi_test_data_exit(kunit_get_current_test(), base);
}

static void test_plane_cleanup(struct kunit *test)
{
	struct evdi_test_data *base =
		kunit_kzalloc(test, sizeof(*base), GFP_KERNEL);
	struct plane_free_watch *watch =
		kunit_kzalloc(test, sizeof(*watch), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, base);
	KUNIT_ASSERT_NOT_NULL(test, watch);
	evdi_test_data_init(test, base);
	base->dev = evdi_drm_device_create(base->parent);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, base->dev);
	KUNIT_ASSERT_EQ(
		test, kunit_add_action_or_reset(test, remove_test_device, base),
		0);
	start_plane_watch(test, base->dev, watch);
	fuzz_invalid_plane_ids(test, base->dev);
	kunit_release_action(test, remove_test_device, base);
	kunit_release_action(test, stop_plane_watch, watch);
	for (unsigned int i = 0; i < watch->count; i++)
		KUNIT_EXPECT_EQ(test, atomic_read(&watch->frees[i]), 1);
}

static struct kunit_case cases[] = { KUNIT_CASE(test_plane_cleanup), {} };

static struct kunit_suite suite = {
	.name = "drm_evdi_plane_cleanup",
	.test_cases = cases,
};

kunit_test_suite(suite);
MODULE_LICENSE("GPL");
