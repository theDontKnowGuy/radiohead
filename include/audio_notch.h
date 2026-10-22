#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

constexpr int kNotchMinimumHz = 100;
constexpr int kNotchMaximumHz = 10000;
constexpr int kNotchDefaultHz = 3000;
constexpr uint32_t kNotchEnabledBit = 1U << 16;
constexpr unsigned kNotchEndShift = 17;
// Zero denotes a range starting at DC, never a single-frequency notch.
inline bool radioNotchValidFrequency(int frequencyHz, int endHz) {
    return (frequencyHz == 0 || (frequencyHz >= kNotchMinimumHz &&
            frequencyHz <= kNotchMaximumHz)) &&
        ((endHz == 0 && frequencyHz != 0) ||
         (endHz >= kNotchMinimumHz && endHz > frequencyHz && endHz <= kNotchMaximumHz));
}
// Preserve the original low 17 bits; zero upper endpoint means single/Q=30.
inline uint32_t radioNotchConfigurationValue(bool enabled, int frequencyHz, int endHz = 0) {
    return static_cast<uint32_t>(frequencyHz) | (enabled ? kNotchEnabledBit : 0U) |
        (static_cast<uint32_t>(endHz) << kNotchEndShift);
}
// One coherent task-safe publication; histories belong to each output task.
inline std::atomic<uint32_t> radioNotchConfiguration{kNotchDefaultHz};

class AudioNotch {
public:
    void configure(uint32_t configuration, uint32_t sampleRate) {
        if (configuration == configuration_ && sampleRate == sampleRate_) return;
        configuration_ = configuration;
        sampleRate_ = sampleRate;
        const uint32_t frequency = configuration & 0xffffU;
        const uint32_t endHz = configuration >> kNotchEndShift;
        active_ = (configuration & kNotchEnabledBit) &&
            radioNotchValidFrequency(frequency, endHz) &&
            sampleRate >= 8000 && frequency * 2U < sampleRate;
        silence_ = active_ && frequency == 0 && endHz * 2U >= sampleRate;
        for (auto& channel : history_) channel = {};
        if (!active_ || silence_) return;
        if (frequency == 0 || (endHz != 0 && endHz * 2U >= sampleRate)) {
            // The stop range touches DC or Nyquist: use the corresponding
            // Butterworth high/low pass instead of silently bypassing it.
            // https://www.w3.org/TR/audio-eq-cookbook/ (HPF/LPF, Q=1/sqrt(2))
            const bool highPass = frequency == 0;
            const float omega = 6.28318530718f * (highPass ? endHz : frequency) / sampleRate;
            const float cosine = std::cos(omega);
            const float alpha = std::sin(omega) * 0.70710678118f;
            const float inverseA0 = 1.0f / (1.0f + alpha);
            b0_ = (1.0f + (highPass ? cosine : -cosine)) * 0.5f * inverseA0;
            b1_ = (highPass ? -2.0f : 2.0f) * b0_;
            a1_ = -2.0f * cosine * inverseA0;
            a2_ = (1.0f - alpha) * inverseA0;
            return;
        }
        if (endHz != 0) {
            // Bilinear transform of H(s)=(s²+w0²)/(s²+B*s+w0²),
            // prewarping BOTH -3 dB band edges so width is correct at every rate.
            // Same notch prototype: https://www.w3.org/TR/audio-eq-cookbook/
            const float low = std::tan(3.14159265359f * frequency / sampleRate);
            const float high = std::tan(3.14159265359f * endHz / sampleRate);
            const float centerSquared = low * high;
            const float width = high - low;
            const float inverseA0 = 1.0f / (1.0f + width + centerSquared);
            b0_ = (1.0f + centerSquared) * inverseA0;
            b1_ = 2.0f * (centerSquared - 1.0f) * inverseA0;
            a1_ = b1_;
            a2_ = (1.0f - width + centerSquared) * inverseA0;
            return;
        }
        // RBJ notch, Q=30 (about 100 Hz wide at 3 kHz). Normalized by a0.
        // https://www.w3.org/TR/audio-eq-cookbook/#formulae
        const float omega = 6.28318530718f * frequency / sampleRate;
        const float alpha = std::sin(omega) / 60.0f;
        b0_ = 1.0f / (1.0f + alpha);
        b1_ = -2.0f * std::cos(omega) * b0_;
        a1_ = b1_;
        a2_ = (1.0f - alpha) * b0_;
    }

    float process(float input, unsigned channel) {
        if (!active_) return input;
        if (silence_) return 0.0f;
        auto& h = history_[channel & 1U];
        const float output = b0_ * input + b1_ * h.x1 + b0_ * h.x2 -
            a1_ * h.y1 - a2_ * h.y2;
        h.x2 = h.x1;
        h.x1 = input;
        h.y2 = h.y1;
        h.y1 = output;
        return output;
    }

    bool active() const { return active_; }

private:
    struct History { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    History history_[2];
    uint32_t configuration_ = UINT32_MAX, sampleRate_ = 0;
    float b0_ = 1, b1_ = 0, a1_ = 0, a2_ = 0;
    bool active_ = false, silence_ = false;
};
