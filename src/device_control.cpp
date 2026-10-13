#include "device_control.h"

#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <time.h>

#include "app_state.h"
#include "display.h"
#include "media.h"
#include "settings.h"
#include "web_server.h"

namespace {

constexpr unsigned long kButtonDebounceMs = 25;
constexpr unsigned long kButtonHoldMs = 3000;
constexpr unsigned long kTouchPollMs = 8;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
// All shared encoder diagnostics use encoderMux, like encoderPos.
EncoderDiagnostics encoderState;
TouchDiagnostics touchState;
TouchPressLatch pressLatch;

void incrementTouchCounter(uint32_t& counter) {
    if (counter != UINT32_MAX) ++counter;
}

void recordTouchAxis(TouchAxisDiagnostics& axis, const stock_touch_sampling::Axis& sample) {
    axis.last = sample;
    if (!sample.valid) incrementTouchCounter(axis.railFrames);
    else if (sample.valid < 3) incrementTouchCounter(axis.sparseFrames);
    else if (sample.cluster < 3) incrementTouchCounter(axis.unstableFrames);
}

// Sleep wake is level-triggered. Consume both the initiating hold and the
// waking press, including bounce, so neither leaks into the next boot.
void waitForPowerButtonsRelease() {
    unsigned long releasedAt = millis();
    while (millis() - releasedAt < kButtonDebounceMs) {
        if (digitalRead(PIN_SW) == LOW || digitalRead(PIN_K0) == LOW) {
            releasedAt = millis();
        }
        delay(1);
    }
}

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
        Serial.println("Power transition deferred during device maintenance");
        return;
    }
    // Preserve light sleep and both button wake sources after the pin remap.
    pinMode(PIN_SW, INPUT_PULLUP);
    pinMode(PIN_K0, INPUT_PULLUP);
    if (gpio_wakeup_enable(static_cast<gpio_num_t>(PIN_SW), GPIO_INTR_LOW_LEVEL) != ESP_OK ||
        gpio_wakeup_enable(static_cast<gpio_num_t>(PIN_K0), GPIO_INTR_LOW_LEVEL) != ESP_OK ||
        esp_sleep_enable_gpio_wakeup() != ESP_OK) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(PIN_SW));
        gpio_wakeup_disable(static_cast<gpio_num_t>(PIN_K0));
        Serial.println("Unable to configure button wake; staying awake");
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
    tft.drawCenterString("Me sleeping", 160, 100, &fonts::FreeSansBold12pt7b);

    waitForPowerButtonsRelease();
    WiFi.disconnect(true);
    delay(100);
    tft.writeCommand(0x10);
    ledcWrite(TFT_BLK, 0);
    delay(50);
    rtc_gpio_hold_en(static_cast<gpio_num_t>(TFT_BLK));
    const esp_err_t result = esp_light_sleep_start();
    if (result != ESP_OK) {
        Serial.printf("Sleep failed (%d); restarting\n", static_cast<int>(result));
    }
    // Reuse the existing startup path to restore display, Wi-Fi and playback.
    // Release first so encoder wake cannot request the boot-time setup AP.
    waitForPowerButtonsRelease();
    ESP.restart();
}

