#pragma once

#include <Arduino.h>

namespace BootScreen {

// The boot audio starts after the media mute period plus 500 ms. With sound,
// the screen holds through playback and its tail, up to MaxHoldMs.
constexpr unsigned long ProgressMs = 7500;
constexpr unsigned long NewArtworkProgressMs = 9750;
constexpr unsigned long AfterAudioMs = 5000;
constexpr unsigned long MaxHoldMs = 18000;

// Paint this boot's selected 4:3 artwork with its mode's empty progress trough.
void draw();

// Animate the progress bar. With sound enabled, also wait for playback and
// its tail. Network joining continues independently.
void hold(unsigned long startedAt, bool playSound);

}  // namespace BootScreen
