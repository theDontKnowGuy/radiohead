#pragma once

#include <algorithm>
#include <stdint.h>

// Instrument the pinned LovyanGFX XPT2046 protocol without changing its
// pressure gate, rail bounds, median selection or seven-conversion sequence.
namespace stock_touch_sampling {
constexpr unsigned kSamples = 7;
constexpr unsigned kFrameBytes = 57;
constexpr unsigned kBufferBytes = 60;  // SPI FIFO helper copies whole words.
constexpr uint16_t kDiagnosticSpan = 150;  // Reporting only; never a tap gate.

struct Axis {
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0;
    uint16_t median = 0;
    uint8_t valid = 0;
    uint8_t cluster = 0;
};

struct Frame {
    Axis x, y;
    uint16_t pressure = 0;
    uint8_t pressureValid = 0;
    bool accepted = false;
};

inline void prepare(uint8_t (&data)[kBufferBytes]) {
    std::fill(data, data + kBufferBytes, 0);
    for (unsigned i = 0; i < kSamples; ++i) {
        data[8 * i] = 0x91;
        data[8 * i + 2] = 0xB1;
        data[8 * i + 4] = 0xD1;
        data[8 * i + 6] = 0xC1;
    }
    data[kFrameBytes - 1] = 0x80;
}

inline void addAxis(Axis& axis, uint16_t (&values)[kSamples], uint16_t value) {
    axis.minimum = std::min(axis.minimum, value);
    axis.maximum = std::max(axis.maximum, value);
    if (value > 128 && value <= 3968) values[axis.valid++] = value;
}

inline void summarize(Axis& axis, uint16_t (&values)[kSamples]) {
    if (!axis.valid) return;
    std::sort(values, values + axis.valid);
    axis.median = values[axis.valid >> 1];
    for (unsigned start = 0; start < axis.valid; ++start) {
        unsigned end = start;
        while (end + 1 < axis.valid && values[end + 1] - values[start] <= kDiagnosticSpan) ++end;
        axis.cluster = std::max<uint8_t>(axis.cluster, end - start + 1);
    }
}

inline Frame decode(const uint8_t* data) {
    Frame frame;
    uint16_t x[kSamples], y[kSamples], pressure[kSamples];
    for (unsigned i = 0; i < kSamples; ++i) {
        const uint8_t* d = data + i * 8;
        const uint16_t rawX = (uint16_t(d[5]) << 8 | d[6]) >> 3;
        const uint16_t rawY = (uint16_t(d[1]) << 8 | d[2]) >> 3;
        const int z = 0x3200 + rawY - rawX +
            ((int(uint16_t(d[3]) << 8 | d[4]) - int(uint16_t(d[7]) << 8 | d[8])) >> 1);
        addAxis(frame.x, x, rawX);
        addAxis(frame.y, y, rawY);
        if (z > 0) pressure[frame.pressureValid++] = z;
    }
    summarize(frame.x, x);
    summarize(frame.y, y);
    if (frame.pressureValid) {
        std::sort(pressure, pressure + frame.pressureValid);
        frame.pressure = pressure[frame.pressureValid >> 1] >> 8;
    }
    frame.accepted = frame.x.valid >= 3 && frame.y.valid >= 3 &&
        frame.pressureValid >= 3 && frame.pressure != 0;
    return frame;
}
}  // namespace stock_touch_sampling
