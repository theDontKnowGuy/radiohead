#pragma once

#include <Arduino.h>

namespace BootScreen {

// Tune these together for the embedded AAC. Audio starts after the artwork is
// painted; the bar fills quickly, and the screen closes after playback + tail.
constexpr unsigned long AudioStartDelayMs = 0;
constexpr unsigned long ProgressMs = 4500;
constexpr unsigned long AfterAudioMs = 2000;
constexpr unsigned long MaxHoldMs = 15000;

// Paint the supplied 4:3 artwork edge to edge with an empty progress trough.
void draw();

// Start the firmware-embedded boot sound.
void startAudio();

// Animate from the caller's start time, then keep the screen until playback
// finishes and its tail elapses. Network joining may continue afterward.
void hold(
    unsigned long startedAt,
    bool (*stillWaiting)(),
    unsigned long maxHoldMs);

}  // namespace BootScreen
