#include "ui_controller.h"

#include "media.h"
#include "app_state.h"
#include "settings.h"

#include <limits.h>

namespace {

constexpr int kRowsPerPage = UI_LIST_ROWS;
constexpr int kFavoriteRowsPerPage = 3;
constexpr unsigned long kVolumeOverlayMs = 1500;
constexpr int16_t kPodcastProgressX = 128;
constexpr int16_t kPodcastProgressWidth = 166;

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
    state.stationOffset = 0;
    if (count > kRowsPerPage) {
        state.stationOffset = constrain(state.stationFocus - (kRowsPerPage - 1), 0, count - kRowsPerPage);
    }
    state.stationFavoriteFocus = false;
    markDirty();
}

int favoriteStationCount() {
    int count = 0;
    for (int visibleIndex = 0; visibleIndex < playableStationCount(); ++visibleIndex) {
        const int slot = playableStationSlotAt(visibleIndex);
        if (isStationFavorite(slot)) ++count;
    }
    return count;
}

int favoriteStationSlotAt(int favoriteIndex) {
    if (favoriteIndex < 0) return -1;
    for (int visibleIndex = 0; visibleIndex < playableStationCount(); ++visibleIndex) {
        const int slot = playableStationSlotAt(visibleIndex);
        if (isStationFavorite(slot) && favoriteIndex-- == 0) return slot;
    }
    return -1;
}

void openFavorites() {
    state.page = UiPage::Favorites;
    state.favoriteShowsTab = false;
    state.favoriteTabFocus = true;
    state.favoriteFocus = 0;
    for (int index = 0; index < favoriteStationCount(); ++index) {
        if (favoriteStationSlotAt(index) == currentStationIdx) {
            state.favoriteFocus = index * 2;
            break;
        }
    }
    state.favoriteOffset = (state.favoriteFocus / 2 / kFavoriteRowsPerPage) * kFavoriteRowsPerPage;
    markDirty();
}

void openStationOptions() {
    state.page = UiPage::StationOptions;
    state.optionStation = currentStationIdx >= 0 && currentStationIdx < STATION_COUNT
        ? currentStationIdx
        : -1;
    state.optionsFocus = 0;
    markDirty();
}

void openStationInfo() {
    state.page = UiPage::StationInfo;
    markDirty();
}

int podcastShowCount(bool favoritesOnly) {
    if (!favoritesOnly) return PODCAST_SHOW_COUNT;
    int count = 0;
    for (int index = 0; index < PODCAST_SHOW_COUNT; ++index) {
        if (isPodcastShowFavorite(index)) ++count;
    }
    return count;
}

int podcastShowAt(int visibleIndex, bool favoritesOnly) {
    if (visibleIndex < 0) return -1;
    for (int index = 0; index < PODCAST_SHOW_COUNT; ++index) {
        if (favoritesOnly && !isPodcastShowFavorite(index)) continue;
        if (visibleIndex-- == 0) return index;
    }
    return -1;
}

void openRecordedShows(bool favoritesOnly = false) {
    state.page = UiPage::RecordedShows;
    state.showFavoritesOnly = favoritesOnly;
    state.showFocus = 0;
    state.showOffset = 0;
    markDirty();
}

void openEpisodes(int showIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT) return;
    state.page = UiPage::ShowEpisodes;
    state.episodeShow = showIndex;
    state.episodeFocus = 0;
    state.episodeOffset = 0;
    queue(UiCommandKind::RequestPodcastEpisodes, showIndex);
    markDirty();
}

void openPodcastPlayer(int showIndex, int episodeIndex) {
    state.page = UiPage::PodcastPlayer;
    state.episodeShow = showIndex;
    state.episodeFocus = episodeIndex;
    state.podcastPlayerFocus = 1;
    markDirty();
}

void seekPodcastToProgress(int touchX) {
    const PodcastPlaybackSnapshot playback = podcastPlaybackSnapshot();
    if (!playback.canSeek || playback.durationSeconds == 0) return;

    const int16_t clampedX = constrain(touchX, kPodcastProgressX,
                                       kPodcastProgressX + kPodcastProgressWidth);
    const uint32_t targetSeconds = static_cast<uint32_t>(
        (static_cast<uint64_t>(clampedX - kPodcastProgressX) * playback.durationSeconds) /
        kPodcastProgressWidth);
    const int64_t delta = static_cast<int64_t>(targetSeconds) - playback.elapsedSeconds;
    const int commandSeconds = delta > INT_MAX ? INT_MAX :
        (delta < INT_MIN ? INT_MIN : static_cast<int>(delta));
    if (commandSeconds != 0) queue(UiCommandKind::SeekPodcast, commandSeconds);
    markDirty();
}

