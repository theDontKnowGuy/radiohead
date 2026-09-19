#pragma once

#include <stdint.h>
#include <stdlib.h>

enum class TouchEvent : uint8_t {
    None,
    Begin,
    Tap,
    HorizontalDrag,
    SwipeUp,
    SwipeDown,
};

// Deterministic gesture recognition; sampling and screen hit testing stay with
// their owners. No minimum hold time, allocation, or blocking debounce.
class TouchGesture {
public:
    TouchEvent sample(bool down, int16_t x, int16_t y, uint32_t now,
                      int16_t& eventX, int16_t& eventY) {
        if (!down) {
            if (!active_ || uint32_t(now - lastContact_) < 28) return TouchEvent::None;
            active_ = false;
            eventX = lastX_;
            eventY = lastY_;
            if (scrolling_) return TouchEvent::None;
            const int dx = lastX_ - startX_;
            const int dy = lastY_ - startY_;
            if (abs(dx) >= 18 && abs(dx) > abs(dy) * 2) return TouchEvent::HorizontalDrag;
            return moved_ ? TouchEvent::None : TouchEvent::Tap;
        }

        lastContact_ = now;
        if (!active_) {
            active_ = true;
            moved_ = scrolling_ = false;
            startX_ = lastX_ = x;
            startY_ = lastY_ = scrollY_ = y;
            sampleCount_ = 0;
            eventX = x;
            eventY = y;
        }
        const bool first = sampleCount_ == 0;
        xs_[sampleCount_ % 3] = x;
        ys_[sampleCount_ % 3] = y;
        if (++sampleCount_ >= 3) {
            // One bad coordinate must not permanently cancel a finger tap.
            x = median(xs_[0], xs_[1], xs_[2]);
            y = median(ys_[0], ys_[1], ys_[2]);
            if (sampleCount_ == 6) sampleCount_ = 3;
        }
        lastX_ = x;
        lastY_ = y;
        if (first) return TouchEvent::Begin;

        const int dx = x - startX_;
        const int dy = y - startY_;
        if (abs(dx) > 18 || abs(dy) > 18) moved_ = true;
        const int scrollDelta = y - scrollY_;
        if ((!scrolling_ && abs(dy) >= 24 && abs(dy) * 2 > abs(dx) * 3) ||
            (scrolling_ && abs(scrollDelta) >= 39)) {
            scrolling_ = true;
            scrollY_ = y;
            eventX = x;
            eventY = y;
            return scrollDelta < 0 ? TouchEvent::SwipeUp : TouchEvent::SwipeDown;
        }
        return TouchEvent::None;
    }

private:
    static int16_t median(int16_t a, int16_t b, int16_t c) {
        if (a > b) { const int16_t t = a; a = b; b = t; }
        if (b > c) b = c;
        return a > b ? a : b;
    }
    bool active_ = false;
    bool moved_ = false;
    bool scrolling_ = false;
    uint8_t sampleCount_ = 0;
    int16_t xs_[3] = {}, ys_[3] = {};
    int16_t startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0, scrollY_ = 0;
    uint32_t lastContact_ = 0;
};
