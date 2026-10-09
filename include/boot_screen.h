#pragma once

#include <Arduino.h>

namespace BootScreen {

// Tune these together for the embedded AAC. Audio starts after the artwork is
// painted; mode 1 takes 50% longer to fill than the original ProgressMs below.
// The screen closes after playback + tail and mode 1's full progress animation.
constexpr unsigned long AudioStartDelayMs = 0;
constexpr unsigned long ProgressMs = 4500;
constexpr unsigned long AfterAudioMs = 2000;
constexpr unsigned long MaxHoldMs = 15000;

// Paint this boot's selected 4:3 artwork with its mode's empty progress trough.
void draw();

// Start the firmware-embedded boot sound.
void startAudio();

// Animate from the caller's start time, then keep the screen until playback
// finishes and its tail elapses; mode 1 also waits for its bar to finish.
// Never wait for the network here; joining continues on the QR screen.
void hold(unsigned long startedAt);

}  // namespace BootScreen
