#include "ui_controller.h"

#include "media.h"

namespace {

constexpr int kRowsPerPage = 3;
constexpr unsigned long kVolumeOverlayMs = 1500;

UiRenderState state;
UiCommand pendingCommand;
bool hasPendingCommand = false;
int pendingVolumeDelta = 0;
bool alarmIsActive = false;
unsigned long volumeOverlayUntil = 0;

void markDirty() {
    state.dirty = true;
}

void queue(UiCommandKind kind, int value = 0) {
    // A rotary burst is intentionally coalesced. Keep it separate from the
    // one bounded terminal command so a quick turn followed by a selection
    // cannot lose that selection.
    if (kind == UiCommandKind::ChangeVolume) {
        pendingVolumeDelta += value;
        return;
    }
    if (!hasPendingCommand) {
        pendingCommand = {kind, value};
        hasPendingCommand = true;
    }
}

void openStations() {
    state.page = UiPage::Stations;
    const int count = playableStationCount();
    state.stationFocus = selectedPlayableStationIndex();
    if (state.stationFocus < 0) {
        state.stationFocus = 0;
    }
    if (count > 0 && state.stationFocus >= count) {
        state.stationFocus = count - 1;
    }
    state.stationOffset = (state.stationFocus / kRowsPerPage) * kRowsPerPage;
    markDirty();
}

void openPlayer() {
    state.page = UiPage::Listening;
    markDirty();
}

void openUnavailable() {
    state.page = UiPage::Unavailable;
    markDirty();
}

void closeToHome() {
    state.page = UiPage::Home;
    state.standbyConfirm = false;
    markDirty();
}

void selectFocusedStation() {
    const int slot = playableStationSlotAt(state.stationFocus);
    if (slot >= 0) {
        queue(UiCommandKind::SelectStation, slot);
        openPlayer();
    }
}

void moveStationFocus(int detents) {
    const int count = playableStationCount();
    if (count <= 0 || detents == 0) {
        return;
    }
    state.stationFocus = constrain(state.stationFocus + detents, 0, count - 1);
    state.stationOffset = (state.stationFocus / kRowsPerPage) * kRowsPerPage;
    markDirty();
}

void handleTarget(UiTarget target) {
    switch (state.page) {
    case UiPage::Home:
        if (target == UiTarget::HomeLiveRadio) {
            openStations();
        } else if (target == UiTarget::HomeNowPlaying) {
            openPlayer();
        } else if (target == UiTarget::HomeRecordedShows || target == UiTarget::HomeFavorites ||
                   target == UiTarget::HomeSettings) {
            openUnavailable();
        }
        break;
    case UiPage::Listening:
        if (target == UiTarget::ListeningStation || target == UiTarget::ListeningMenu) {
            openStations();
        } else if (target == UiTarget::ListeningMute) {
            queue(UiCommandKind::ToggleMute);
            markDirty();
        }
        break;
    case UiPage::Stations:
        if (target == UiTarget::ListBack) {
            closeToHome();
        } else if (target == UiTarget::ListPrevious) {
            moveStationFocus(-kRowsPerPage);
        } else if (target == UiTarget::ListNext) {
            moveStationFocus(kRowsPerPage);
        } else if (target >= UiTarget::ListRow0 && target <= UiTarget::ListRow2) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::ListRow0);
            const int visibleIndex = state.stationOffset + row;
            if (visibleIndex < playableStationCount()) {
                state.stationFocus = visibleIndex;
                selectFocusedStation();
            }
        }
        break;
    case UiPage::StandbyConfirm:
        if (target == UiTarget::ConfirmStandby) {
            queue(UiCommandKind::EnterStandby);
        }
        closeToHome();
        break;
    case UiPage::Unavailable:
        closeToHome();
        break;
    }
}

}  // namespace

void uiControllerBegin() {
    state = {};
    pendingCommand = {};
    hasPendingCommand = false;
    pendingVolumeDelta = 0;
    alarmIsActive = false;
    volumeOverlayUntil = 0;
}

void uiControllerTurn(int detents, unsigned long now, bool displayWasDimmed) {
    if (detents == 0 || alarmIsActive) {
        return;
    }
    if (displayWasDimmed && state.page != UiPage::Listening) {
        markDirty();
        return;
    }
    if (state.page == UiPage::Home) {
        state.homeFocus = constrain(static_cast<int>(state.homeFocus) + detents, 0, 3);
        markDirty();
    } else if (state.page == UiPage::Listening) {
        queue(UiCommandKind::ChangeVolume, detents);
        volumeOverlayUntil = now + kVolumeOverlayMs;
        markDirty();
    } else if (state.page == UiPage::Stations) {
        moveStationFocus(detents);
    } else if (state.page == UiPage::StandbyConfirm) {
        state.confirmAcceptFocused = detents > 0;
        markDirty();
    }
}

void uiControllerPush(unsigned long now, bool displayWasDimmed) {
    (void)now;
    if (alarmIsActive || displayWasDimmed) {
        markDirty();
        return;
    }
    if (state.page == UiPage::Home) {
        const UiTarget homeTargets[] = {
            UiTarget::HomeLiveRadio,
            UiTarget::HomeRecordedShows,
            UiTarget::HomeFavorites,
            UiTarget::HomeSettings,
        };
        handleTarget(homeTargets[state.homeFocus]);
    } else if (state.page == UiPage::Listening) {
        openStations();
    } else if (state.page == UiPage::Stations) {
        selectFocusedStation();
    } else if (state.page == UiPage::StandbyConfirm) {
        handleTarget(state.confirmAcceptFocused ? UiTarget::ConfirmStandby : UiTarget::ConfirmCancel);
    }
}

void uiControllerHold(unsigned long now, bool displayWasDimmed) {
    (void)now;
    if (alarmIsActive || displayWasDimmed) {
        markDirty();
        return;
    }
    if (state.page == UiPage::Home || state.page == UiPage::Listening) {
        state.page = UiPage::StandbyConfirm;
        state.standbyConfirm = true;
        state.confirmAcceptFocused = false;
        markDirty();
    } else {
        closeToHome();
    }
}

void uiControllerTap(UiTarget target, unsigned long now, bool displayWasDimmed) {
    (void)now;
    if (alarmIsActive || displayWasDimmed) {
        markDirty();
        return;
    }
    handleTarget(target);
}

void uiControllerSetAlarmActive(bool active) {
    alarmIsActive = active;
}

void uiControllerTick(unsigned long now) {
    if (volumeOverlayUntil != 0 && static_cast<long>(now - volumeOverlayUntil) >= 0) {
        volumeOverlayUntil = 0;
        markDirty();
    }
}

bool uiControllerTakeCommand(UiCommand& command) {
    if (pendingVolumeDelta != 0) {
        command = {UiCommandKind::ChangeVolume, pendingVolumeDelta};
        pendingVolumeDelta = 0;
        return true;
    }
    if (!hasPendingCommand) {
        return false;
    }
    command = pendingCommand;
    hasPendingCommand = false;
    return true;
}

UiRenderState uiControllerRenderState() {
    UiRenderState renderState = state;
    renderState.volumeOverlay = volumeOverlayUntil != 0;
    return renderState;
}

void uiControllerMarkRendered() {
    state.dirty = false;
}
