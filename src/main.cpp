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
#include "ui_controller.h"
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
    mediaBegin();
    audio.setVolume(volCurve[mainVal]);
    audio.setTone(gB, gM, gT);
    if (playableStationCount() > 0 && selectedPlayableStationIndex() < 0) {
        currentStationIdx = playableStationSlotAt(0);
        tempStationIdx = currentStationIdx;
    }
    if (!isAP) {
        playStation(currentStationIdx);
    }

    xTaskCreatePinnedToCore(taskControl, "Ctrl", 4096, nullptr, 1, nullptr, 0);
    lastInteraction = millis();
    uiControllerBegin();
    forceRedraw = true;
}

void loop() {
    audio.loop();
    server.handleClient();
    updateWeatherData();

    const unsigned long now = millis();
    const bool displayWasDimmed = isDimmed;
    const ButtonEvent buttonEvent = pollEncoderButton(now);
    int16_t touchX = 0;
    int16_t touchY = 0;
    const TouchEvent touchEvent = pollTouchEvent(touchX, touchY, now);
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

    const bool alarmWasActive = isAlarming;
    updateAlarm(timeInfo, timeValid, buttonEvent == ButtonEvent::Push, now);
    uiControllerSetAlarmActive(isAlarming);

    // The first accepted contact owns one action. TouchGesture stays latched
    // across navigation until release, including consumed dim/alarm presses.
    const UiPage touchPage = uiControllerRenderState().page;

    const int detents = consumeEncoderDetents();
    if (!alarmWasActive) {
        uiControllerTurn(detents, now, displayWasDimmed);
        if (buttonEvent == ButtonEvent::Push) {
            uiControllerPush(now, displayWasDimmed);
        } else if (buttonEvent == ButtonEvent::Hold) {
            uiControllerHold(now, displayWasDimmed);
        }
        const UiRenderState state = uiControllerRenderState();
        if (touchEvent == TouchEvent::Begin && state.page == touchPage &&
            !displayWasDimmed && !isAlarming) {
            uiControllerTap(uiHitTest(state, touchX, touchY), touchX, now, false);
        }
    }
    if (buttonEvent != ButtonEvent::None || touchEvent != TouchEvent::None) {
        lastInteraction = now;
    }

    static bool volumeSavePending = false;
    UiCommand command;
    while (uiControllerTakeCommand(command)) {
        switch (command.kind) {
        case UiCommandKind::ChangeVolume:
            setRadioVolumeIndex(mainVal + command.value);
            volumeSavePending = true;
            break;
        case UiCommandKind::SetVolume:
            setRadioVolumeIndex(command.value);
            volumeSavePending = true;
            break;
        case UiCommandKind::ToggleMute:
            toggleRadioMute();
            break;
        case UiCommandKind::SelectStation:
            if (!isAP && command.value >= 0 && command.value < STATION_COUNT) {
                playStation(command.value);
                saveSettings();
            }
            break;
        case UiCommandKind::PreviousStation:
        case UiCommandKind::NextStation: {
            const int direction = command.kind == UiCommandKind::PreviousStation ? -1 : 1;
            const int station = adjacentPlayableStationSlot(currentStationIdx, direction);
            if (!isAP && station >= 0) {
                playStation(station);
                saveSettings();
            }
            break;
        }
        case UiCommandKind::StopPlayback:
            stopStationPlayback();
            break;
        case UiCommandKind::RejoinStation:
            if (!isAP && currentStationIdx >= 0 && currentStationIdx < STATION_COUNT) {
                playStation(currentStationIdx);
            }
            break;
        case UiCommandKind::ToggleStationFavorite:
            if (command.value >= 0 && command.value < STATION_COUNT) {
                const bool wasFavorite = isStationFavorite(command.value);
                if (!toggleStationFavorite(command.value)) break;
                if (!saveFavorites()) {
                    Serial.println("Unable to save station favorite");
                    setStationFavorite(command.value, wasFavorite);
                }
                forceRedraw = true;
            }
            break;
        case UiCommandKind::RequestPodcastEpisodes:
            requestPodcastEpisodes(command.value);
            break;
        case UiCommandKind::PlayPodcastEpisode: {
            const int show = command.value / MAX_EPISODES;
            const int episode = command.value % MAX_EPISODES;
            playPodcastEpisode(show, episode);
            break;
        }
        case UiCommandKind::TogglePodcastPause:
            if (!togglePodcastPause()) Serial.println("Podcast pause unavailable");
            break;
        case UiCommandKind::SeekPodcast:
            if (!seekPodcastBySeconds(command.value)) Serial.println("Podcast seek unavailable");
            break;
        case UiCommandKind::TogglePodcastShowFavorite:
            if (togglePodcastShowFavorite(command.value) && !saveFavorites()) {
                // A failed save must not show a favorite that will disappear on reboot.
                togglePodcastShowFavorite(command.value);
                Serial.println("Unable to save show favorite");
            }
            forceRedraw = true;
            break;
        case UiCommandKind::EnterStandby:
            goToSleep();
            break;
        case UiCommandKind::None:
            break;
        }
    }
    if (volumeSavePending && now - lastVolChange >= 1000) {
        saveSettings();
        volumeSavePending = false;
    }

    mediaTick(now);
    updatePowerState(now);
    uiControllerTick(now);
    static char lastRenderedTime[10] = "";
    static bool lastRenderedTimeValid = false;
    static unsigned long lastPodcastProgressRenderAt = 0;
    const UiRenderState state = uiControllerRenderState();
    const PodcastPlaybackSnapshot podcastPlayback = podcastPlaybackSnapshot();
    const bool podcastProgressDue = state.page == UiPage::PodcastPlayer && podcastPlayback.active &&
        !podcastPlayback.paused && now - lastPodcastProgressRenderAt >= 1000;
    if (state.dirty || forceRedraw || podcastProgressDue) {
        renderRadioUi(state, currentTime, timeValid);
        forceRedraw = false;
        uiControllerMarkRendered();
        if (state.page == UiPage::PodcastPlayer) lastPodcastProgressRenderAt = now;
        strncpy(lastRenderedTime, currentTime, sizeof(lastRenderedTime));
        lastRenderedTime[sizeof(lastRenderedTime) - 1] = '\0';
        lastRenderedTimeValid = timeValid;
    } else if (lastRenderedTimeValid != timeValid || strcmp(lastRenderedTime, currentTime) != 0) {
        // Home's large clock sits over the photographic background. Rebuild that
        // composition once per minute rather than painting a flat rectangle over
        // it.  Live Stations has the same transparent-header requirement.
        if (state.page == UiPage::Home || state.page == UiPage::Stations ||
            state.page == UiPage::PodcastPlayer) {
            renderRadioUi(state, currentTime, timeValid);
            if (state.page == UiPage::PodcastPlayer) lastPodcastProgressRenderAt = now;
        } else {
            renderRadioUiClock(currentTime, timeValid);
        }
        strncpy(lastRenderedTime, currentTime, sizeof(lastRenderedTime));
        lastRenderedTime[sizeof(lastRenderedTime) - 1] = '\0';
        lastRenderedTimeValid = timeValid;
    }
}
