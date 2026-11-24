#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>

constexpr int kAudioBalanceLimit = 100;
inline std::atomic<int> radioBalancePosition{0};

inline int radioBalance() {
    return radioBalancePosition.load(std::memory_order_relaxed);
}

inline void radioSetBalance(int position) {
    radioBalancePosition.store(
        position >= -kAudioBalanceLimit && position <= kAudioBalanceLimit ? position : 0,
        std::memory_order_relaxed);
}

// Interleaved left/right PCM. Attenuate the opposite channel and leave the
// favored channel untouched. Sample the setting once per output block.
template <typename Sample>
void radioApplyBalance(Sample* samples, std::size_t words) {
    const int position = radioBalance();
    if (position == 0) return;
    const std::size_t channel = position > 0 ? 0 : 1;
    const float gain = (kAudioBalanceLimit - std::abs(position)) /
        static_cast<float>(kAudioBalanceLimit);
    for (std::size_t frame = 0; frame + 1 < words; frame += 2) {
        samples[frame + channel] = static_cast<Sample>(samples[frame + channel] * gain);
    }
}