enum class UnavailableDestination : uint8_t {
    Generic,
    RecordedShows,
    Favorites,
    Settings,
    StationOptions,
};

void openUnavailable(UnavailableDestination destination = UnavailableDestination::Generic) {
    state.page = UiPage::Unavailable;
    state.unavailableDestination = static_cast<uint8_t>(destination);
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
        // Home already shows the active station, so selecting a live stream
        // returns there instead of opening the redundant live-player page.
        closeToHome();
    }
}

void moveStationActionFocus(int detents) {
    const int count = playableStationCount();
    if (count <= 0 || detents == 0) return;
    const int direction = detents > 0 ? 1 : -1;
    for (int step = 0; step < abs(detents); ++step) {
        if (direction > 0) {
            if (!state.stationFavoriteFocus) {
                state.stationFavoriteFocus = true;
            } else if (state.stationFocus < count - 1) {
                ++state.stationFocus;
                state.stationFavoriteFocus = false;
            }
        } else if (state.stationFavoriteFocus) {
            state.stationFavoriteFocus = false;
        } else if (state.stationFocus > 0) {
            --state.stationFocus;
            state.stationFavoriteFocus = true;
        }
    }
    const int maxOffset = max(0, count - kRowsPerPage);
    if (state.stationFocus < state.stationOffset) {
        state.stationOffset = state.stationFocus;
    } else if (state.stationFocus >= state.stationOffset + kRowsPerPage) {
        state.stationOffset = state.stationFocus - (kRowsPerPage - 1);
    }
    state.stationOffset = constrain(state.stationOffset, 0, maxOffset);
    markDirty();
}

void moveFavoriteActionFocus(int detents) {
    if (state.favoriteTabFocus) {
        if (detents != 0) {
            state.favoriteShowsTab = detents > 0;
            markDirty();
        }
        return;
    }
    const int actionCount = state.favoriteShowsTab ? 0 : favoriteStationCount() * 2;
    if (detents == 0) return;
    if (actionCount <= 0 || (state.favoriteFocus == 0 && detents < 0)) {
        state.favoriteTabFocus = true;
        state.favoriteShowsTab = false;
        markDirty();
        return;
    }
    state.favoriteFocus = constrain(state.favoriteFocus + detents, 0, actionCount - 1);
    state.favoriteOffset = (state.favoriteFocus / 2 / kFavoriteRowsPerPage) * kFavoriteRowsPerPage;
    markDirty();
}

