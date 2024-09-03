# Copyright (c) 2022 DisplayLink (UK) Ltd.
import pytest
import PyEvdi
import utilities
import time
import logging

@pytest.fixture
def pyevdi_card():
    card = PyEvdi.Card(utilities.get_available_evdi_card())
    yield card
    card.close()

@pytest.fixture
def pyevdi_connect(pyevdi_card):
    edid = utilities.get_edid()
    pyevdi_card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)
    yield
    pyevdi_card.disconnect()

logging.getLogger().setLevel(20);

def my_mode_changed_handler(mode) -> None:
    print('Mode: ' + str(mode.width) + 'x' + str(mode.height) + '@' + str(mode.refresh_rate))

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def test_mode_changed_handler_called_after_connect(mocker, pyevdi_card, pyevdi_connect):
    mock_requests = mocker.patch("test_connect.my_mode_changed_handler", return_value = None)
    card = pyevdi_card
    card.mode_changed_handler = my_mode_changed_handler
    timoutms=100
    start_time = time.time()

    while time.time() < start_time + 60 and not mock_requests.called:
        card.handle_events(timoutms)

    mock_requests.assert_called()

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def test_handling_events_ten_times_with_default_handlers(pyevdi_card, pyevdi_connect):
    card = pyevdi_card
    timoutms=1000

    for i in range(10):
        card.handle_events(timoutms)

def my_acquire_framebuffer_handler(buffer) -> None:
    print("received buffer", buffer.id)
    print("rect_count:", buffer.rect_count, "\nrects:")
    for i in buffer.rects:
        print(i.x1, i.y1, i.x2, i.y2)
    del buffer

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def test_handling_events_ten_times_with_aquire_framebuffer_set(pyevdi_card, pyevdi_connect):
    card = pyevdi_card
    card.acquire_framebuffer_handler = my_acquire_framebuffer_handler
    timoutms=1000

    for i in range(10):
        card.handle_events(timoutms)

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def test_reading_buffer():
    stats = PyEvdi.MemoryAccessStats()
    card = PyEvdi.Card(utilities.get_available_evdi_card(), stats)
    card.acquire_framebuffer_handler = stats
    edid = utilities.get_edid()

    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)
    card.handle_events(1000)

    assert stats.totalGrabPixelsTime() > 0
    card.disconnect()
    card.close()

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def test_time_of_first_frame(pyevdi_card, pyevdi_connect):
    # never fails, because wait_for_master
    # does not timeout when the card is opened
    start = time.time()
    card = pyevdi_card
    timeout_in_seconds_to_recieve_frame = 2
    ms_in_s = 1000

    card.handle_events(timeout_in_seconds_to_recieve_frame * ms_in_s)
    end = time.time()

    assert end - start < timeout_in_seconds_to_recieve_frame, 'Turning on is too long'
