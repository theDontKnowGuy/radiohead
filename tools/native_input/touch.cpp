#include "touch_gesture.h"
#include "xpt2046_sampling.h"
#include "ui_controller.h"
#include "settings.h"
#include <cassert>
#include <cstdio>
#include <climits>

int currentStationIdx = 0;
int gB = 0, gM = 0, gT = 0;
uint16_t autoDimSeconds = 30;
bool firmwareAutoUpdate = true;
uint16_t normalizeAutoDimSeconds(uint16_t seconds) {
    for (const uint16_t option : {0, 15, 30, 60, 120, 300}) {
        if (seconds == option) return option;
    }
    return 30;
}
int podcastEpisodeCount = 8;
bool ready = true;
bool favoriteEnabled = false;
int playableStationCount() { return 10; }
int playableStationSlotAt(int i) { return i >= 0 && i < 10 ? i : -1; }
int selectedPlayableStationIndex() { return 0; }
bool isStationFavorite(int slot) { return favoriteEnabled && slot == 0; }
bool isPodcastShowFavorite(int i) { return i % 2 == 0; }
bool podcastEpisodesReadyFor(int show) { return ready && show == 0; }
PodcastPlaybackSnapshot podcastPlaybackSnapshot() { return {}; }
PlaybackState mediaPlaybackState() { return PlaybackState::Stopped; }
int mediaRequestedStation() { return -1; }
int mediaPlayingStation() { return -1; }

// Synthetic ADC results exercise production frame offsets, settling exclusion,
// independent axes and no-contact rejection; they do not simulate panel physics.
void checkRawSampling() {
    using namespace xpt2046_sampling;
    uint8_t data[kBufferBytes];
    prepare(data);
    for (unsigned i = 0; i < kConversionsPerAxis; ++i) {
        assert(data[2 * i] == 0x91 && data[kAxisBytes + 2 * i] == 0xD1);
        assert(data[2 * i + 1] == 0 && data[kAxisBytes + 2 * i + 1] == 0);
    }
    assert(data[kFrameBytes - 1] == 0x80);
    static_assert(kBufferBytes >= ((kFrameBytes + 3) & ~3u), "SPI word copy must fit");
    for (unsigned i = kFrameBytes; i < kBufferBytes; ++i) assert(data[i] == 0);
    auto put = [&](unsigned axis, unsigned i, uint16_t value) {
        const unsigned offset = axis * kAxisBytes + 2 * i + 1;
        data[offset] = (value << 3) >> 8;
        data[offset + 1] = (value << 3) & 255;
    };
    for (unsigned axis = 0; axis < 2; ++axis) {
        for (unsigned i = 0; i < kConversionsPerAxis; ++i) {
            put(axis, i, i < kSettlingConversions ? 500 : 4095);
        }
    }
    // Early readings agree, but must be discarded after an axis switch.
    uint16_t x = 0, y = 0;
    AxisQuality quality;
    assert(!readAxis(data, y, &quality));
    assert(quality.valid == 0 && quality.cluster == 0);
    assert(quality.minimum == 4095 && quality.maximum == 4095);
    assert(!readAxis(data + kAxisBytes, x));
    // The valid pair may arrive at different times on each axis.
    put(0, 4, 1600); put(0, 8, 1608);
    put(1, 6, 2400); put(1, 9, 2412);
    assert(readAxis(data, y, &quality) && y == 1604);
    assert(quality.valid == 2 && quality.cluster == 2);
    assert(quality.minimum == 1600 && quality.maximum == 4095);
    assert(readAxis(data + kAxisBytes, x) && x == 2406);
    // One valid reading or widely inconsistent readings are insufficient.
    put(0, 8, 4095);
    assert(!readAxis(data, y, &quality));
    assert(quality.valid == 1 && quality.cluster == 1);
    put(0, 8, 2600);
    assert(!readAxis(data, y, &quality));
    assert(quality.valid == 2 && quality.cluster == 1);
    // A broad finger contact can have a good cluster plus a repeated spike.
    // The old closest-pair rule incorrectly chose 900 instead of about 2000.
    const uint16_t clustered[] = {900, 900, 2000, 2004, 1996, 2002, 1998};
    for (unsigned i = 0; i < 7; ++i) put(0, i + kSettlingConversions, clustered[i]);
    assert(readAxis(data, y, &quality) && y == 2000);
    assert(quality.valid == 7 && quality.cluster == 5);
    for (auto& byte : data) byte = 0;
    assert(!readAxis(data, y) && !readAxis(data + kAxisBytes, x));
    for (auto& byte : data) byte = 255;
    assert(!readAxis(data, y) && !readAxis(data + kAxisBytes, x));
}

