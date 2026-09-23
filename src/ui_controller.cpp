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
constexpr int kSettingsItemCount = 5;

UiRenderState state;
UiCommand pendingCommand;
bool hasPendingCommand = false;
int pendingVolumeDelta = 0;
bool alarmIsActive = false;
unsigned long volumeOverlayUntil = 0;
UiCommand pendingToneCommand;
bool hasPendingToneCommand = false;
int committedBass = 0, committedMid = 0, committedTreble = 0;
UiTarget repeatTarget = UiTarget::None;
unsigned long repeatLastAt = 0;
bool repeatStarted = false;
constexpr unsigned long kToneHoldDelayMs = 450;
constexpr unsigned long kToneRepeatMs = 120;

void openToneSettings();
void openDisplaySettings();
void openDeviceSettings(bool clearFailure);
void openFirmwareSettings();

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

void queueTone(UiCommandKind kind, int bass, int mid, int treble) {
    // Keep previews independent of simultaneous encoder commands. Save/Cancel
    // replaces any unconsumed preview, so an old preview cannot run afterward.
    pendingToneCommand = {kind, bass, mid, treble};
    hasPendingToneCommand = true;
}

void loadCommittedTone() {
    state.toneBassDraft = committedBass = gB;
    state.toneMidDraft = committedMid = gM;
    state.toneTrebleDraft = committedTreble = gT;
}

void syncCommittedTone() {
    if (state.page != UiPage::SettingsAudio ||
        (committedBass == gB && committedMid == gM && committedTreble == gT)) return;
    // A newer web edit supersedes the draft; neither Save nor Cancel may
    // overwrite it with the values that were present when the editor opened.
    loadCommittedTone();
    hasPendingToneCommand = false;
    repeatTarget = UiTarget::None;
    markDirty();
}

