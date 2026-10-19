# Plane lifetime regression

`drm_evdi_plane_cleanup` creates a real EVDI DRM device and checks that
teardown frees each registered primary/cursor plane allocation. DRM cleanup
releases plane state and registration; the driver's destroy callback must also
free the containing allocation. Failed plane initialization retains its existing
constructor cleanup path.

The test observes native `kfree` tracepoints without dereferencing released
planes. It retires each observed identity before allocator reuse and unregisters
and synchronizes the probe before releasing its context. KUnit actions retain
cleanup ownership if an assertion aborts the test.

Before teardown, valid plane-ID queries act as controls for 130 rejected IDs:
zero, `UINT32_MAX`, and 128 seeded IDs outside the signed DRM object-ID range.
Queries use `DRM_IOCTL_MODE_GETPLANE` on the EVDI device. This bounded property
check does not test arbitrary invalid kernel pointers.

Enable `CONFIG_DRM_EVDI_KUNIT_TEST` using the existing `.kunitconfig` and run
the suite with KUnit. Also run with KASAN/UBSan and lockdep/kmemleak; passing
assertions alone do not establish leak freedom. This fix does not change the
update-ready handshake or establish physical display acceptance.
