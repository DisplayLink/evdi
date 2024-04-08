import pytest
import PyEvdi
import utilities


@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testEnablingCursorEventsWithoutCursorSetHandlers(caplog):
    card = PyEvdi.Card(utilities.get_available_evdi_card())
    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)

    assert "Enabling cursor events on" not in caplog.text
    card.enableCursorEvents(True)
    assert "Enabling cursor events on" in caplog.text

    card.disconnect()
    card.close()

@pytest.mark.skipif(utilities.get_available_evdi_card() == -1, reason = 'No available device.')
@pytest.mark.skipif(utilities.get_available_evdi_card() == -1 and utilities.is_not_running_as_root(), reason = 'Please run test as root.')
def testDisablingCursorEventsDoesNotShowEnablingCursorEventsMsgInLog(caplog):
    card = PyEvdi.Card(utilities.get_available_evdi_card())
    edid = utilities.get_edid()
    card.connect(edid, len(edid), utilities._FullHDAreaLimit, utilities._FullHDAreaLimit * utilities._60Hz)


    card.enableCursorEvents(False)
    assert "Enabling cursor events on" not in caplog.text

    card.disconnect()
    card.close()
