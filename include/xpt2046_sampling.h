#pragma once

#include <stdint.h>

// Keep each panel axis driven through several conversions. Switching axes on
// every conversion repeatedly restarts settling, especially with light contact.
namespace xpt2046_sampling {
constexpr unsigned kConversionsPerAxis = 10;
constexpr unsigned kSettlingConversions = 3;
constexpr unsigned kAxisBytes = 2 * kConversionsPerAxis;
constexpr unsigned kFrameBytes = 2 * kAxisBytes + 1;
constexpr uint16_t kMaximumPairDelta = 150;

inline void prepare(uint8_t (&data)[kFrameBytes]) {
    for (unsigned i = 0; i < kFrameBytes; ++i) data[i] = 0;
    for (unsigned i = 0; i < kConversionsPerAxis; ++i) {
        // Differential 12-bit conversions, PD0=1: drivers stay on until the
        // channel changes. Preserve the existing driver's X/Y orientation.
        data[2 * i] = 0x91;
        data[kAxisBytes + 2 * i] = 0xD1;
    }
    data[kFrameBytes - 1] = 0x80;  // Finish powered down, as in LovyanGFX.
}

inline bool readAxis(const uint8_t* data, uint16_t& coordinate) {
    uint16_t values[kConversionsPerAxis - kSettlingConversions];
    unsigned count = 0;
    for (unsigned i = kSettlingConversions; i < kConversionsPerAxis; ++i) {
        const uint16_t value = (uint16_t(data[2 * i + 1]) << 8 | data[2 * i + 2]) >> 3;
        // Retain the previous rail/no-contact guard. Do not turn floating or
        // saturated ADC readings into taps by merely lowering every threshold.
        if (value > 128 && value <= 3968) values[count++] = value;
    }
    if (count < 2) return false;
    unsigned bestDelta = 4096;
    uint16_t result = 0;
    for (unsigned i = 0; i + 1 < count; ++i) {
        for (unsigned j = i + 1; j < count; ++j) {
            const unsigned delta = values[i] > values[j]
                ? values[i] - values[j] : values[j] - values[i];
            if (delta < bestDelta) {
                bestDelta = delta;
                result = (values[i] + values[j]) / 2;
            }
        }
    }
    if (bestDelta > kMaximumPairDelta) return false;
    coordinate = result;
    return true;
}
}  // namespace xpt2046_sampling
