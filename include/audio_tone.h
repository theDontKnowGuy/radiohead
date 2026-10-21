#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

// One coherent publication from the main loop; PCM tasks own their DSP state.
inline uint32_t radioToneConfiguration(int bass, int mid, int treble) {
    return static_cast<uint32_t>(std::clamp(bass, -15, 15) + 15) |
        (static_cast<uint32_t>(std::clamp(mid, -15, 15) + 15) << 8) |
        (static_cast<uint32_t>(std::clamp(treble, -15, 15) + 15) << 16);
}
inline std::atomic<uint32_t> radioToneRequested{radioToneConfiguration(0, 0, 0)};

inline int radioToneBand(uint32_t configuration, unsigned band) {
    return static_cast<int>((configuration >> (band * 8)) & 0xffU) - 15;
}

// Match the pinned library's -12..12 dB clamp, preserving the public -15..15
// levels. Undo its global normalization only AFTER its volume control.
inline float radioToneMakeup(int bass, int mid, int treble) {
    const int boost = std::clamp(std::max({0, bass, mid, treble}), 0, 12);
    return std::pow(10.0f, boost / 20.0f);
}

// Instant attack, 100 ms release, linked stereo gain. Unlike EQ-dependent
// headroom this attenuates only when the actual post-volume signal overloads.
// No allocation, logging, network work or locks in either PCM task.
class AudioToneLimiter {
public:
    void configure(uint32_t sampleRate) {
        if (sampleRate == sampleRate_) return;
        sampleRate_ = sampleRate;
        release_ = sampleRate ? -std::expm1(-1.0f / (0.1f * sampleRate)) : 1.0f;
        reset();
    }
    void reset() { gain_ = 1.0f; }
    void process(float& left, float& right) {
        const float peak = std::max(std::fabs(left), std::fabs(right));
        const float required = peak > 1.0f ? 1.0f / peak : 1.0f;
        gain_ = std::min(required, gain_ + release_ * (1.0f - gain_));
        left *= gain_;
        right *= gain_;
    }

private:
    uint32_t sampleRate_ = 0;
    float release_ = 1.0f, gain_ = 1.0f;
};

inline int32_t radioPcm32(float value) {
    // INT32_MAX rounds up as a float: compare before converting to avoid UB.
    return value >= 1.0f ? INT32_MAX : value <= -1.0f ? INT32_MIN :
        static_cast<int32_t>(value * 2147483648.0f);
}

inline int16_t radioPcm16(float value) {
    return value >= 1.0f ? INT16_MAX : value <= -1.0f ? INT16_MIN :
        static_cast<int16_t>(value * 32768.0f);
}
