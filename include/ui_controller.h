#pragma once

#include <Arduino.h>
#include "media.h"

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
    PodcastOptions,
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
    FavoritesRowFavorite0,
    FavoritesRowFavorite1,
    FavoritesBack,
    FavoritesPrevious,
    FavoritesNext,
    ShowsBack,
    ShowRow0,
    ShowRow1,
    ShowRow2,
    ShowRow3,
    ShowRow4,
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
    PodcastOptions,
    PodcastOptionFavorite,
    PodcastOptionsBack,
    ConfirmCancel,
    ConfirmStandby,
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
};

struct UiCommand {
    UiCommandKind kind = UiCommandKind::None;
    int value = 0;
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
    uint8_t podcastOptionsFocus = 0;
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
    bool dirty = true;
};

void uiControllerBegin();
void uiControllerTurn(int detents, unsigned long now, bool displayWasDimmed);
void uiControllerPush(unsigned long now, bool displayWasDimmed);
void uiControllerHold(unsigned long now, bool displayWasDimmed);
void uiControllerTap(UiTarget target, int value, unsigned long now, bool displayWasDimmed);
void uiControllerSwipe(int direction, unsigned long now, bool displayWasDimmed);
void uiControllerSetAlarmActive(bool active);
void uiControllerTick(unsigned long now);
bool uiControllerTakeCommand(UiCommand& command);
UiRenderState uiControllerRenderState();
void uiControllerMarkRendered();
