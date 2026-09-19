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
    for (auto& byte : data) byte = 0;
    assert(!readAxis(data, y) && !readAxis(data + kAxisBytes, x));
    for (auto& byte : data) byte = 255;
    assert(!readAxis(data, y) && !readAxis(data + kAxisBytes, x));
}

int main() {
    checkRawSampling();
    int16_t x = 0, y = 0;
    TouchGesture tap;
    assert(tap.sample(true, 100, 100, 0, x, y) == TouchEvent::Begin);
    assert(tap.sample(false, 0, 0, 8, x, y) == TouchEvent::None);
    assert(tap.sample(false, 0, 0, 32, x, y) == TouchEvent::Tap);
    assert(tap.sample(false, 0, 0, 40, x, y) == TouchEvent::None);
    // Light contact drops out briefly; coordinate noise must not cancel it.
    TouchGesture noisy;
    noisy.sample(true, 100, 100, 0, x, y);
    noisy.sample(true, 101, 100, 8, x, y);
    assert(noisy.sample(true, 210, 180, 16, x, y) == TouchEvent::None);
    assert(noisy.sample(false, 0, 0, 32, x, y) == TouchEvent::None);
    noisy.sample(true, 102, 100, 40, x, y);
    assert(noisy.sample(false, 0, 0, 72, x, y) == TouchEvent::Tap);
    assert(x >= 100 && x <= 102 && y == 100);

    uiControllerBegin();
    uiControllerTap(UiTarget::HomeRecordedShows, 0, 0, false);
    assert(uiControllerRenderState().page == UiPage::RecordedShows);
    uiControllerSwipe(1, 0, false);
    assert(uiControllerRenderState().showOffset == 1);
    uiControllerSwipe(-1, 0, false);
    uiControllerTap(UiTarget::ShowRow0, 0, 0, false);
    UiCommand command;
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::RequestPodcastEpisodes);
    assert(!uiControllerTakeCommand(command));
    // Feed an actual gesture to the episode controller: scroll before release,
    // reverse while held, never start playback, and clamp at both list ends.
    TouchGesture swipe;
    swipe.sample(true, 100, 190, 0, x, y);
    for (int i = 1; i <= 6; ++i) {
        const auto event = swipe.sample(true, 100 + i * 3, 190 - i * 25, i * 8, x, y);
        if (event == TouchEvent::SwipeUp) uiControllerSwipe(1, i * 8, false);
        else assert(event == TouchEvent::None);
    }
    assert(uiControllerRenderState().episodeOffset > 0);
    const int beforeReverse = uiControllerRenderState().episodeOffset;
    for (int i = 1; i <= 5; ++i) {
        const auto event = swipe.sample(true, 118, 40 + i * 28, 48 + i * 8, x, y);
        if (event == TouchEvent::SwipeDown) uiControllerSwipe(-1, 0, false);
        else assert(event == TouchEvent::None);
    }
    assert(uiControllerRenderState().episodeOffset < beforeReverse);
    assert(swipe.sample(false, 0, 0, 120, x, y) == TouchEvent::None);
    assert(!uiControllerTakeCommand(command));
    for (int i = 0; i < 20; ++i) uiControllerSwipe(1, 0, false);
    assert(uiControllerRenderState().episodeOffset == 3);
    uiControllerTap(UiTarget::EpisodeRow4, 0, 0, false);
    assert(uiControllerTakeCommand(command) && command.kind == UiCommandKind::PlayPodcastEpisode && command.value == 7);
    uiControllerTap(UiTarget::PodcastBack, 0, 0, false);
    assert(uiControllerTakeCommand(command));
    for (int i = 0; i < 20; ++i) uiControllerSwipe(-1, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerSwipe(1, 0, true);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerSetAlarmActive(true);
    uiControllerSwipe(1, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    uiControllerSetAlarmActive(false);
    ready = false;
    uiControllerSwipe(1, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    ready = true;
    podcastEpisodeCount = 2;
    uiControllerSwipe(1, 0, false);
    assert(uiControllerRenderState().episodeOffset == 0);
    // Horizontal scrubbing remains distinct from a tap or vertical scrolling.
    TouchGesture horizontal;
    horizontal.sample(true, 140, 115, 0, x, y);
    horizontal.sample(true, 190, 116, 8, x, y);
    assert(horizontal.sample(false, 0, 0, 40, x, y) == TouchEvent::HorizontalDrag);
    assert(x == 190);
    TouchGesture held;
    held.sample(true, 100, 100, 0, x, y);
    for (int i = 1; i <= 300; ++i) {
        assert(held.sample(true, 100 + i % 3, 100, i * 8, x, y) == TouchEvent::None);
    }
    assert(held.sample(false, 0, 0, 2440, x, y) == TouchEvent::Tap);
    TouchGesture diagonal;
    diagonal.sample(true, 100, 100, 0, x, y);
    assert(diagonal.sample(true, 130, 130, 8, x, y) == TouchEvent::None);
    assert(diagonal.sample(false, 0, 0, 40, x, y) == TouchEvent::None);
    // Millisecond rollover must not create a stuck press.
    TouchGesture wrap;
    wrap.sample(true, 100, 100, UINT32_MAX - 10, x, y);
    assert(wrap.sample(false, 0, 0, 20, x, y) == TouchEvent::Tap);
    puts("Raw touch sampling, gestures and production list controller checks passed");
}
