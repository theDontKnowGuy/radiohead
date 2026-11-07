#pragma once

#include <cstdint>

// Loop-owned radio progress tracking. The caller supplies the PCM-task counter;
// sample values/volume are irrelevant, so encoded silence and mute are healthy.
class RadioStreamHealth {
public:
    static constexpr uint32_t StallMs = 15000;
    static constexpr uint32_t StableMs = 30000;
    enum class Result { Waiting, Healthy, Stalled };

    void reset() { armed_ = false; }

    Result update(bool playingRadio, uint32_t now, uint32_t blocks) {
        if (!playingRadio) {
            reset();
            return Result::Waiting;
        }
        if (!armed_) {
            armed_ = true;
            blocks_ = blocks;
            progressAt_ = healthySince_ = now;
            return Result::Waiting;
        }
        if (blocks != blocks_) {
            blocks_ = blocks;
            // A late resumption needs another stable period before retry
            // backoff resets, but should not force an unnecessary reconnect.
            if (uint32_t(now - progressAt_) >= StallMs) healthySince_ = now;
            progressAt_ = now;
            if (uint32_t(now - healthySince_) >= StableMs) return Result::Healthy;
        }
        if (uint32_t(now - progressAt_) >= StallMs) return Result::Stalled;
        return Result::Waiting;
    }

private:
    bool armed_ = false;
    uint32_t blocks_ = 0;
    uint32_t progressAt_ = 0;
    uint32_t healthySince_ = 0;
};
