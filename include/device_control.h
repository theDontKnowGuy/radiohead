#pragma once

#include <Arduino.h>
#include "touch_gesture.h"

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

ButtonEvent pollEncoderButton(unsigned long now);
TouchEvent pollTouchEvent(int16_t& x, int16_t& y, unsigned long now);
