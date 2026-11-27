#pragma once

#include <Arduino.h>

namespace BootScreen {

// Tune these together for the embedded AAC. Audio waits for the media boot
// mute period plus 500 ms; mode 1 takes 50% longer to fill than ProgressMs below.
// The screen closes after playback + tail and mode 1's full progress animation.
constexpr unsigned long ProgressMs = 4500;
constexpr unsigned long AfterAudioMs = 2000;
constexpr unsigned long MaxHoldMs = 15000;

// Paint this boot's selected 4:3 artwork with its mode's empty progress trough.
void draw();

// Animate the progress bar. With sound enabled, also wait for playback and
// its tail. Network joining continues independently.
void hold(unsigned long startedAt, bool playSound);

}  // namespace BootScreen
