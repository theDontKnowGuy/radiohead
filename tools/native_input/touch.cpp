#include "touch_gesture.h"
#include "xpt2046_sampling.h"
#include "ui_controller.h"
#include <cassert>
#include <cstdio>

int currentStationIdx = 0;
int podcastEpisodeCount = 8;
bool ready = true;
int playableStationCount() { return 10; }
int playableStationSlotAt(int i) { return i >= 0 && i < 10 ? i : -1; }
int selectedPlayableStationIndex() { return 0; }
bool isStationFavorite(int) { return false; }
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
    uint8_t data[kFrameBytes];
    prepare(data);
    for (unsigned i = 0; i < kConversionsPerAxis; ++i) {
        assert(data[2 * i] == 0x91 && data[kAxisBytes + 2 * i] == 0xD1);
        assert(data[2 * i + 1] == 0 && data[kAxisBytes + 2 * i + 1] == 0);
    }
    assert(data[kFrameBytes - 1] == 0x80);
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
    assert(!readAxis(data, y));
    assert(!readAxis(data + kAxisBytes, x));
    // The valid pair may arrive at different times on each axis.
    put(0, 4, 1600); put(0, 8, 1608);
    put(1, 6, 2400); put(1, 9, 2412);
    assert(readAxis(data, y) && y == 1604);
    assert(readAxis(data + kAxisBytes, x) && x == 2406);
    // One valid reading or widely inconsistent readings are insufficient.
    put(0, 8, 4095);
    assert(!readAxis(data, y));
    put(0, 8, 2600);
    assert(!readAxis(data, y));
    // A broad finger contact can have a good cluster plus a repeated spike.
    // The old closest-pair rule incorrectly chose 900 instead of about 2000.
    const uint16_t clustered[] = {900, 900, 2000, 2004, 1996, 2002, 1998};
    for (unsigned i = 0; i < 7; ++i) put(0, i + kSettlingConversions, clustered[i]);
    assert(readAxis(data, y) && y == 2000);
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
        assert(press.sample(i % 4 != 0, i % 320, i % 240, i * 8, x, y) == TouchEvent::None);
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
    uiControllerTurn(20, 0, false);
    assert(uiControllerRenderState().stationFocus == 9);
    assert(uiControllerRenderState().stationOffset == 6);
    assert(!uiControllerTakeCommand(command));

    // A consumed wake press stays consumed after the display wakes.
    uiControllerBegin();
    TouchPressLatch wake;
    assert(wake.sample(true, 100, 100, 0, x, y) == TouchEvent::Begin);
    uiControllerTap(UiTarget::HomeRecordedShows, 0, 0, true);
    assert(uiControllerRenderState().page == UiPage::Home);
    assert(wake.sample(true, 100, 100, 40, x, y) == TouchEvent::None);
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