bool factoryReset() {
    if (webUpdateInProgress()) {
        Serial.println("Factory reset deferred during device maintenance");
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

void initializeTouchCalibration() {
    TouchCalibration calibration = {};
    if (loadTouchCalibration(calibration)) {
        tft.setTouchCalibrate(calibration.data());
        touchState.activeCalibration = calibration;
        touchState.calibrationSource = "saved at boot";
        Serial.println("Loaded saved touch calibration");
    } else {
        const auto config = tft.touch()->config();
        touchState.activeCalibration = {config.x_min, config.y_min, config.x_min,
            config.y_max, config.x_max, config.y_min, config.x_max, config.y_max};
        touchState.calibrationSource = "panel defaults";
        // Keep the panel's configured coordinate bounds until the user
        // explicitly calibrates in Settings. Boot must also work without a TFT.
        Serial.println("Using default touch mapping; calibrate in Settings if needed");
    }
}

bool startTouchCalibration() {
    TouchCalibration calibration = {};
    lastInteraction = millis();
    setBrightness(255);
    isDimmed = false;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCenterString("TOUCH CALIBRATION", tft.width() / 2, 72, &fonts::FreeSansBold12pt7b);
    tft.drawCenterString("Tap each corner marker", tft.width() / 2, 112, &fonts::FreeSans9pt7b);
    tft.drawCenterString("then release", tft.width() / 2, 140, &fonts::FreeSans9pt7b);
    delay(1500);
    tft.fillScreen(TFT_BLACK);

    tft.calibrateTouch(calibration.data(), TFT_WHITE, TFT_BLACK, 16);
    touchState.activeCalibration = calibration;
    touchState.calibrationSource = "measured this session";
    touchState.contact = false;
    touchState.onPanel = false;

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
        "Redo in Settings > Device",
        tft.width() / 2,
        135,
        &fonts::FreeSans9pt7b);
    delay(1500);
    return saved;
}

const TouchDiagnostics& touchDiagnostics() {
    return touchState;
}

void recordTouchFrame(const stock_touch_sampling::Frame& frame) {
    auto& adc = touchState.adc;
    incrementTouchCounter(adc.frames);
    if (frame.accepted) incrementTouchCounter(adc.acceptedFrames);
    if (frame.x.valid >= 3 && frame.y.valid >= 3 && !frame.accepted) {
        incrementTouchCounter(adc.pressureRejectedFrames);
    }
    recordTouchAxis(adc.x, frame.x);
    recordTouchAxis(adc.y, frame.y);
    adc.pressure = frame.pressure;
    adc.pressureValid = frame.pressureValid;
}

bool recoverTouch() {
    touchState.beforeRecoveryAdc = touchState.adc;
    incrementTouchCounter(touchState.recoveryAttempts);
    touchState.lastRecoveryAt = static_cast<uint32_t>(millis());
    pressLatch.cancelUntilRelease(touchState.lastRecoveryAt);
    touchState.contact = false;
    const uint32_t startedUs = micros();
    touchState.lastRecoverySucceeded = tft.recoverTouch();
    touchState.lastRecoveryUs = static_cast<uint32_t>(micros()) - startedUs;
    if (touchState.lastRecoverySucceeded) incrementTouchCounter(touchState.recoverySuccesses);
    return touchState.lastRecoverySucceeded;
}

void setTouchDebugEnabled(bool enabled) {
    touchState.debugEnabled = enabled;
    touchState.contact = false;
}

bool touchDebugMarkerVisible(unsigned long now) {
    return touchState.debugEnabled && touchState.contactSamples != 0 &&
        (touchState.contact ||
         static_cast<uint32_t>(now) - touchState.lastContactAt < TOUCH_DEBUG_MARKER_HOLD_MS);
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

EncoderDiagnostics encoderDiagnostics() {
    portENTER_CRITICAL(&encoderMux);
    EncoderDiagnostics snapshot = encoderState;
    portEXIT_CRITICAL(&encoderMux);
    // Current electrical levels are distinct from the task's last sample.
    snapshot.rawA = digitalRead(PIN_A) == HIGH;
    snapshot.rawB = digitalRead(PIN_B) == HIGH;
    snapshot.rawSwitch = digitalRead(PIN_SW) == HIGH;
    return snapshot;
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
    static unsigned long lastPoll = 0;
    if (now - lastPoll < kTouchPollMs) return TouchEvent::None;
    const unsigned long gap = lastPoll == 0 ? 0 : now - lastPoll;
    lastPoll = now;

    const uint32_t readStarted = micros();
    lgfx::touch_point_t point = {};
    const bool rawContact = tft.getTouchRaw(&point);
    touchState.lastReadUs = static_cast<uint32_t>(micros()) - readStarted;
    touchState.maxReadUs = std::max(touchState.maxReadUs, touchState.lastReadUs);
    touchState.lastPollGapMs = gap;
    touchState.maxPollGapMs = std::max(touchState.maxPollGapMs, touchState.lastPollGapMs);
    if (touchState.polls != UINT32_MAX) ++touchState.polls;
    touchState.contact = rawContact;
    bool touched = rawContact;
    if (touched) {
        touchState.rawX = point.x;
        touchState.rawY = point.y;
        if (touchState.contactSamples != UINT32_MAX) ++touchState.contactSamples;
        touchState.lastContactAt = static_cast<uint32_t>(now);
        tft.convertRawXY(&point);
        // A spurious off-panel sample is a dropout, not a permanent veto of
        // the whole press. The latch bridges brief missing samples.
        touched = point.x >= 0 && point.x < tft.width() &&
                  point.y >= 0 && point.y < tft.height();
        touchState.onPanel = touched;
        if (!touched) incrementTouchCounter(touchState.offPanelSamples);
        touchState.screenX = point.x;
        touchState.screenY = point.y;
    }
    const bool latchDown = touched || (pressLatch.awaitingRelease() && rawContact);
    const TouchEvent event = pressLatch.sample(latchDown, static_cast<int16_t>(point.x),
                                               static_cast<int16_t>(point.y), now, x, y);
    if (event == TouchEvent::Begin) incrementTouchCounter(touchState.presses);
    if (event == TouchEvent::Release) incrementTouchCounter(touchState.releases);
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
        const uint8_t ab = oldAB & 0x03;
        portENTER_CRITICAL(&encoderMux);
        if (encoderState.samples != 0) {
            const uint8_t changed = encoderState.sampledAB ^ ab;
            if (changed & 0x02) incrementTouchCounter(encoderState.aEdges);
            if (changed & 0x01) incrementTouchCounter(encoderState.bEdges);
            if (changed == 0x03) incrementTouchCounter(encoderState.invalidTransitions);
        }
        encoderState.sampledAB = ab;
        incrementTouchCounter(encoderState.samples);
        if (difference != 0) {
            encoderPos += difference;
            if (difference > 0) incrementTouchCounter(encoderState.positiveTicks);
            else incrementTouchCounter(encoderState.negativeTicks);
        }
        portEXIT_CRITICAL(&encoderMux);
        if (difference != 0) {
            lastInteraction = millis();
        }
        vTaskDelay(1);
    }
}

void updatePowerState(unsigned long now) {
    static bool volumeBarVisible = false;
    if (now - lastVolChange < 3050) {
        volumeBarVisible = true;
    } else if (volumeBarVisible) {
        forceRedraw = true;
        volumeBarVisible = false;
    }

    // The encoder task can advance lastInteraction beyond the loop's cached
    // now. Snapshot activity first, then read the clock, so unsigned elapsed
    // time cannot wrap into a false idle timeout while the encoder is turning.
    const uint32_t interactionAt = lastInteraction;
    const uint32_t idleMs = static_cast<uint32_t>(millis()) - interactionAt;
    const bool dimTimeoutExpired =
        autoDimSeconds != AUTO_DIM_NEVER_SECONDS &&
        idleMs > static_cast<unsigned long>(autoDimSeconds) * 1000UL;
    if (dimTimeoutExpired) {
        if (!isDimmed) {
            setBrightness(20);
            isDimmed = true;
        }
    } else if (isDimmed) {
        setBrightness(255);
        isDimmed = false;
    }

    static uint32_t buttonPressedAt = 0;
    static bool buttonActive = false;
    if (digitalRead(PIN_K0) == LOW) {
        if (!buttonActive) {
            buttonPressedAt = now;
            buttonActive = true;
        }
        if (now - buttonPressedAt > 5000) {
            factoryReset();
        }
    } else if (buttonActive) {
        if (now - buttonPressedAt > 1500) {
            goToSleep();
        }
        buttonActive = false;
    }
}

DeviceInput pollDeviceInput(unsigned long now) {
    DeviceInput input;
    input.displayWasDimmed = isDimmed;
    input.button = pollEncoderButton(now);
    input.touch = pollTouchEvent(input.touchX, input.touchY, now);
    return input;
}

void startDeviceControl() {
    const bool created =
        xTaskCreatePinnedToCore(taskControl, "Ctrl", 4096, nullptr, 1, nullptr, 0) == pdPASS;
    portENTER_CRITICAL(&encoderMux);
    encoderState.taskCreated = created;
    portEXIT_CRITICAL(&encoderMux);
}
