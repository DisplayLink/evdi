# Copyright (c) 2022 DisplayLink (UK) Ltd.
import pytest
import test
import PyEvdi
import utilities
import time
import logging


logging.getLogger().setLevel(20);

def my_mode_changed_handler(mode) -> None:
    print('Mode: ' + str(mode.width) + 'x' + str(mode.height) + '@' + str(mode.refresh_rate))

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testModeChangedHandlerCalledAfterConnect(mocker):
    mock_requests = mocker.patch("test_connect.my_mode_changed_handler", return_value = None)

    card = PyEvdi.Card(utilities.get_available_evdi_card())
    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)

    card.mode_changed_handler = my_mode_changed_handler

    timoutms=100

    start_time = time.time()

    while time.time() < start_time + 60 and not mock_requests.called:
        card.handle_events(timoutms)

    card.disconnect()
    card.close()

    mock_requests.assert_called()

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testHandlingEventsTenTimesWithDefaultHandlers():
    card = PyEvdi.Card(utilities.get_available_evdi_card())

    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)


    timoutms=1000
    for i in range(10):
        card.handle_events(timoutms)

    card.disconnect()
    card.close()

def my_acquire_framebuffer_cb(buffer) -> None:
    print("received buffer", buffer.id)
    print("rect_count:", buffer.rect_count, "\nrects:")
    for i in buffer.rects:
        print(i.x1, i.y1, i.x2, i.y2)
    del buffer

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testHandlingEventsTenTimesWithAquireFramebufferSet():
    card = PyEvdi.Card(utilities.get_available_evdi_card())

    card.acquire_framebuffer_cb = my_acquire_framebuffer_cb

    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)

    timoutms=1000
    for i in range(10):
        card.handle_events(timoutms)

    card.disconnect()
    card.close()
