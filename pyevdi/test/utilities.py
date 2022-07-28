# Copyright (c) 2022 DisplayLink (UK) Ltd.
import PyEvdi
import os

_FullHDAreaLimit = 1920*1080
_4KAreaLimit = 3840*2160
_60Hz=60

def is_not_running_as_root():
    return os.geteuid() != 0

def get_available_evdi_card():
    for i in range(20):
        if PyEvdi.check_device(i) == PyEvdi.AVAILABLE:
            return i
    PyEvdi.add_device()
    for i in range(20):
        if PyEvdi.check_device(i) == PyEvdi.AVAILABLE:
            return i
    return -1

def get_edid():
    with open("4K60HzTest.edid", mode='rb') as file:
        ed = file.read()
    return ed