void handleTarget(UiTarget target, int value = 0) {
    switch (state.page) {
    case UiPage::Home:
        if (target == UiTarget::HomeLiveRadio) {
            openStations();
        } else if (target == UiTarget::HomeFavorites) {
            openFavorites();
        } else if (target == UiTarget::HomeRecordedShows) {
            openRecordedShows();
        } else if (target == UiTarget::HomeSettings) {
            openUnavailable(UnavailableDestination::Settings);
        }
        break;
    case UiPage::Listening:
        if (target == UiTarget::ListeningStation) {
            openStations();
        } else if (target == UiTarget::ListeningMute) {
            queue(UiCommandKind::ToggleMute);
            markDirty();
        } else if (target == UiTarget::ListeningVolume) {
            if (state.playerControlFocus) {
                volumeOverlayUntil = millis() + kVolumeOverlayMs;
                state.playerControlFocus = false;
                markDirty();
                break;
            }
            const int volume = constrain((value - 48) * 21 / 214, 0, 21);
            queue(UiCommandKind::SetVolume, volume);
            volumeOverlayUntil = millis() + kVolumeOverlayMs;
            markDirty();
        } else if (target == UiTarget::PlayerBack) {
            state.playerControlFocus = false;
            closeToHome();
        } else if (target == UiTarget::PlayerOptions) {
            openStationOptions();
        } else if (target == UiTarget::PlayerPrevious) {
            queue(UiCommandKind::PreviousStation);
        } else if (target == UiTarget::PlayerNext) {
            queue(UiCommandKind::NextStation);
        } else if (target == UiTarget::PlayerStopOrPlay) {
            if (mediaPlaybackState() == PlaybackState::Stopped ||
                mediaPlaybackState() == PlaybackState::Failed) {
                queue(UiCommandKind::RejoinStation);
            } else {
                queue(UiCommandKind::StopPlayback);
            }
            markDirty();
        }
        break;
    case UiPage::Stations:
        if (target == UiTarget::ListBack) {
            closeToHome();
        } else if (target == UiTarget::ListPrevious) {
            uiControllerPage(-1, 0, false);
        } else if (target == UiTarget::ListNext) {
            uiControllerPage(1, 0, false);
        } else if (target >= UiTarget::ListRow0 && target <= UiTarget::ListRow3) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::ListRow0);
            const int visibleIndex = state.stationOffset + row;
            if (visibleIndex < playableStationCount()) {
                state.stationFocus = visibleIndex;
                selectFocusedStation();
            }
        } else if (target >= UiTarget::ListRowFavorite0 && target <= UiTarget::ListRowFavorite3) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::ListRowFavorite0);
            const int slot = playableStationSlotAt(state.stationOffset + row);
            if (slot >= 0) queue(UiCommandKind::ToggleStationFavorite, slot);
        }
        break;
    case UiPage::StationOptions:
        if (target == UiTarget::OptionsFavorite && state.optionStation >= 0) {
            queue(UiCommandKind::ToggleStationFavorite, state.optionStation);
            markDirty();
        } else if (target == UiTarget::OptionsInfo) {
            openStationInfo();
        } else if (target == UiTarget::OptionsBack) {
            closeToHome();
        }
        break;
    case UiPage::StationInfo:
        if (target == UiTarget::InfoBack) openStationOptions();
        break;
    case UiPage::Favorites:
        if (target == UiTarget::FavoritesStationsTab) {
            state.favoriteShowsTab = false;
            state.favoriteTabFocus = false;
            state.favoriteFocus = 0;
            state.favoriteOffset = 0;
            markDirty();
        } else if (target == UiTarget::FavoritesShowsTab) {
            state.favoriteShowsTab = true;
            state.favoriteTabFocus = true;
            state.favoriteFocus = 0;
            state.favoriteOffset = 0;
            markDirty();
        } else if (target == UiTarget::FavoritesBack) {
            closeToHome();
        } else if (target == UiTarget::FavoritesPrevious) {
            uiControllerPage(-1, 0, false);
        } else if (target == UiTarget::FavoritesNext) {
            uiControllerPage(1, 0, false);
        } else if (target >= UiTarget::FavoritesRow0 && target <= UiTarget::FavoritesRow2) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::FavoritesRow0);
            const int slot = favoriteStationSlotAt(state.favoriteOffset + row);
            if (!state.favoriteShowsTab && slot >= 0) {
                queue(UiCommandKind::SelectStation, slot);
                closeToHome();
            } else if (state.favoriteShowsTab) {
                const int show = podcastShowAt(state.favoriteOffset + row, true);
                if (show >= 0) openEpisodes(show);
            }
        } else if (target >= UiTarget::FavoritesRowFavorite0 && target <= UiTarget::FavoritesRowFavorite2) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::FavoritesRowFavorite0);
            const int slot = favoriteStationSlotAt(state.favoriteOffset + row);
            if (!state.favoriteShowsTab && slot >= 0) queue(UiCommandKind::ToggleStationFavorite, slot);
        }
        break;
    case UiPage::RecordedShows:
        if (target == UiTarget::ShowsBack) {
            state.showFavoritesOnly ? openFavorites() : closeToHome();
        } else if (target == UiTarget::ListPrevious || target == UiTarget::ListNext) {
            uiControllerPage(target == UiTarget::ListNext ? 1 : -1, 0, false);
        } else if (target >= UiTarget::ShowRow0 && target <= UiTarget::ShowRow3) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::ShowRow0);
            const int show = podcastShowAt(state.showOffset + row, state.showFavoritesOnly);
            if (show >= 0) openEpisodes(show);
        } else if (target >= UiTarget::ShowRowFavorite0 && target <= UiTarget::ShowRowFavorite3) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::ShowRowFavorite0);
            const int show = podcastShowAt(state.showOffset + row, state.showFavoritesOnly);
            if (show >= 0) queue(UiCommandKind::TogglePodcastShowFavorite, show);
        }
        break;
    case UiPage::ShowEpisodes:
        if (target == UiTarget::EpisodesBack) {
            openRecordedShows(state.showFavoritesOnly);
        } else if (target == UiTarget::ListPrevious || target == UiTarget::ListNext) {
            uiControllerPage(target == UiTarget::ListNext ? 1 : -1, 0, false);
        } else if (target >= UiTarget::EpisodeRow0 && target <= UiTarget::EpisodeRow3 &&
                   podcastEpisodesReadyFor(state.episodeShow)) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::EpisodeRow0);
            const int episode = state.episodeOffset + row;
            if (episode >= 0 && episode < podcastEpisodeCount) {
                queue(UiCommandKind::PlayPodcastEpisode, state.episodeShow * MAX_EPISODES + episode);
                openPodcastPlayer(state.episodeShow, episode);
            }
        }
        break;
    case UiPage::PodcastPlayer:
        if (target == UiTarget::PodcastBack) {
            openEpisodes(state.episodeShow);
        } else if (target == UiTarget::PodcastPause) {
            queue(UiCommandKind::TogglePodcastPause);
        } else if (target == UiTarget::PodcastSeekBack) {
            queue(UiCommandKind::SeekPodcast, -15);
        } else if (target == UiTarget::PodcastSeekForward) {
            queue(UiCommandKind::SeekPodcast, 30);
        } else if (target == UiTarget::PodcastProgress) {
            seekPodcastToProgress(value);
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
        if (state.playerControlFocus) {
            state.playerFocus = constrain(static_cast<int>(state.playerFocus) + detents, 0, 6);
        } else {
            queue(UiCommandKind::ChangeVolume, detents);
            volumeOverlayUntil = now + kVolumeOverlayMs;
        }
        markDirty();
    } else if (state.page == UiPage::Stations) {
        moveStationActionFocus(detents);
    } else if (state.page == UiPage::StationOptions) {
        state.optionsFocus = constrain(static_cast<int>(state.optionsFocus) + detents, 0, 2);
        markDirty();
    } else if (state.page == UiPage::Favorites) {
        moveFavoriteActionFocus(detents);
    } else if (state.page == UiPage::RecordedShows) {
        const int count = podcastShowCount(state.showFavoritesOnly);
        state.showFocus = constrain(state.showFocus + detents, 0, max(0, count - 1));
        state.showOffset = constrain(state.showFocus - (kRowsPerPage - 1), 0, max(0, count - kRowsPerPage));
        markDirty();
    } else if (state.page == UiPage::ShowEpisodes) {
        const int count = podcastEpisodesReadyFor(state.episodeShow) ? podcastEpisodeCount : 0;
        state.episodeFocus = constrain(state.episodeFocus + detents, 0, max(0, count - 1));
        state.episodeOffset = constrain(state.episodeFocus - (kRowsPerPage - 1), 0, max(0, count - kRowsPerPage));
        markDirty();
    } else if (state.page == UiPage::PodcastPlayer) {
        state.podcastPlayerFocus = constrain(static_cast<int>(state.podcastPlayerFocus) + detents, 0, 2);
        markDirty();
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
        if (!state.playerControlFocus) {
            state.playerControlFocus = true;
            state.playerFocus = 3;  // Stop/Play starts selected.
            markDirty();
        } else {
            const UiTarget playerTargets[] = {
                UiTarget::PlayerBack,
                UiTarget::PlayerOptions,
                UiTarget::PlayerPrevious,
                UiTarget::PlayerStopOrPlay,
                UiTarget::PlayerNext,
                UiTarget::ListeningMute,
                UiTarget::ListeningVolume,
            };
            handleTarget(playerTargets[state.playerFocus], 155);
        }
    } else if (state.page == UiPage::Stations) {
        if (state.stationFavoriteFocus) {
            const int slot = playableStationSlotAt(state.stationFocus);
            if (slot >= 0) queue(UiCommandKind::ToggleStationFavorite, slot);
        } else {
            selectFocusedStation();
        }
    } else if (state.page == UiPage::StationOptions) {
        const UiTarget options[] = {UiTarget::OptionsFavorite, UiTarget::OptionsInfo, UiTarget::OptionsBack};
        handleTarget(options[state.optionsFocus]);
    } else if (state.page == UiPage::StationInfo) {
        handleTarget(UiTarget::InfoBack);
    } else if (state.page == UiPage::Favorites) {
        if (!state.favoriteShowsTab) {
            if (state.favoriteTabFocus) {
                state.favoriteTabFocus = false;
                markDirty();
                return;
            }
            const int favoriteIndex = state.favoriteFocus / 2;
            const int slot = favoriteStationSlotAt(favoriteIndex);
            if (slot >= 0) {
                if ((state.favoriteFocus & 1) == 0) {
                    queue(UiCommandKind::SelectStation, slot);
                    closeToHome();
                } else {
                    queue(UiCommandKind::ToggleStationFavorite, slot);
                }
            }
        } else if (state.favoriteTabFocus) {
            state.favoriteTabFocus = false;
            markDirty();
        } else {
            openRecordedShows(true);
        }
    } else if (state.page == UiPage::RecordedShows) {
        const int count = podcastShowCount(state.showFavoritesOnly);
        if (count > 0) openEpisodes(podcastShowAt(state.showFocus, state.showFavoritesOnly));
    } else if (state.page == UiPage::ShowEpisodes) {
        if (podcastEpisodesReadyFor(state.episodeShow) && state.episodeFocus < podcastEpisodeCount) {
            queue(UiCommandKind::PlayPodcastEpisode, state.episodeShow * MAX_EPISODES + state.episodeFocus);
            openPodcastPlayer(state.episodeShow, state.episodeFocus);
        }
    } else if (state.page == UiPage::PodcastPlayer) {
        const UiTarget targets[] = {UiTarget::PodcastSeekBack, UiTarget::PodcastPause, UiTarget::PodcastSeekForward};
        handleTarget(targets[state.podcastPlayerFocus]);
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
    if (state.page == UiPage::Listening && state.playerControlFocus) {
        state.playerControlFocus = false;
        closeToHome();
    } else if (state.page == UiPage::Home || state.page == UiPage::Listening) {
        state.page = UiPage::StandbyConfirm;
        state.standbyConfirm = true;
        state.confirmAcceptFocused = false;
        markDirty();
    } else if (state.page == UiPage::StationOptions || state.page == UiPage::Favorites) {
        closeToHome();
    } else if (state.page == UiPage::StationInfo) {
        openStationOptions();
    } else if (state.page == UiPage::RecordedShows) {
        state.showFavoritesOnly ? openFavorites() : closeToHome();
    } else if (state.page == UiPage::ShowEpisodes) {
        openRecordedShows(state.showFavoritesOnly);
    } else if (state.page == UiPage::PodcastPlayer) {
        openEpisodes(state.episodeShow);
    } else {
        closeToHome();
    }
}

