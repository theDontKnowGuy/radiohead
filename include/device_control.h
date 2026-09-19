#pragma once

#include <Arduino.h>

#ifndef TOUCH_DEBUG_ENABLED
#define TOUCH_DEBUG_ENABLED 0
#endif

void factoryReset();
void goToSleep();
void initializeTouchCalibration();
void taskControl(void* parameter);
int consumeEncoderDetents();

enum class ButtonEvent : uint8_t {
    None,
    Push,
    Hold,
};

enum class TouchEvent : uint8_t {
    None,
    Tap,
    SwipeUp,
    SwipeDown,
};

ButtonEvent pollEncoderButton(unsigned long now);
TouchEvent pollTouchEvent(int16_t& x, int16_t& y, unsigned long now);
#if TOUCH_DEBUG_ENABLED
void updateTouchTest(unsigned long now);
#endif
