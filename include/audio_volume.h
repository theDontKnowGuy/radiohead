#pragma once

#include <cmath>
#include <stdint.h>

// One gain scale for both backends: 0 is silent and 21 is unity gain.
// A gentle 1.5-power curve gives quiet listening more room on the dial.
constexpr uint8_t kRadioVolumeSteps = 21;
inline float radioVolumeGain(float normalized) {
    if (normalized <= 0.0f) return 0.0f;
    if (normalized >= 1.0f) return 1.0f;
    return normalized * sqrtf(normalized);
}

inline float radioVolumeDecibels(float normalized) {
    // Audio expects decibels and handles zero as true silence itself.
    const float gain = radioVolumeGain(normalized);
    return gain > 0.0f ? 20.0f * log10f(gain) : -60.0f;
}

inline uint8_t radioVolumeGain8(int index) {
    if (index <= 0) return 0;
    if (index >= kRadioVolumeSteps) return 255;
    const float normalized = static_cast<float>(index) / kRadioVolumeSteps;
    return static_cast<uint8_t>(radioVolumeGain(normalized) * 255.0f + 0.5f);
}
