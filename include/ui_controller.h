#pragma once

#include <Arduino.h>
#include "media.h"

constexpr int UI_LIST_ROWS = 4;

// The controller deals only in semantic inputs and commands.  It deliberately
// does not own the TFT, Audio, Preferences, or web server.
enum class UiPage : uint8_t {
    Home,
    Listening,
    Stations,
    StationOptions,
    StationInfo,
    Favorites,
    RecordedShows,
    ShowEpisodes,
    PodcastPlayer,
    StandbyConfirm,
    Settings,
    SettingsAudio,
    SettingsDisplay,
    SettingsDevice,
    SettingsAbout,
    SettingsWebHandoff,
    SettingsConfirm,
    Unavailable,
};

enum class UiTarget : uint8_t {
    None,
    HomeLiveRadio,
    HomeRecordedShows,
    HomeFavorites,
    HomeSettings,
    ListeningStation,
    ListeningMute,
    ListeningVolume,
    PlayerBack,
    PlayerOptions,
    PlayerPrevious,
    PlayerStopOrPlay,
    PlayerNext,
    ListBack,
    ListPrevious,
    ListNext,
    ListRow0,
    ListRow1,
    ListRow2,
    ListRow3,
    ListRow4,
    ListRowFavorite0,
    ListRowFavorite1,
    ListRowFavorite2,
    ListRowFavorite3,
    ListRowFavorite4,
    OptionsBack,
    OptionsFavorite,
    OptionsInfo,
    InfoBack,
    FavoritesStationsTab,
    FavoritesShowsTab,
    FavoritesRow0,
    FavoritesRow1,
    FavoritesRow2,
    FavoritesRowFavorite0,
    FavoritesRowFavorite1,
    FavoritesRowFavorite2,
    FavoritesBack,
    FavoritesPrevious,
    FavoritesNext,
    ShowsBack,
    ShowRow0,
    ShowRow1,
    ShowRow2,
    ShowRow3,
    ShowRow4,
    ShowRowFavorite0,
    ShowRowFavorite1,
    ShowRowFavorite2,
    ShowRowFavorite3,
    ShowRowFavorite4,
    EpisodesBack,
    EpisodeRow0,
    EpisodeRow1,
    EpisodeRow2,
    EpisodeRow3,
    EpisodeRow4,
    PodcastBack,
    PodcastPause,
    PodcastSeekBack,
    PodcastSeekForward,
    PodcastProgress,
    ConfirmCancel,
    ConfirmStandby,
    SettingsBack,
    SettingsPrevious,
    SettingsNext,
    SettingsRow0,
    SettingsRow1,
    SettingsRow2,
    SettingsRow3,
    SettingsAudio,
    SettingsDisplay,
    SettingsDevice,
    ToneBassDecrease,
    ToneBassIncrease,
    ToneMidDecrease,
    ToneMidIncrease,
    ToneTrebleDecrease,
    ToneTrebleIncrease,
    ToneSave,
    ToneCancel,
    DimDecrease,
    DimIncrease,
    DimSave,
    DimCancel,
    DeviceCalibration,
    DeviceAbout,
    DeviceRestart,
    DeviceFactoryReset,
    AboutBack,
    SettingsConfirmCancel,
    SettingsConfirmAccept,
};

enum class UiCommandKind : uint8_t {
    None,
    ChangeVolume,
    SetVolume,
    ToggleMute,
    SelectStation,
    PreviousStation,
    NextStation,
    StopPlayback,
    RejoinStation,
    ToggleStationFavorite,
    RequestPodcastEpisodes,
    PlayPodcastEpisode,
    TogglePodcastPause,
    SeekPodcast,
    TogglePodcastShowFavorite,
    EnterStandby,
    ApplyTone,
    ApplyAutoDim,
    StartTouchCalibration,
    RestartDevice,
    FactoryResetDevice,
};

struct UiCommand {
    UiCommandKind kind = UiCommandKind::None;
    int value = 0;
    int secondary = 0;
    int tertiary = 0;
};

struct UiRenderState {
    UiPage page = UiPage::Home;
    int stationFocus = 0;
    int stationOffset = 0;
    bool stationFavoriteFocus = false;
    int favoriteFocus = 0;
    int favoriteOffset = 0;
    bool favoriteShowsTab = false;
    bool favoriteTabFocus = true;
    int optionStation = -1;
    uint8_t optionsFocus = 0;
    int showFocus = 0;
    int showOffset = 0;
    bool showFavoritesOnly = false;
    int episodeShow = -1;
    int episodeFocus = 0;
    int episodeOffset = 0;
    uint8_t podcastPlayerFocus = 1;
    bool standbyConfirm = false;
    bool confirmAcceptFocused = false;
    bool volumeOverlay = false;
    uint8_t homeFocus = 0;
    PlaybackState playback = PlaybackState::Stopped;
    int requestedStation = -1;
    int playingStation = -1;
    uint8_t unavailableDestination = 0;
    bool playerControlFocus = false;
    uint8_t playerFocus = 3;
    int settingsOffset = 0;
    // 0 is Network, 1 is Weather & Time; both are real browser handoffs.
    uint8_t settingsWebHandoff = 0;
    int toneBassDraft = 0;
    int toneMidDraft = 0;
    int toneTrebleDraft = 0;
    uint16_t dimSecondsDraft = 30;
    // 0 is none, 1 is restart, 2 is factory reset.
    uint8_t settingsConfirmAction = 0;
    bool deviceActionFailed = false;
    bool dirty = true;
};

void uiControllerBegin();
void uiControllerTurn(int detents, unsigned long now, bool displayWasDimmed);
void uiControllerPush(unsigned long now, bool displayWasDimmed);
void uiControllerHold(unsigned long now, bool displayWasDimmed);
void uiControllerTap(UiTarget target, int value, unsigned long now, bool displayWasDimmed);
void uiControllerPage(int direction, unsigned long now, bool displayWasDimmed);
void uiControllerSetAlarmActive(bool active);
void uiControllerReportDeviceActionFailure();
void uiControllerTick(unsigned long now);
bool uiControllerTakeCommand(UiCommand& command);
UiRenderState uiControllerRenderState();
void uiControllerMarkRendered();
