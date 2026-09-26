#pragma once

#include <atomic>
#include <cstdint>
#include "AudioSink.h"
#include "driver/i2s_std.h"

class MAX98357AAudioSink final : public AudioSink {
public:
    MAX98357AAudioSink();
    ~MAX98357AAudioSink() override;
    void feedPCMFrames(const uint8_t* buffer, size_t bytes) override;
    void volumeChanged(uint16_t volume) override;
    void setTone(int8_t bassDb, int8_t middleDb, int8_t trebleDb);
    bool setParams(uint32_t sampleRate, uint8_t channels, uint8_t bitDepth) override;
    uint32_t partialWrites() const { return partialWrites_.load(std::memory_order_relaxed); }
    uint32_t writeFailures() const { return writeFailures_.load(std::memory_order_relaxed); }
    uint32_t pcmBytes() const { return pcmBytes_.load(std::memory_order_relaxed); }

private:
    i2s_chan_handle_t tx_ = nullptr;
    std::atomic<uint8_t> volume_{0};
    std::atomic<uint32_t> partialWrites_{0};
    std::atomic<uint32_t> writeFailures_{0};
    std::atomic<uint32_t> pcmBytes_{0};
    float lowGain_ = 1.0f, midGain_ = 1.0f, highGain_ = 1.0f, headroom_ = 1.0f;
    float lowState_[2] = {}, highState_[2] = {};
};
