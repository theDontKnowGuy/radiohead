#pragma once

#include <Arduino.h>

namespace BootScreen {

// The progress animation has a fixed pace so a quick network join does not
// make the product mark flash past. A slower join may keep the completed bar
// visible up to the caller's existing network timeout.
constexpr unsigned long HoldMs = 7000;

// Paint the supplied 4:3 artwork edge to edge with an empty progress trough.
void draw();

// Animate from the caller's start time, then wait with a full bar only while
// stillWaiting reports outstanding startup work and maxHoldMs has not elapsed.
void hold(
    unsigned long startedAt,
    bool (*stillWaiting)(),
    unsigned long maxHoldMs);

}  // namespace BootScreen
