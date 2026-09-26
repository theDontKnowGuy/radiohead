#include "MAX98357AAudioSink.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace {
constexpr gpio_num_t kBck = GPIO_NUM_15;
constexpr gpio_num_t kWs = GPIO_NUM_17;
constexpr gpio_num_t kDout = GPIO_NUM_16;
constexpr char kTag[] = "max98357a";
}

MAX98357AAudioSink::MAX98357AAudioSink() {
    softwareVolumeControl = false;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    if (i2s_new_channel(&channel, &tx_, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "I2S channel allocation failed");
        tx_ = nullptr;
        return;
    }
    i2s_std_config_t standard = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = kBck,
            .ws = kWs,
            .dout = kDout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {},
        },
    };
    if (i2s_channel_init_std_mode(tx_, &standard) != ESP_OK ||
        i2s_channel_enable(tx_) != ESP_OK) {
        ESP_LOGE(kTag, "I2S setup failed");
        i2s_del_channel(tx_);
        tx_ = nullptr;
    }
}

MAX98357AAudioSink::~MAX98357AAudioSink() {
    if (tx_) {
        i2s_channel_disable(tx_);
        i2s_del_channel(tx_);
    }
}

void MAX98357AAudioSink::feedPCMFrames(const uint8_t* source, size_t bytes) {
    if (!tx_ || !source || bytes == 0 || bytes % sizeof(int16_t) != 0) return;
    int16_t scaled[512];
    size_t consumed = 0;
    while (consumed < bytes) {
        const size_t count = std::min(sizeof(scaled), bytes - consumed) / sizeof(int16_t);
        const uint8_t gain = volume_.load(std::memory_order_relaxed);
        for (size_t index = 0; index < count; ++index) {
            int16_t sample;
            memcpy(&sample, source + consumed + index * sizeof(sample), sizeof(sample));
            const unsigned channel = index & 1U;
            const float input = static_cast<float>(sample);
            // One-pole 200 Hz / 4 kHz crossover at the fixed 44.1 kHz PCM rate.
            lowState_[channel] += 0.0281f * (input - lowState_[channel]);
            highState_[channel] += 0.435f * (input - highState_[channel]);
            const float low = lowState_[channel];
            const float middle = highState_[channel] - low;
            const float high = input - highState_[channel];
            const float shaped = (low * lowGain_ + middle * midGain_ + high * highGain_) *
                headroom_ * gain / 255.0f;
            scaled[index] = static_cast<int16_t>(std::clamp(shaped, -32768.0f, 32767.0f));
        }
        const size_t chunkBytes = count * sizeof(int16_t);
        size_t sent = 0;
        const int64_t deadlineUs = esp_timer_get_time() + 200000;
        while (sent < chunkBytes) {
            size_t written = 0;
            const size_t remaining = chunkBytes - sent;
            const esp_err_t result = i2s_channel_write(
                tx_, reinterpret_cast<const uint8_t*>(scaled) + sent,
                remaining, &written, 50);
            if (written < remaining) partialWrites_.fetch_add(1, std::memory_order_relaxed);
            sent += written;
            if ((result != ESP_OK && written == 0) || esp_timer_get_time() >= deadlineUs) {
                const uint32_t failures = writeFailures_.fetch_add(1, std::memory_order_relaxed) + 1;
                if (failures == 1) ESP_LOGE(kTag, "I2S output stalled result=%d", result);
                return;
            }
        }
        pcmBytes_.fetch_add(static_cast<uint32_t>(chunkBytes), std::memory_order_relaxed);
        consumed += chunkBytes;
    }
}

void MAX98357AAudioSink::volumeChanged(uint16_t volume) {
    volume_.store(static_cast<uint8_t>(std::min<uint16_t>(volume, 255)), std::memory_order_relaxed);
}

void MAX98357AAudioSink::setTone(int8_t bassDb, int8_t middleDb, int8_t trebleDb) {
    lowGain_ = powf(10.0f, std::clamp<int>(bassDb, -12, 12) / 20.0f);
    midGain_ = powf(10.0f, std::clamp<int>(middleDb, -12, 12) / 20.0f);
    highGain_ = powf(10.0f, std::clamp<int>(trebleDb, -12, 12) / 20.0f);
    headroom_ = 1.0f / std::max({1.0f, lowGain_, midGain_, highGain_});
}

bool MAX98357AAudioSink::setParams(uint32_t sampleRate, uint8_t channels, uint8_t bitDepth) {
    ESP_LOGI(kTag, "PCM format rate=%lu channels=%u bits=%u",
             static_cast<unsigned long>(sampleRate), channels, bitDepth);
    return tx_ && sampleRate == 44100 && channels == 2 && bitDepth == 16;
}