bool isToneAdjustment(UiTarget target) {
    return target >= UiTarget::ToneBassDecrease && target <= UiTarget::ToneTrebleIncrease;
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

void openSettings(bool fromHome = false) {
    state.page = UiPage::Settings;
    if (fromHome) state.settingsOffset = 0;
    state.settingsConfirmAction = 0;
    markDirty();
}

void openSettingsWebHandoff(uint8_t handoff) {
    state.page = UiPage::SettingsWebHandoff;
    state.settingsWebHandoff = handoff;
    markDirty();
}

void openSettingsItem(int item) {
    switch (item) {
    case 0: openSettingsWebHandoff(0); break;  // Network
    case 1: openDisplaySettings(); break;
    case 2: openToneSettings(); break;
    case 3: openSettingsWebHandoff(1); break;  // Weather & Time
    case 4: openDeviceSettings(true); break;
    default: break;
    }
}

void openToneSettings() {
    state.page = UiPage::SettingsAudio;
    loadCommittedTone();
    markDirty();
}

void openDisplaySettings() {
    state.page = UiPage::SettingsDisplay;
    state.dimSecondsDraft = normalizeAutoDimSeconds(autoDimSeconds);
    markDirty();
}

void openDeviceSettings(bool clearFailure = false) {
    state.page = UiPage::SettingsDevice;
    if (clearFailure) state.deviceActionFailed = false;
    markDirty();
}

void openFirmwareSettings() {
    state.page = UiPage::SettingsFirmware;
    markDirty();
}

void openSettingsConfirmation(uint8_t action) {
    state.page = UiPage::SettingsConfirm;
    state.settingsConfirmAction = action;
    markDirty();
}

uint16_t adjustedDimSeconds(uint16_t current, int direction) {
    static constexpr uint16_t kChoices[] = {15, 30, 60, 120};
    int selected = 0;
    for (int index = 0; index < static_cast<int>(sizeof(kChoices) / sizeof(kChoices[0])); ++index) {
        if (kChoices[index] == current) {
            selected = index;
            break;
        }
    }
    selected = constrain(selected + direction, 0,
                         static_cast<int>(sizeof(kChoices) / sizeof(kChoices[0])) - 1);
    return kChoices[selected];
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
            openSettings(true);
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
    case UiPage::Settings:
        if (target == UiTarget::SettingsBack) {
            closeToHome();
        } else if (target == UiTarget::SettingsPrevious || target == UiTarget::SettingsNext) {
            uiControllerPage(target == UiTarget::SettingsNext ? 1 : -1, 0, false);
        } else if (target >= UiTarget::SettingsRow0 && target <= UiTarget::SettingsRow3) {
            const int row = static_cast<int>(target) - static_cast<int>(UiTarget::SettingsRow0);
            const int item = state.settingsOffset + row;
            if (item < kSettingsItemCount) openSettingsItem(item);
        }
        break;
    case UiPage::SettingsAudio:
        if (target == UiTarget::SettingsBack || target == UiTarget::ToneCancel) {
            queueTone(UiCommandKind::PreviewTone, gB, gM, gT);
            openSettings();
        } else if (isToneAdjustment(target)) {
            int* draft = target <= UiTarget::ToneBassIncrease ? &state.toneBassDraft :
                         target <= UiTarget::ToneMidIncrease ? &state.toneMidDraft :
                                                              &state.toneTrebleDraft;
            const bool decrease = target == UiTarget::ToneBassDecrease ||
                                  target == UiTarget::ToneMidDecrease ||
                                  target == UiTarget::ToneTrebleDecrease;
            const int next = constrain(*draft + (decrease ? -1 : 1), -15, 15);
            if (*draft != next) {
                *draft = next;
                queueTone(UiCommandKind::PreviewTone, state.toneBassDraft,
                          state.toneMidDraft, state.toneTrebleDraft);
                markDirty();
            }
        } else if (target == UiTarget::ToneSave) {
            queueTone(UiCommandKind::ApplyTone, state.toneBassDraft,
                      state.toneMidDraft, state.toneTrebleDraft);
            openSettings();
        }
        break;
    case UiPage::SettingsDisplay:
        if (target == UiTarget::SettingsBack || target == UiTarget::DimCancel) {
            openSettings();
        } else if (target == UiTarget::DimDecrease) {
            state.dimSecondsDraft = adjustedDimSeconds(state.dimSecondsDraft, -1);
            markDirty();
        } else if (target == UiTarget::DimIncrease) {
            state.dimSecondsDraft = adjustedDimSeconds(state.dimSecondsDraft, 1);
            markDirty();
        } else if (target == UiTarget::DimSave) {
            queue(UiCommandKind::ApplyAutoDim, state.dimSecondsDraft);
            openSettings();
        }
        break;
    case UiPage::SettingsDevice:
        if (target == UiTarget::SettingsBack) {
            openSettings();
        } else if (target == UiTarget::DeviceFirmware) {
            openFirmwareSettings();
        } else if (target == UiTarget::DeviceCalibration) {
            queue(UiCommandKind::StartTouchCalibration);
        } else if (target == UiTarget::DeviceRestart) {
            openSettingsConfirmation(1);
        } else if (target == UiTarget::DeviceFactoryReset) {
            openSettingsConfirmation(2);
        }
        break;
    case UiPage::SettingsFirmware:
        if (target == UiTarget::SettingsBack) {
            openDeviceSettings();
        } else if (target == UiTarget::FirmwareCheckNow) {
            queue(UiCommandKind::RequestFirmwareUpdateCheck);
            markDirty();
        } else if (target == UiTarget::FirmwareInstallNow) {
            queue(UiCommandKind::RequestFirmwareUpdateInstall);
            markDirty();
        } else if (target == UiTarget::FirmwareToggleAutoInstall) {
            queue(UiCommandKind::SetFirmwareAutoInstall, firmwareAutoUpdate ? 0 : 1);
            markDirty();
        }
        break;
    case UiPage::SettingsWebHandoff:
        if (target == UiTarget::SettingsBack) openSettings();
        break;
    case UiPage::SettingsConfirm:
        if (target == UiTarget::SettingsConfirmAccept) {
            if (state.settingsConfirmAction == 1) queue(UiCommandKind::RestartDevice);
            if (state.settingsConfirmAction == 2) queue(UiCommandKind::FactoryResetDevice);
            openDeviceSettings();
        } else if (target == UiTarget::SettingsConfirmCancel ||
                   target == UiTarget::SettingsBack) {
            openDeviceSettings();
        }
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
    hasPendingToneCommand = false;
    repeatTarget = UiTarget::None;
}

void uiControllerTurn(int detents, unsigned long now, bool displayWasDimmed) {
    (void)displayWasDimmed;
    if (detents == 0 || alarmIsActive) return;

    // The encoder is deliberately device-wide. Touch owns navigation,
    // selection and confirmation, so a turn cannot change the page or focus.
    queue(UiCommandKind::ChangeVolume, detents);
    volumeOverlayUntil = now + kVolumeOverlayMs;
    markDirty();
}

void uiControllerPush(unsigned long now, bool displayWasDimmed) {
    (void)displayWasDimmed;
    if (alarmIsActive) return;

    // A press is mute only; it cannot activate the focused item or accept a
    // dialog after navigation has changed the current page.
    queue(UiCommandKind::ToggleMute);
    volumeOverlayUntil = now + kVolumeOverlayMs;
    markDirty();
}

void uiControllerHold(unsigned long now, bool displayWasDimmed) {
    (void)now;
    (void)displayWasDimmed;
    if (alarmIsActive) return;

    // The button classifier fires Hold once and suppresses Push on release.
    queue(UiCommandKind::EnterStandby);
}

void uiControllerTap(UiTarget target, int value, unsigned long now, bool displayWasDimmed) {
    repeatTarget = UiTarget::None;
    syncCommittedTone();
    if (alarmIsActive || displayWasDimmed) {
        markDirty();
        return;
    }
    if (state.page == UiPage::SettingsAudio && isToneAdjustment(target)) {
        repeatTarget = target;
        repeatLastAt = now;
        repeatStarted = false;
    }
    handleTarget(target, value);
}

void uiControllerTouchContact(UiTarget target, unsigned long now) {
    syncCommittedTone();
    if (alarmIsActive || state.page != UiPage::SettingsAudio || target != repeatTarget) {
        repeatTarget = UiTarget::None;
        return;
    }
    if (repeatTarget == UiTarget::None) return;
    const unsigned long interval = repeatStarted ? kToneRepeatMs : kToneHoldDelayMs;
    if (now - repeatLastAt < interval) return;
    repeatStarted = true;
    repeatLastAt = now;
    // At most one step per fresh contact, never a blocking catch-up burst.
    handleTarget(target, 0);
}

void uiControllerTouchEnd() {
    repeatTarget = UiTarget::None;
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
    case UiPage::Settings:
        offset = &state.settingsOffset;
        count = kSettingsItemCount;
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
    if (active) repeatTarget = UiTarget::None;
    alarmIsActive = active;
}

void uiControllerReportDeviceActionFailure() {
    state.page = UiPage::SettingsDevice;
    state.deviceActionFailed = true;
    markDirty();
}

void uiControllerTick(unsigned long now) {
    syncCommittedTone();
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
        if (!hasPendingToneCommand) return false;
        command = pendingToneCommand;
        hasPendingToneCommand = false;
        return true;
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
