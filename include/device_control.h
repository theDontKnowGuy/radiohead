#pragma once

#include <Arduino.h>
#include "touch_gesture.h"

#ifndef TOUCH_DEBUG_ENABLED
#define TOUCH_DEBUG_ENABLED 0
#endif

// Clears the documented radio/favorites/artwork scope while retaining touch
// calibration.  Returns false when a Preferences namespace cannot be cleared.
bool factoryReset();
void goToSleep();
// Loads saved calibration during boot; otherwise retains the panel defaults.
// Never starts interactive calibration or waits for a touchscreen.
void initializeTouchCalibration();
// Always starts the interactive calibration flow for the Settings action.
void startTouchCalibration();
void taskControl(void* parameter);
int consumeEncoderDetents();

enum class ButtonEvent : uint8_t {
    None,
    Push,
    Hold,
};

ButtonEvent pollEncoderButton(unsigned long now);
TouchEvent pollTouchEvent(int16_t& x, int16_t& y, unsigned long now);

// One input sample per loop; retain dim state from before any wake interaction.
struct DeviceInput {
    bool displayWasDimmed = false;
    ButtonEvent button = ButtonEvent::None;
    TouchEvent touch = TouchEvent::None;
    int16_t touchX = 0;
    int16_t touchY = 0;
};

DeviceInput pollDeviceInput(unsigned long now);
void startDeviceControl();
void updatePowerState(unsigned long now);
