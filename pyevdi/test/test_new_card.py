# Copyright (c) 2022 DisplayLink (UK) Ltd.
import PyEvdi
import pytest
import os
import utilities

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 , reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testOpeningNewCard():
    x = PyEvdi.Card(utilities.get_available_evdi_card())
    assert x

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 , reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testOpeningAndClosingNewCard():
    x = PyEvdi.Card(utilities.get_available_evdi_card())
    x.close()
    assert x

def testOpeningInvalidCard():    
    with pytest.raises(ValueError):
        PyEvdi.Card(44)
