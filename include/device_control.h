#pragma once

#include <Arduino.h>
#include "touch_gesture.h"
#include "settings.h"
#include "stock_touch_sampling.h"

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
// Starts interactive TFT calibration and reports whether it was saved.
bool startTouchCalibration();
// Loop-owned diagnostics. Debug is transient and never changes acquisition or
// input routing; the display draws the latest detected contact independently.
struct TouchAxisDiagnostics {
    stock_touch_sampling::Axis last;
    uint32_t railFrames = 0;
    uint32_t sparseFrames = 0;
    uint32_t unstableFrames = 0;
};
struct TouchAdcDiagnostics {
    TouchAxisDiagnostics x, y;
    uint32_t frames = 0;
    uint32_t acceptedFrames = 0;
    uint32_t pressureRejectedFrames = 0;
    uint16_t pressure = 0;
    uint8_t pressureValid = 0;
};
struct TouchDiagnostics {
    TouchCalibration activeCalibration = {};
    const char* calibrationSource = "not initialized";
    bool debugEnabled = false;
    bool contact = false;
    // Retained last accepted contact, including off-panel mappings. A rejected
    // sample changes contact only; it must not destroy diagnostic evidence.
    bool onPanel = false;
    int32_t rawX = 0, rawY = 0;
    int32_t screenX = 0, screenY = 0;
    uint32_t polls = 0;
    uint32_t contactSamples = 0;
    uint32_t lastContactAt = 0;
    TouchAdcDiagnostics adc;
    uint32_t lastPollGapMs = 0, maxPollGapMs = 0;
    uint32_t lastReadUs = 0, maxReadUs = 0;
    uint32_t presses = 0, releases = 0, offPanelSamples = 0;
    uint32_t recoveryAttempts = 0, recoverySuccesses = 0;
    uint32_t lastRecoveryAt = 0, lastRecoveryUs = 0;
    bool lastRecoverySucceeded = false;
    TouchAdcDiagnostics beforeRecoveryAdc;
};
const TouchDiagnostics& touchDiagnostics();
// Called by the instrumented stock reader, including calibration reads. Never
// samples again, allocates, writes Serial or changes contact acceptance.
void recordTouchFrame(const stock_touch_sampling::Frame& frame);
// Loop-only manual recovery. Calibration and counters survive; held touch is
// consumed until stable release. Bus init success is not a sensor health test.
bool recoverTouch();
void setTouchDebugEnabled(bool enabled);
constexpr uint32_t TOUCH_DEBUG_MARKER_HOLD_MS = 750;
bool touchDebugMarkerVisible(unsigned long now);
void taskControl(void* parameter);
int consumeEncoderDetents();

// Snapshot only: observe the existing rotation task without resetting input.
struct EncoderDiagnostics {
    bool taskCreated = false;
    uint32_t samples = 0;
    uint32_t aEdges = 0;
    uint32_t bEdges = 0;
    uint32_t positiveTicks = 0;
    uint32_t negativeTicks = 0;
    uint32_t invalidTransitions = 0;
    uint8_t sampledAB = 0;
    bool rawA = false, rawB = false, rawSwitch = false;
};
EncoderDiagnostics encoderDiagnostics();

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
