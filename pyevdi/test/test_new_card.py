# Copyright (c) 2022 DisplayLink (UK) Ltd.
import PyEvdi
import pytest
import os

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

@pytest.mark.skipif(get_available_evdi_card() == -1 , reason = 'No available device.')
@pytest.mark.skipif(get_available_evdi_card() == -1 and is_not_running_as_root(), reason = 'Please run test as root.')
def testOpeningNewCard():
    x = PyEvdi.Card(get_available_evdi_card())
    assert x

@pytest.mark.skipif(get_available_evdi_card() == -1 , reason = 'No available device.')
@pytest.mark.skipif(get_available_evdi_card() == -1 and is_not_running_as_root(), reason = 'Please run test as root.')
def testOpeningAndClosingNewCard():
    x = PyEvdi.Card(get_available_evdi_card())
    x.close()
    assert x

def testOpeningInvalidCard():    
    with pytest.raises(ValueError):
        PyEvdi.Card(44)
