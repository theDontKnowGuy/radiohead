#pragma once

#include <cmath>
#include <stdint.h>

// One gain scale for both backends: 0 is silent and 21 is unity gain.
// Equal amplitude steps keep the lower half useful instead of applying
// another attenuation curve to the already curved library volume control.
constexpr uint8_t kRadioVolumeSteps = 21;
inline float radioVolumeDecibels(float normalized) {
    // Audio expects decibels and handles zero as true silence itself.
    return normalized > 0.0f ? 20.0f * log10f(normalized) : -60.0f;
}

inline uint8_t radioVolumeGain8(int index) {
    if (index <= 0) return 0;
    if (index >= kRadioVolumeSteps) return 255;
    return static_cast<uint8_t>((index * 255 + kRadioVolumeSteps / 2) / kRadioVolumeSteps);
}
