/* INTERNET RADIO PROJECT - ESP32-S3 */
/* Original project by Gabor Nemes. */

#include <Arduino.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "app_state.h"
#include "device_control.h"
#include "display.h"
#include "media.h"
#include "settings.h"
#include "web_server.h"

namespace {

void connectToNetwork() {
    if (!st_ssid.isEmpty()) {
        WiFi.begin(st_ssid.c_str(), st_pass.c_str());
        int retryCount = 0;
        while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
            delay(500);
            ++retryCount;
        }
        Serial.println(WiFi.localIP());
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.softAP("Radio_Setup");
        isAP = true;
        return;
    }

    configTime(0, 0, ntpServer);
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

void updateAlarm(const tm& timeInfo, bool timeValid, bool switchPressed, unsigned long now) {
    if (!isAP && timeValid && alarmActive) {
        if (timeInfo.tm_hour == alarmH && timeInfo.tm_min == alarmM) {
            if (!isAlarming && (now - alarmStartMillis > 61000 || alarmStartMillis == 0)) {
                isAlarming = true;
                alarmStartMillis = now;
                lastAlarmStep = now;
                alarmVolume = 5;
                audio.setVolume(volCurve[alarmVolume]);
                playStation(currentStationIdx);
                setBrightness(255);
                forceRedraw = true;
            }
            if (isAlarming && now - alarmStartMillis > 20000) {
                isAlarming = false;
                audio.setVolume(volCurve[mainVal]);
                forceRedraw = true;
            }
            if (isAlarming && alarmVolume < 9 && now - lastAlarmStep > 4000) {
                ++alarmVolume;
                audio.setVolume(volCurve[alarmVolume]);
                lastAlarmStep = now;
            }
        } else if (!isAlarming) {
            alarmStartMillis = 0;
        }
    }

    if (isAlarming && switchPressed) {
        isAlarming = false;
        audio.setVolume(volCurve[mainVal]);
        forceRedraw = true;
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

    if (now - lastInteraction > 30000 && !isAlarming) {
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

void updateEncoder(bool switchPressed, unsigned long now) {
    const int currentRotation = encoderPos / 4;
    static int lastRotation = 0;
    static bool volumeSavePending = false;
    const int difference = currentRotation - lastRotation;
    lastRotation = currentRotation;

    if (difference != 0) {
        if (!switchPressed) {
            mainVal = constrain(mainVal + difference, 0, 21);
            audio.setVolume(volCurve[mainVal]);
            lastVolChange = now;
            volumeSavePending = true;
        } else {
            tempStationIdx = (tempStationIdx + difference) % STATION_COUNT;
            if (tempStationIdx < 0) {
                tempStationIdx += STATION_COUNT;
            }
        }
        lastInteraction = now;
        forceRedraw = true;
    }

    static bool previousSwitchState = false;
    if (!switchPressed && previousSwitchState) {
        if (!isAP && tempStationIdx != currentStationIdx) {
            currentStationIdx = tempStationIdx;
            playStation(currentStationIdx);
            saveSettings();
            volumeSavePending = false;
        }
        forceRedraw = true;
    }
    previousSwitchState = switchPressed;

    if (volumeSavePending && now - lastVolChange >= 1000) {
        saveSettings();
        volumeSavePending = false;
    }
}

void updateVisualizers() {
    if (isAP || !showSpectrum) {
        return;
    }

    tft.startWrite();
    if (visualMode == 1 || visualMode == 3) {
        drawSpectrum();
    }
    if (visualMode == 2 || visualMode == 3) {
        drawAnalogVU();
    }
    tft.endWrite();
}

void updateDisplay(bool switchPressed, const char* currentTime, unsigned long now) {
    static uint32_t lastUiUpdate = 0;
    if (now - lastUiUpdate <= 200 && !forceRedraw) {
        return;
    }
    lastUiUpdate = now;

    tft.startWrite();
    const int displayVolume = isAlarming ? alarmVolume : mainVal;
    const bool headerChanged =
        displayVolume != lastMain || switchPressed != lastDrawnMode ||
        tempStationIdx != lastDrawnStationIdx || String(currentTime) != lastDrawnTime ||
        forceRedraw || now - lastVolChange < 3200;

    if (headerChanged) {
        const uint16_t headerBackground = switchPressed
            ? currentSkin.selMode
            : isAlarming ? currentSkin.almWarn : isAP ? 0x001F : currentSkin.bgTop;
        tft.fillRect(0, 0, 320, 35, headerBackground);
        tft.setFont(&fonts::Font0);
        tft.setTextColor(currentSkin.hInfo);
        tft.setTextDatum(TL_DATUM);
        updateWeatherUI();
        tft.drawString(
            ("V:" + String(displayVolume < 10 ? "0" : "") + String(displayVolume) +
             " | S:" + String(tempStationIdx + 1)).c_str(),
            8,
            12);

        if (alarmActive) {
            tft.setTextColor(currentSkin.almWarn);
            char alarmText[15];
            snprintf(alarmText, sizeof(alarmText), "Alarm %02d:%02d", alarmH, alarmM);
            tft.drawString(alarmText, 200, 12);
        }

        drawWifiSignal(285, 10);
        tft.setTextDatum(TC_DATUM);
        tft.setFont(&fonts::FreeSansBold12pt7b);
        tft.setTextColor(currentSkin.clk);
        tft.drawCenterString(isAP ? "SETUP MODE" : currentTime, 160, 8);

        if (!isAP && (isAlarming || now - lastVolChange < 3000)) {
            const int volumeWidth = displayVolume * 320 / 21;
            tft.fillRect(0, 33, volumeWidth, 2, currentSkin.volBar);
            tft.fillRect(volumeWidth, 33, 320 - volumeWidth, 2, currentSkin.bgBottom);
        }

        lastMain = displayVolume;
        lastDrawnMode = switchPressed;
        lastDrawnStationIdx = tempStationIdx;
        lastDrawnTime = currentTime;
    }

    String stationName;
    if (switchPressed) {
        stationName = stations[tempStationIdx].name;
    } else if (podcastMode) {
        stationName = podcastShowTft;
    } else {
        stationName = stations[currentStationIdx].name;
    }

    if (stationName != lastDrawnStationName || songTitle != lastDrawnSong || forceRedraw) {
        tft.fillRect(0, 36, 320, 105, currentSkin.bgBottom);
        if (isAP) {
            tft.setTextColor(currentSkin.textAccent);
            tft.drawCenterString("Radio_Setup / 192.168.4.1", 160, 80, &fonts::FreeSans9pt7b);
        } else {
            tft.setTextColor(currentSkin.textMain);
            tft.drawCenterString(stationName.c_str(), 160, 60, &fonts::FreeSansBold12pt7b);
            tft.setTextColor(currentSkin.textAccent);
            tft.drawCenterString(songTitle.c_str(), 160, 115, &fonts::FreeSans9pt7b);
        }
        lastDrawnStationName = stationName;
        lastDrawnSong = songTitle;
    }

    tft.setTextColor(currentSkin.textAccent);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(
        isAP ? "192.168.4.1" : WiFi.localIP().toString().c_str(),
        315,
        235,
        &fonts::Font0);
    forceRedraw = false;
    tft.endWrite();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);

    rtc_gpio_hold_dis(static_cast<gpio_num_t>(TFT_BLK));
    rtc_gpio_deinit(static_cast<gpio_num_t>(TFT_BLK));
    pinMode(TFT_BLK, OUTPUT);
    digitalWrite(TFT_BLK, LOW);
    delay(50);
    ledcAttach(TFT_BLK, 5000, 8);
    setBrightness(255);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    initializeTouchCalibration();
    loadSettings();
    connectToNetwork();
    startWebServer();

    audio.setPinout(I2S_BCK, I2S_LRC, I2S_DIN);
    audio.setVolume(volCurve[mainVal]);
    audio.setTone(gB, gM, gT);
    if (!isAP) {
        playStation(currentStationIdx);
    }

    xTaskCreatePinnedToCore(taskControl, "Ctrl", 4096, nullptr, 1, nullptr, 0);
    lastInteraction = millis();
    tft.fillScreen(currentSkin.bgBottom);
}

void loop() {
    audio.loop();
    server.handleClient();

    const bool switchPressed = digitalRead(PIN_SW) == LOW;
    const unsigned long now = millis();
    struct tm timeInfo = {};
    const time_t wallClock = time(nullptr);
    const bool timeValid =
        wallClock >= 1483228800 && localtime_r(&wallClock, &timeInfo) != nullptr;
    char currentTime[10];
    if (timeValid) {
        strftime(currentTime, sizeof(currentTime), "%H:%M", &timeInfo);
    } else {
        strcpy(currentTime, "00:00");
    }

    updateAlarm(timeInfo, timeValid, switchPressed, now);
    updatePowerState(now);
    updateEncoder(switchPressed, now);
    updateVisualizers();
    updateDisplay(switchPressed, currentTime, now);
    updateTouchTest(now);
}
