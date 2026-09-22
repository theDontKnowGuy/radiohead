#pragma once

#include <stdint.h>

// Keep each panel axis driven through several conversions. Switching axes on
// every conversion repeatedly restarts settling, especially with light contact.
namespace xpt2046_sampling {
constexpr unsigned kConversionsPerAxis = 10;
constexpr unsigned kSettlingConversions = 3;
constexpr unsigned kAxisBytes = 2 * kConversionsPerAxis;
constexpr unsigned kFrameBytes = 2 * kAxisBytes + 1;
// LovyanGFX's ESP32 SPI helper copies whole words into its FIFO, even when
// fewer bytes are clocked. Keep the backing storage rounded up as well.
constexpr unsigned kBufferBytes = (kFrameBytes + 3) & ~3u;
constexpr uint16_t kMaximumPairDelta = 150;

struct AxisQuality {
    uint16_t minimum = 4095;
    uint16_t maximum = 0;
    unsigned valid = 0;
    unsigned cluster = 0;
};

inline void prepare(uint8_t (&data)[kBufferBytes]) {
    for (unsigned i = 0; i < kBufferBytes; ++i) data[i] = 0;
    for (unsigned i = 0; i < kConversionsPerAxis; ++i) {
        // Differential 12-bit conversions, PD0=1: drivers stay on until the
        // channel changes. Preserve the existing driver's X/Y orientation.
        data[2 * i] = 0x91;
        data[kAxisBytes + 2 * i] = 0xD1;
    }
    data[kFrameBytes - 1] = 0x80;  // Finish powered down, as in LovyanGFX.
}

inline bool readAxis(const uint8_t* data, uint16_t& coordinate,
                     AxisQuality* quality = nullptr) {
    if (quality) *quality = AxisQuality{};
    uint16_t values[kConversionsPerAxis - kSettlingConversions];
    unsigned count = 0;
    for (unsigned i = kSettlingConversions; i < kConversionsPerAxis; ++i) {
        const uint16_t value = (uint16_t(data[2 * i + 1]) << 8 | data[2 * i + 2]) >> 3;
        if (quality) {
            if (value < quality->minimum) quality->minimum = value;
            if (value > quality->maximum) quality->maximum = value;
        }
        // Retain the previous rail/no-contact guard. Do not turn floating or
        // saturated ADC readings into taps by merely lowering every threshold.
        if (value > 128 && value <= 3968) values[count++] = value;
    }
    if (quality) {
        quality->valid = count;
        quality->cluster = count;
    }
    if (count < 2) return false;
    // Prefer the largest consistent group, not the closest pair: two matching
    // spikes must not beat five good readings. Seven entries at most; no heap.
    for (unsigned i = 1; i < count; ++i) {
        const uint16_t value = values[i];
        unsigned j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    unsigned bestStart = 0, bestCount = 0, bestSpan = 4096;
    for (unsigned start = 0; start < count; ++start) {
        unsigned end = start;
        while (end + 1 < count && values[end + 1] - values[start] <= kMaximumPairDelta) ++end;
        const unsigned groupCount = end - start + 1;
        const unsigned span = values[end] - values[start];
        if (groupCount > bestCount || (groupCount == bestCount && span < bestSpan)) {
            bestStart = start;
            bestCount = groupCount;
            bestSpan = span;
        }
    }
    if (quality) quality->cluster = bestCount;
    if (bestCount < 2) return false;
    coordinate = (values[bestStart + (bestCount - 1) / 2] +
                  values[bestStart + bestCount / 2]) / 2;
    return true;
}
}  // namespace xpt2046_sampling
