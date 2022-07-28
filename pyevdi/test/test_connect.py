# Copyright (c) 2022 DisplayLink (UK) Ltd.
import pytest
import test
import PyEvdi
import utilities

def my_mode_changed_handler(mode) -> None:
    print('Mode: ' + str(mode.width) + 'x' + str(mode.height) + '@' + str(mode.refresh_rate))

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testModeChangedHandlerCalledAfterConnect(mocker):
    mock_requests = mocker.patch("test_connect.my_mode_changed_handler", return_value = None)

    card = PyEvdi.Card(3)
    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)

    card.mode_changed_handler = my_mode_changed_handler

    timoutms=1000
    card.handle_events(timoutms)

    card.disconnect()
    card.close()

    mock_requests.assert_called()
