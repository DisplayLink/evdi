# Copyright (c) 2022 DisplayLink (UK) Ltd.
import PyEvdi
import pytest
import os
import re
import utilities

def testCheckDevice():
    for i in get_available_devices():
         assert PyEvdi.check_device(i) == PyEvdi.AVAILABLE
    assert PyEvdi.check_device(0) == PyEvdi.UNRECOGNIZED
    assert PyEvdi.check_device(25) == PyEvdi.NOT_PRESENT

@pytest.mark.skipif(utilities.is_not_running_as_root(), reason = 'Only root can add a new device.')
def testAddDevice():
    assert PyEvdi.add_device() == 1

def get_available_devices():
    list_of_available_devices = []
    list_evdi = os.listdir('/sys/devices/platform/')
    r = re.compile("evdi*")
    list_evdi = list(filter(r.match, list_evdi))

    for s in list_evdi:
        list2 = os.listdir('/sys/devices/platform/' + s + '/drm')
        r = re.compile("card*")
        list_of_available_devices += list(filter(r.match, list2))

    list_of_available_devices = [int(i[4:]) for i in list_of_available_devices]
    return list_of_available_devices
