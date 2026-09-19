#include "device_control.h"

#include <WiFi.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "app_state.h"
#include "settings.h"

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
    delay(3000);
    ESP.restart();
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
            encoderPos += difference;
            lastInteraction = millis();
        }
        vTaskDelay(1);
    }
}