void uiControllerTap(UiTarget target, int value, unsigned long now, bool displayWasDimmed) {
    (void)now;
    if (alarmIsActive || displayWasDimmed) {
        markDirty();
        return;
    }
    handleTarget(target, value);
}

void uiControllerPage(int direction, unsigned long now, bool displayWasDimmed) {
    (void)now;
    if (alarmIsActive || displayWasDimmed || direction == 0) return;
    int* offset = nullptr;
    int count = 0;
    int rows = kRowsPerPage;
    switch (state.page) {
    case UiPage::Stations:
        offset = &state.stationOffset;
        count = playableStationCount();
        break;
    case UiPage::RecordedShows:
        offset = &state.showOffset;
        count = podcastShowCount(state.showFavoritesOnly);
        break;
    case UiPage::ShowEpisodes:
        if (!podcastEpisodesReadyFor(state.episodeShow)) return;
        offset = &state.episodeOffset;
        count = podcastEpisodeCount;
        break;
    case UiPage::Favorites:
        offset = &state.favoriteOffset;
        count = state.favoriteShowsTab ? podcastShowCount(true) : favoriteStationCount();
        rows = kFavoriteRowsPerPage;
        break;
    default:
        return;
    }
    const int nextOffset = constrain(*offset + (direction > 0 ? rows : -rows),
                                     0, max(0, count - rows));
    if (nextOffset == *offset) return;
    *offset = nextOffset;
    // Paging is viewport-only. It never changes which station/show/episode is
    // selected; selection remains an explicit row action.
    if (state.page == UiPage::Stations) state.stationFavoriteFocus = false;
    markDirty();
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
    renderState.playback = mediaPlaybackState();
    renderState.requestedStation = mediaRequestedStation();
    renderState.playingStation = mediaPlayingStation();
    return renderState;
}

void uiControllerMarkRendered() {
    state.dirty = false;
}