int main() {
    checkRawSampling();
    int16_t x = 0, y = 0;
    TouchPressLatch press;
    // First accepted contact fires immediately, before release or more samples.
    assert(press.sample(true, 100, 100, 0, x, y) == TouchEvent::Begin);
    assert(x == 100 && y == 100);
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeRecordedShows, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::RecordedShows);
    // Holding, sliding and short weak-contact dropouts cannot activate the new
    // page. Neither release nor a continuous hold emits another click.
    for (int i = 1; i <= 300; ++i) {
        assert(press.sample(i % 4 != 0, i % 320, i % 240, i * 8, x, y) ==
               (i % 4 != 0 ? TouchEvent::Contact : TouchEvent::None));
        uiControllerTouchContact(UiTarget::ShowRow0, i * 8);
    }
    UiCommand command;
    assert(!uiControllerTakeCommand(command));
    assert(press.sample(false, 0, 0, 2440, x, y) == TouchEvent::None);
    assert(press.sample(false, 0, 0, 2480, x, y) == TouchEvent::Release);
    assert(press.sample(false, 0, 0, 2488, x, y) == TouchEvent::None);
    assert(press.sample(true, 120, 70, 2496, x, y) == TouchEvent::Begin);
    assert(x == 120 && y == 70);

    // Paging never starts playback; bounds and filtered collections are honored.
    uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().showOffset == 4);
    assert(uiControllerRenderState().showFocus == 0);
    uiControllerTap(UiTarget::ListPrevious, 0, 0, false);
    assert(uiControllerRenderState().showOffset == 0);
    uiControllerTap(UiTarget::ShowRow0, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::RequestPodcastEpisodes);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().episodeOffset == 4);
    assert(uiControllerRenderState().episodeFocus == 0);
    for (int i = 0; i < 20; ++i) uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().episodeOffset == 4);
    assert(!uiControllerTakeCommand(command));
    // The fourth row is visible; obsolete fifth-row commands remain inert.
    uiControllerTap(UiTarget::EpisodeRow4, 0, 0, false);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::EpisodeRow3, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PlayPodcastEpisode && command.value == 7);
    uiControllerTap(UiTarget::PodcastBack, 0, 0, false);
    assert(uiControllerTakeCommand(command));
    for (int i = 0; i < 20; ++i) uiControllerTap(UiTarget::ListPrevious, 0, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerTap(UiTarget::ListNext, 0, 0, true);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerSetAlarmActive(true);
    uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerSetAlarmActive(false);
    ready = false;
    uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    ready = true;
    for (int count : {0, 2}) {
        podcastEpisodeCount = count;
        uiControllerTap(UiTarget::ListNext, 0, 0, false);
        assert(uiControllerRenderState().episodeOffset == 0);
    }
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeLiveRadio, 0, 0, false);
    uiControllerTap(UiTarget::ListNext, 0, 0, false);
    assert(uiControllerRenderState().stationOffset == 4);
    assert(uiControllerRenderState().stationFocus == 0);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::ListPrevious, 0, 0, false);
    assert(uiControllerRenderState().stationOffset == 0);

    // The encoder is global volume/mute/power control. It must not move a
    // list focus, activate a selected item, or navigate away from this page.
    uiControllerTurn(20, 0, false);
    assert(uiControllerRenderState().page == UiPage::Stations);
    assert(uiControllerRenderState().stationFocus == 0);
    assert(uiControllerRenderState().stationOffset == 0);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ChangeVolume && command.value == 20);
    uiControllerPush(0, false);
    assert(uiControllerRenderState().page == UiPage::Stations);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ToggleMute);
    uiControllerHold(0, false);
    assert(uiControllerRenderState().page == UiPage::Stations);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::EnterStandby);
    assert(!uiControllerTakeCommand(command));

    // A live-station selection returns to Home before playback starts, so a
    // blocking stream connection cannot leave the old list looking frozen.
    uiControllerTap(UiTarget::ListRow1, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::Home);
    assert(uiControllerRenderState().homeStationPreview == 1);
    assert(!uiControllerTakeCommand(command));
    uiControllerMarkRendered();
    assert(uiControllerRenderState().homeStationPreview == -1);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::SelectStation && command.value == 1);
    assert(!uiControllerTakeCommand(command));

    // Favorite-station touch selections take the same Home-first route as the
    // regular station list. Encoder input remains volume/mute/power only.
    favoriteEnabled = true;
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeFavorites, 0, 0, false);
    uiControllerTap(UiTarget::FavoritesRow0, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::Home);
    assert(uiControllerRenderState().homeStationPreview == 0);
    assert(!uiControllerTakeCommand(command));
    uiControllerMarkRendered();
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::SelectStation && command.value == 0);
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeFavorites, 0, 0, false);
    const UiPage favoritePage = uiControllerRenderState().page;
    uiControllerTurn(-1, 0, false);
    assert(uiControllerRenderState().page == favoritePage);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ChangeVolume && command.value == -1);
    uiControllerPush(0, false);
    assert(uiControllerRenderState().page == favoritePage);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ToggleMute);
    favoriteEnabled = false;

    // Settings are touch-owned. Tone previews leave committed values alone, while the
    // global encoder contract remains volume/mute/power on these pages.
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeSettings, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::Settings);
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::SettingsAudio);
    uiControllerTap(UiTarget::ToneBassIncrease, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone &&
           command.value == 1 && command.secondary == 0 && command.tertiary == 0);
    assert(gB == 0 && gM == 0 && gT == 0);
    uiControllerTap(UiTarget::ToneTrebleDecrease, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone &&
           command.value == 1 && command.tertiary == -1);
    assert(uiControllerRenderState().toneBassDraft == 1);
    assert(uiControllerRenderState().toneTrebleDraft == -1);
    assert(gB == 0 && gM == 0 && gT == 0);
    uiControllerTap(UiTarget::ToneCancel, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::Settings);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone &&
           command.value == 0 && command.secondary == 0 && command.tertiary == 0);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    uiControllerTap(UiTarget::ToneMidIncrease, 0, 0, false);
    uiControllerTap(UiTarget::ToneSave, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ApplyTone &&
           command.value == 0 && command.secondary == 1 && command.tertiary == 0);
    uiControllerTap(UiTarget::SettingsRow1, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 30);
    uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 60);
    uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 120);
    uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 300);
    uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == AUTO_DIM_NEVER_SECONDS);
    uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == AUTO_DIM_NEVER_SECONDS);
    uiControllerTap(UiTarget::DimDecrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 300);
    uiControllerTap(UiTarget::DimCancel, 0, 0, false);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::SettingsRow1, 0, 0, false);
    uiControllerTap(UiTarget::DimDecrease, 0, 0, false);
    assert(uiControllerRenderState().dimSecondsDraft == 15);
    uiControllerTap(UiTarget::DimSave, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ApplyAutoDim && command.value == 15);
    uiControllerTap(UiTarget::SettingsRow1, 0, 0, false);
    for (int step = 0; step < 5; ++step) {
        uiControllerTap(UiTarget::DimIncrease, 0, 0, false);
    }
    assert(uiControllerRenderState().dimSecondsDraft == AUTO_DIM_NEVER_SECONDS);
    uiControllerTap(UiTarget::DimSave, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ApplyAutoDim &&
           command.value == AUTO_DIM_NEVER_SECONDS);
    uiControllerTap(UiTarget::SettingsNext, 0, 0, false);
    assert(uiControllerRenderState().settingsOffset == 0);
    uiControllerTap(UiTarget::SettingsRow3, 0, 0, false);
    uiControllerTap(UiTarget::DeviceFactoryReset, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::SettingsConfirm);
    uiControllerTap(UiTarget::SettingsBack, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::SettingsDevice);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::DeviceFactoryReset, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::SettingsConfirm);
    uiControllerTap(UiTarget::SettingsConfirmCancel, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::SettingsDevice);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::DeviceRestart, 0, 0, false);
    uiControllerTap(UiTarget::SettingsConfirmAccept, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::RestartDevice);
    uiControllerTurn(1, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ChangeVolume && command.value == 1);

    // Hold-to-repeat starts after 450 ms, steps every 120 ms, and stops on
    // release or leaving the original target. It never catches up in a burst.
    uiControllerBegin();
    uiControllerTap(UiTarget::HomeSettings, 0, 0, false);
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    uiControllerTap(UiTarget::ToneBassIncrease, 0, 100, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone);
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 549);
    assert(!uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 550);
    assert(uiControllerTakeCommand(command) && command.value == 2);
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 669);
    assert(!uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 670);
    assert(uiControllerTakeCommand(command) && command.value == 3);
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 5000);
    assert(uiControllerTakeCommand(command) && command.value == 4);
    uiControllerTouchEnd();
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 6000);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::ToneBassIncrease, 0, 6100, false);
    assert(uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneMidIncrease, 6500);
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 7000);
    assert(!uiControllerTakeCommand(command));

    // Bounds for all six buttons, coalesced preview, and encoder independence.
    for (const auto target : {UiTarget::ToneBassDecrease, UiTarget::ToneBassIncrease,
                             UiTarget::ToneMidDecrease, UiTarget::ToneMidIncrease,
                             UiTarget::ToneTrebleDecrease, UiTarget::ToneTrebleIncrease}) {
        uiControllerTap(target, 0, 0, false);
        for (unsigned long step = 0; step < 40; ++step) {
            uiControllerTouchContact(target, 450 + step * 120);
        }
        assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone);
        const bool decrease = target == UiTarget::ToneBassDecrease ||
                              target == UiTarget::ToneMidDecrease || target == UiTarget::ToneTrebleDecrease;
        const int value = target <= UiTarget::ToneBassIncrease ? command.value :
                          target <= UiTarget::ToneMidIncrease ? command.secondary : command.tertiary;
        assert(value == (decrease ? -15 : 15));
        uiControllerTouchContact(target, 10000);
        assert(!uiControllerTakeCommand(command));
    }
    uiControllerPush(11000, false);
    uiControllerTap(UiTarget::ToneMidDecrease, 0, 11000, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ToggleMute);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone && command.secondary == 14);
    uiControllerTap(UiTarget::SettingsBack, 0, 12000, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone &&
           command.value == 0 && command.secondary == 0 && command.tertiary == 0);
    uiControllerTouchContact(UiTarget::ToneMidDecrease, 13000);
    assert(!uiControllerTakeCommand(command));

    // A web commit supersedes the open draft, including a pending preview.
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    uiControllerTap(UiTarget::ToneBassIncrease, 0, 0, false);
    gB = 7; gM = -3; gT = 4;
    uiControllerTick(500);
    assert(uiControllerRenderState().toneBassDraft == 7);
    assert(!uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 600);
    assert(!uiControllerTakeCommand(command));
    uiControllerTap(UiTarget::ToneCancel, 0, 700, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PreviewTone &&
           command.value == 7 && command.secondary == -3 && command.tertiary == 4);
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    uiControllerTap(UiTarget::ToneBassIncrease, 0, 0, false);
    gB = 8;
    uiControllerTap(UiTarget::ToneSave, 0, 700, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::ApplyTone && command.value == 8);
    assert(!uiControllerTakeCommand(command));

    // Timer wrap and alarm suppression cannot manufacture another tap.
    uiControllerTap(UiTarget::SettingsRow2, 0, 0, false);
    uiControllerTap(UiTarget::ToneMidIncrease, 0, ULONG_MAX - 200, false);
    assert(uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneMidIncrease, 248);
    assert(!uiControllerTakeCommand(command));
    uiControllerTouchContact(UiTarget::ToneMidIncrease, 249);
    assert(uiControllerTakeCommand(command) && command.secondary == -1);
    uiControllerSetAlarmActive(true);
    uiControllerSetAlarmActive(false);
    uiControllerTouchContact(UiTarget::ToneMidIncrease, 900);
    assert(!uiControllerTakeCommand(command));
    gB = gM = gT = 0;

    // A consumed wake press stays consumed after the display wakes.
    uiControllerBegin();
    TouchPressLatch wake;
    assert(wake.sample(true, 100, 100, 0, x, y) == TouchEvent::Begin);
    uiControllerTap(UiTarget::HomeRecordedShows, 0, 0, true);
    assert(uiControllerRenderState().page == UiPage::Home);
    assert(wake.sample(true, 100, 100, 40, x, y) == TouchEvent::Contact);
    uiControllerTouchContact(UiTarget::ToneBassIncrease, 600);
    assert(!uiControllerTakeCommand(command));
    assert(wake.sample(false, 0, 0, 120, x, y) == TouchEvent::Release);
    assert(wake.sample(true, 100, 100, 128, x, y) == TouchEvent::Begin);
    uiControllerTap(UiTarget::HomeRecordedShows, 0, 128, false);
    assert(uiControllerRenderState().page == UiPage::RecordedShows);
    TouchPressLatch wrap;
    assert(wrap.sample(true, 100, 100, UINT32_MAX - 10, x, y) == TouchEvent::Begin);
    assert(wrap.sample(false, 0, 0, 20, x, y) == TouchEvent::None);
    assert(wrap.sample(false, 0, 0, 80, x, y) == TouchEvent::Release);
    puts("Raw sampling, immediate press/re-arm and production paging checks passed");
}
