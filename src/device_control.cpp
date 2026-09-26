#include "device_control.h"

#include <WiFi.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "app_state.h"
#include "media.h"
#include "settings.h"
#include "web_server.h"

namespace {

constexpr unsigned long kButtonDebounceMs = 25;
constexpr unsigned long kButtonHoldMs = 700;
constexpr unsigned long kTouchPollMs = 8;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

// Observe the production poll without another SPI read or drawing over the UI.
// Skip output when USB is disconnected or lacks room for the complete report.
void recordTouchPoll(bool raw, bool onPanel, TouchEvent event,
                     unsigned long now, unsigned long gap, unsigned long readUs) {
    static unsigned long reportedAt = 0;
    static unsigned polls = 0, contacts = 0, panelContacts = 0;
    static unsigned begins = 0, releases = 0;
    static unsigned long maxGap = 0, maxReadUs = 0;
    ++polls;
    contacts += raw;
    panelContacts += onPanel;
    begins += event == TouchEvent::Begin;
    releases += event == TouchEvent::Release;
    if (gap > maxGap) maxGap = gap;
    if (readUs > maxReadUs) maxReadUs = readUs;
    if (now - reportedAt < 1000) return;
    char report[192];
    const int length = snprintf(report, sizeof(report),
        "Touch polls=%u raw=%u panel=%u press=%u release=%u max_gap_ms=%lu read_us=%lu\n",
        polls, contacts, panelContacts, begins, releases, maxGap, maxReadUs);
    // The ADC report may have just occupied the USB buffer. Keep this window
    // until a later poll has room, rather than losing all mapping/event evidence.
    if (length > 0 && length < static_cast<int>(sizeof(report)) && Serial &&
        Serial.availableForWrite() < length) return;
    if (length > 0 && length < static_cast<int>(sizeof(report)) && Serial &&
        Serial.availableForWrite() >= length) {
        Serial.write(reinterpret_cast<const uint8_t*>(report), length);
    }
    reportedAt = now;
    polls = contacts = panelContacts = begins = releases = 0;
    maxGap = maxReadUs = 0;
}

}  // namespace

void goToSleep() {
    if (webUpdateInProgress()) {
        Serial.println("Power transition deferred while firmware update is writing");
        return;
    }
    saveSettings();
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    stopStationPlayback();
#else
    audio.stopSong();
#endif
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCenterString("Power Off", 160, 100, &fonts::FreeSansBold12pt7b);

    uint64_t sleepTimeUs = 0;
    if (alarmActive) {
        const time_t wallClock = time(nullptr);
        struct tm timeInfo = {};
        if (wallClock >= 1483228800 && configuredLocalTime(wallClock, timeInfo)) {
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

bool factoryReset() {
    if (webUpdateInProgress()) {
        Serial.println("Factory reset deferred while firmware update is writing");
        return false;
    }
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.drawCenterString("FACTORY RESET", 160, 100, &fonts::FreeSansBold12pt7b);
    if (!pref.begin("radio", false)) return false;
    const bool radioCleared = pref.clear();
    pref.end();
    // Favorites are separate from the legacy radio namespace, but factory
    // reset must remove them while retaining touch calibration by design.
    if (!pref.begin("favorites", false)) return false;
    const bool favoritesCleared = pref.clear();
    pref.end();
    const bool wifiCleared = clearAllSavedWiFiCredentials();
    // Station logos are independent LittleFS assets.  Clear every active,
    // staging and recovery file along with the station records; touch
    // calibration is intentionally in its own namespace and is untouched.
    if (!radioCleared || !favoritesCleared || !wifiCleared || !clearAllStationArtwork()) return false;
    delay(3000);
    ESP.restart();
    return true;
}

namespace {

void configureTouchCalibration(bool forceRecalibration) {
    pinMode(PIN_SW, INPUT_PULLUP);

    TouchCalibration calibration = {};
    const bool recalibrationRequested =
        forceRecalibration || digitalRead(PIN_SW) == LOW;
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

}  // namespace

void initializeTouchCalibration() {
    configureTouchCalibration(false);
}

void startTouchCalibration() {
    configureTouchCalibration(true);
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
    static TouchPressLatch pressLatch;
    static unsigned long lastPoll = 0;
    if (now - lastPoll < kTouchPollMs) return TouchEvent::None;
    const unsigned long gap = lastPoll == 0 ? 0 : now - lastPoll;
    lastPoll = now;

    const unsigned long readStarted = TOUCH_DEBUG_ENABLED ? micros() : 0;
    lgfx::touch_point_t point = {};
    const bool rawContact = tft.getTouchRaw(&point);
    bool touched = rawContact;
    if (touched) {
        tft.convertRawXY(&point);
        // A spurious off-panel sample is a dropout, not a permanent veto of
        // the whole press. The latch bridges brief missing samples.
        touched = point.x >= 0 && point.x < tft.width() &&
                  point.y >= 0 && point.y < tft.height();
    }
    const TouchEvent event = pressLatch.sample(touched, static_cast<int16_t>(point.x),
                                               static_cast<int16_t>(point.y), now, x, y);
    if (TOUCH_DEBUG_ENABLED) recordTouchPoll(rawContact, touched, event, now, gap, micros() - readStarted);
    return event;
}

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
