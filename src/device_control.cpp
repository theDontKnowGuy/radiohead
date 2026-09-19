#include "device_control.h"

#include <WiFi.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "app_state.h"
#include "settings.h"

namespace {

constexpr unsigned long kButtonDebounceMs = 25;
constexpr unsigned long kButtonHoldMs = 700;
constexpr unsigned long kTouchPollMs = 8;
constexpr unsigned long kTouchReleaseDebounceMs = 36;
constexpr int16_t kTouchTapTolerance = 16;
constexpr int16_t kTouchSwipeMinDistance = 28;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

}  // namespace

void goToSleep() {
    saveSettings();
    audio.stopSong();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCenterString("Power Off", 160, 100, &fonts::FreeSansBold12pt7b);

    uint64_t sleepTimeUs = 0;
    if (alarmActive) {
        struct tm timeInfo;
        if (getLocalTime(&timeInfo)) {
            const long nowSeconds = timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec;
            const long alarmSeconds = alarmH * 3600 + alarmM * 60;
            const long difference = alarmSeconds > nowSeconds
                ? alarmSeconds - nowSeconds
                : 86400 - nowSeconds + alarmSeconds;
            char buffer[32];
            snprintf(
                buffer,
                sizeof(buffer),
                "Alarm in %02d:%02d",
                static_cast<int>(difference / 3600),
                static_cast<int>((difference % 3600) / 60));
            tft.setFont(&fonts::FreeSans9pt7b);
            tft.drawCenterString(buffer, 160, 140);
            if (difference > 5) {
                sleepTimeUs = static_cast<uint64_t>(difference - 5) * 1000000ULL;
            }
        }
    }

    delay(5000);
    WiFi.disconnect(true);
    delay(100);
    tft.writeCommand(0x10);
    ledcWrite(TFT_BLK, 0);
    delay(50);
    rtc_gpio_hold_en(static_cast<gpio_num_t>(TFT_BLK));
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_K0), 0);
    if (sleepTimeUs > 0) {
        esp_sleep_enable_timer_wakeup(sleepTimeUs);
    }
    esp_deep_sleep_start();
}

void factoryReset() {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.drawCenterString("FACTORY RESET", 160, 100, &fonts::FreeSansBold12pt7b);
    pref.begin("radio", false);
    pref.clear();
    pref.end();
    // Favorites are separate from the legacy radio namespace, but factory
    // reset must remove them while retaining touch calibration by design.
    pref.begin("favorites", false);
    pref.clear();
    pref.end();
    delay(3000);
    ESP.restart();
}

