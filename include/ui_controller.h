#pragma once

#include <Arduino.h>

// The controller deals only in semantic inputs and commands.  It deliberately
// does not own the TFT, Audio, Preferences, or web server.
enum class UiPage : uint8_t {
    Home,
    Listening,
    Stations,
    StandbyConfirm,
    Unavailable,
};

enum class UiTarget : uint8_t {
    None,
    HomeLiveRadio,
    HomeRecordedShows,
    HomeFavorites,
    HomeSettings,
    HomeNowPlaying,
    ListeningStation,
    ListeningMute,
    ListeningMenu,
    ListBack,
    ListPrevious,
    ListNext,
    ListRow0,
    ListRow1,
    ListRow2,
    ConfirmCancel,
    ConfirmStandby,
};

enum class UiCommandKind : uint8_t {
    None,
    ChangeVolume,
    ToggleMute,
    SelectStation,
    EnterStandby,
};

struct UiCommand {
    UiCommandKind kind = UiCommandKind::None;
    int value = 0;
};

struct UiRenderState {
    UiPage page = UiPage::Home;
    int stationFocus = 0;
    int stationOffset = 0;
    bool standbyConfirm = false;
    bool confirmAcceptFocused = false;
    bool volumeOverlay = false;
    uint8_t homeFocus = 0;
    bool dirty = true;
};

void uiControllerBegin();
void uiControllerTurn(int detents, unsigned long now, bool displayWasDimmed);
void uiControllerPush(unsigned long now, bool displayWasDimmed);
void uiControllerHold(unsigned long now, bool displayWasDimmed);
void uiControllerTap(UiTarget target, unsigned long now, bool displayWasDimmed);
void uiControllerSetAlarmActive(bool active);
void uiControllerTick(unsigned long now);
bool uiControllerTakeCommand(UiCommand& command);
UiRenderState uiControllerRenderState();
void uiControllerMarkRendered();
