#pragma once

#include <stdint.h>

enum class TouchEvent : uint8_t {
    None,
    Begin,
    Release,
};

// Button-first input: activate on the first valid ADC contact, with no swipe
// classification or release wait. Sliding/holding never repeats an action.
// Debounce only re-arming, bridging weak-contact dropouts across page changes.
class TouchPressLatch {
public:
    TouchEvent sample(bool down, int16_t x, int16_t y, uint32_t now,
                      int16_t& eventX, int16_t& eventY) {
        if (down) {
            lastContact_ = now;
            if (active_) return TouchEvent::None;
            active_ = true;
            eventX = x;
            eventY = y;
            return TouchEvent::Begin;
        }
        if (!active_ || uint32_t(now - lastContact_) < kReleaseMs) return TouchEvent::None;
        active_ = false;
        return TouchEvent::Release;
    }

private:
    static constexpr uint32_t kReleaseMs = 80;
    bool active_ = false;
    uint32_t lastContact_ = 0;
};