void initializeTouchCalibration() {
    pinMode(PIN_SW, INPUT_PULLUP);

    TouchCalibration calibration = {};
    const bool recalibrationRequested = digitalRead(PIN_SW) == LOW;
    if (!recalibrationRequested && loadTouchCalibration(calibration)) {
        tft.setTouchCalibrate(calibration.data());
        Serial.println("Loaded saved touch calibration");
        return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCenterString("TOUCH CALIBRATION", tft.width() / 2, 72, &fonts::FreeSansBold12pt7b);
    tft.drawCenterString("Tap each corner marker", tft.width() / 2, 112, &fonts::FreeSans9pt7b);
    tft.drawCenterString("then release", tft.width() / 2, 140, &fonts::FreeSans9pt7b);
    delay(1500);
    tft.fillScreen(TFT_BLACK);

    tft.calibrateTouch(calibration.data(), TFT_WHITE, TFT_BLACK, 16);

    Serial.println("Touch calibration coordinates:");
    for (size_t i = 0; i < calibration.size(); i += 2) {
        Serial.printf(
            "  point %u: raw=(%u,%u)\n",
            static_cast<unsigned int>(i / 2 + 1),
            static_cast<unsigned int>(calibration[i]),
            static_cast<unsigned int>(calibration[i + 1]));
    }

    const bool saved = saveTouchCalibration(calibration);
    tft.fillScreen(saved ? TFT_DARKGREEN : TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawCenterString(
        saved ? "CALIBRATION SAVED" : "CALIBRATION SAVE FAILED",
        tft.width() / 2,
        95,
        &fonts::FreeSansBold12pt7b);
    tft.drawCenterString(
        "Hold encoder at boot to redo",
        tft.width() / 2,
        135,
        &fonts::FreeSans9pt7b);
    delay(1500);
}

int consumeEncoderDetents() {
    static int residualTicks = 0;
    int ticks = 0;
    portENTER_CRITICAL(&encoderMux);
    ticks = encoderPos;
    encoderPos = 0;
    portEXIT_CRITICAL(&encoderMux);

    residualTicks += ticks;
    const int detents = residualTicks / 4;
    residualTicks -= detents * 4;
    return detents;
}

ButtonEvent pollEncoderButton(unsigned long now) {
    static bool rawPressed = false;
    static bool stablePressed = false;
    static bool holdSent = false;
    static unsigned long rawChangedAt = 0;
    static unsigned long pressedAt = 0;

    const bool rawNow = digitalRead(PIN_SW) == LOW;
    if (rawNow != rawPressed) {
        rawPressed = rawNow;
        rawChangedAt = now;
    }
    if (rawPressed != stablePressed && now - rawChangedAt >= kButtonDebounceMs) {
        stablePressed = rawPressed;
        if (stablePressed) {
            pressedAt = now;
            holdSent = false;
        } else if (!holdSent) {
            return ButtonEvent::Push;
        }
    }
    if (stablePressed && !holdSent && now - pressedAt >= kButtonHoldMs) {
        holdSent = true;
        return ButtonEvent::Hold;
    }
    return ButtonEvent::None;
}

TouchEvent pollTouchEvent(int16_t& x, int16_t& y, unsigned long now) {
    static bool touching = false;
    static int16_t startX = 0;
    static int16_t startY = 0;
    static int16_t lastX = 0;
    static int16_t lastY = 0;
    static bool cancelled = false;
    static bool outside = false;
    static unsigned long lastPoll = 0;
    static unsigned long lastTouchSampleAt = 0;
    if (now - lastPoll < kTouchPollMs) {
        return TouchEvent::None;
    }
    lastPoll = now;

    lgfx::touch_point_t rawPoint;
    const bool touched = tft.getTouchRaw(&rawPoint);
    if (touched) {
        tft.convertRawXY(&rawPoint);
        const int16_t pointX = static_cast<int16_t>(rawPoint.x);
        const int16_t pointY = static_cast<int16_t>(rawPoint.y);
        if (!touching) {
            touching = true;
            cancelled = pointX < 0 || pointX >= tft.width() || pointY < 0 || pointY >= tft.height();
            outside = cancelled;
            startX = pointX;
            startY = pointY;
            lastX = pointX;
            lastY = pointY;
        } else {
            lastX = pointX;
            lastY = pointY;
            outside = outside || pointX < 0 || pointX >= tft.width() || pointY < 0 || pointY >= tft.height();
        }
        lastTouchSampleAt = now;
        if (abs(pointX - startX) > kTouchTapTolerance || abs(pointY - startY) > kTouchTapTolerance) {
            cancelled = true;
        }
        return TouchEvent::None;
    }

    if (!touching) {
        return TouchEvent::None;
    }
    // A light finger touch can drop a single XPT2046 sample while the finger
    // remains on the resistive panel.  Do not turn that short gap into a
    // release; a real release is still reported within 36 ms.
    if (now - lastTouchSampleAt < kTouchReleaseDebounceMs) {
        return TouchEvent::None;
    }
    touching = false;
    const int16_t deltaX = lastX - startX;
    const int16_t deltaY = lastY - startY;
    if (!outside && abs(deltaY) >= kTouchSwipeMinDistance && abs(deltaY) > abs(deltaX) * 2) {
        return deltaY < 0 ? TouchEvent::SwipeUp : TouchEvent::SwipeDown;
    }
    if (cancelled) return TouchEvent::None;
    x = startX;
    y = startY;
    return TouchEvent::Tap;
}

#if TOUCH_DEBUG_ENABLED
void updateTouchTest(unsigned long now) {
    static bool wasTouched = false;
    static unsigned long lastPoll = 0;
    static unsigned long lastReport = 0;

    if (now - lastPoll < 10) {
        return;
    }
    lastPoll = now;

    lgfx::touch_point_t rawPoint;
    if (!tft.getTouchRaw(&rawPoint)) {
        if (wasTouched) {
            Serial.println("Touch released");
            forceRedraw = true;
        }
        wasTouched = false;
        return;
    }

    lgfx::touch_point_t screenPoint = rawPoint;
    tft.convertRawXY(&screenPoint);
    lastInteraction = now;

    if (!wasTouched || now - lastReport >= 100) {
        Serial.printf(
            "Touch raw=(%ld,%ld) screen=(%ld,%ld) pressure=%u\n",
            static_cast<long>(rawPoint.x),
            static_cast<long>(rawPoint.y),
            static_cast<long>(screenPoint.x),
            static_cast<long>(screenPoint.y),
            static_cast<unsigned int>(rawPoint.size));
        lastReport = now;
    }

    if (screenPoint.x >= 0 && screenPoint.x < tft.width() &&
        screenPoint.y >= 0 && screenPoint.y < tft.height()) {
        tft.fillCircle(screenPoint.x, screenPoint.y, 4, TFT_CYAN);
        tft.drawCircle(screenPoint.x, screenPoint.y, 8, TFT_WHITE);
    }
    wasTouched = true;
}
#endif

void taskControl(void* parameter) {
    (void)parameter;
    pinMode(PIN_A, INPUT_PULLUP);
    pinMode(PIN_B, INPUT_PULLUP);
    pinMode(PIN_K0, INPUT_PULLUP);
    pinMode(PIN_SW, INPUT_PULLUP);

    static uint8_t oldAB = 0;
    static constexpr int8_t transitions[16] = {
        0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0,
    };
    for (;;) {
        oldAB <<= 2;
        oldAB |= (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);
        const int8_t difference = transitions[oldAB & 0x0F];
        if (difference != 0) {
            portENTER_CRITICAL(&encoderMux);
            encoderPos += difference;
            portEXIT_CRITICAL(&encoderMux);
            lastInteraction = millis();
        }
        vTaskDelay(1);
    }
}
