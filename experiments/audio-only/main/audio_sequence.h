#pragma once
// One sample-counted recording sequence, shared verbatim with the WAV exporter.
// Integer-only rendering keeps host and Xtensa output identical after quantization.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "sequence_tones.h"

namespace audio_test {
constexpr uint32_t kSequenceRate = 44100;
constexpr uint32_t kSequenceFade = kSequenceRate * 30 / 1000;
struct SequenceSegment {
    const char* name;
    uint32_t startSeconds, durationSeconds;
    int tone; // 0/1/2 = 250/1000/combined; 3 = existing left music source
    bool higher;
};
inline constexpr SequenceSegment kSequenceSegments[] = {
    {"250 Hz low",7,5,0,false}, {"1 kHz low",14,5,1,false},
    {"250 Hz + 1 kHz low",21,5,2,false}, {"reference music low",28,10,3,false},
    {"250 Hz +6 dB",40,5,0,true}, {"1 kHz +6 dB",47,5,1,true},
    {"250 Hz + 1 kHz +6 dB",54,5,2,true}, {"reference music +6 dB",61,10,3,true}
};
constexpr uint32_t kSequenceEnd = 71 * kSequenceRate;
// WAV includes the first two seconds of the indefinite final digital silence.
constexpr uint32_t kSequenceReferenceFrames = 73 * kSequenceRate;

struct RecordingSequence {
    const int16_t* music;
    size_t musicFrames;
    uint64_t frame = 0;
    void next(int16_t& left, int16_t& right) {
        left = right = 0;
        if (frame < kSequenceEnd) {
            for (const auto& segment : kSequenceSegments) {
                const uint32_t start = segment.startSeconds * kSequenceRate;
                const uint32_t length = segment.durationSeconds * kSequenceRate;
                if (frame < start || frame >= start + length) continue;
                const uint32_t local = static_cast<uint32_t>(frame - start);
                const int16_t source = segment.tone == 3
                    ? (musicFrames ? music[(local % musicFrames) * 2] : 0)
                    : kSequenceTones[segment.tone][local % kSequencePeriod];
                const uint32_t gain = segment.tone == 3
                    ? kSequenceMusicGain[segment.higher] : kSequenceToneGain[segment.higher];
                const uint32_t fade = std::min({kSequenceFade, local, length - 1 - local});
                const int64_t product = static_cast<int64_t>(source) * gain * fade;
                constexpr int64_t denominator = (1LL << 30) * kSequenceFade;
                const int64_t magnitude = product < 0 ? -product : product;
                const int64_t rounded = (magnitude + denominator / 2) / denominator;
                const int16_t sample = static_cast<int16_t>(product < 0 ? -rounded : rounded);
                if (kSequenceChannel == 0) left = sample;
                else right = sample;
                break;
            }
            ++frame;
        }
        // At the end, saturate the cursor and send exact zeros indefinitely.
    }
};
} // namespace audio_test
