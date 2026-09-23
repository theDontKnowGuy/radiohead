/* INTERNET RADIO PROJECT - ESP32-S3 */
/* Original project by Gabor Nemes. */

#include <Arduino.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "app_state.h"
#include "boot_screen.h"
#include "device_control.h"
#include "display.h"
#include "firmware_updater.h"
#include "media.h"
#include "settings.h"
#include "ui_controller.h"
#include "web_server.h"

namespace {

constexpr unsigned long kNetworkJoinTimeoutMs = 30UL * 500UL;
static_assert(kNetworkJoinTimeoutMs >= BootScreen::HoldMs);
bool networkJoinStarted = false;

bool startSetupAccessPoint(SetupAccessReason reason) {
    // Retain the station interface for the configuration page's asynchronous
    // network scan while presenting the open setup AP.
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(kSetupAccessPointSsid)) {
        Serial.println("[wifi] setup access point failed to start");
        return false;
    }
    isAP = true;
    setupAccessReason = reason;
    Serial.print("[wifi] setup access point: ");
    Serial.println(WiFi.softAPIP());
    return true;
}

void beginNetworkConnection() {
    WiFi.mode(WIFI_STA);
    // This must precede WiFi.begin() so the DHCP request and the mDNS responder
    // agree on the radio's name.
    WiFi.setHostname(kRadioMdnsHostname);
    WiFi.begin(st_ssid.c_str(), st_pass.c_str());
    networkJoinStarted = true;
}

bool networkJoinPending() {
    return networkJoinStarted && WiFi.status() != WL_CONNECTED;
}

void finishNetworkConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        startSetupAccessPoint(SetupAccessReason::ConnectionFailed);
        return;
    }

    Serial.println(WiFi.localIP());
    configTime(0, 0, ntpServer);
    applyConfiguredTimeZone();
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

    const bool dimTimeoutExpired =
        autoDimSeconds != AUTO_DIM_NEVER_SECONDS &&
        now - lastInteraction > static_cast<unsigned long>(autoDimSeconds) * 1000UL;
    if (dimTimeoutExpired && !isAlarming) {
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
    // A held encoder requests browser-based Wi-Fi setup.  Start the AP before
    // the potentially interactive calibration flow so it is already reachable
    // as soon as calibration finishes; the held press still intentionally
    // requests recalibration in initializeTouchCalibration().
    pinMode(PIN_SW, INPUT_PULLUP);
    const bool setupRequestedAtBoot = digitalRead(PIN_SW) == LOW;
    if (setupRequestedAtBoot) {
        startSetupAccessPoint(SetupAccessReason::Requested);
    }
    initializeTouchCalibration();
    loadSettings();
    // Initialize artwork storage once, including the user-authorized recovery
    // of the currently corrupt LittleFS partition, before web rendering can
    // ask for station thumbnails.
    stationArtworkBegin();

    BootScreen::draw();
    const unsigned long bootScreenStartedAt = millis();
    if (!isAP && st_ssid.isEmpty()) {
        startSetupAccessPoint(SetupAccessReason::NoCredentials);
    } else if (!isAP) {
        beginNetworkConnection();
    }
    BootScreen::hold(
        bootScreenStartedAt, networkJoinPending, kNetworkJoinTimeoutMs);
    if (networkJoinStarted) finishNetworkConnection();

    startWebServer();
    showNetworkQrScreen();
    if (!isAP) {
        const unsigned long configurationScreenStartedAt = millis();
        while (millis() - configurationScreenStartedAt < 10000) {
            server.handleClient();
            delay(2);
        }
    }

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
    if (!isAP) {
        firmwareUpdater.begin(firmwareAutoUpdate);
    }
    forceRedraw = true;
}

void loop() {
    audio.loop();
    server.handleClient();
    serviceWebNetworkRequests(millis());
    updateWeatherData();

    const unsigned long now = millis();
    // The update worker publishes only discrete state changes. Sampling its
    // revision avoids locking its String state in the hot audio path while
    // still repainting the TFT when a manual release becomes available.
    static uint32_t lastFirmwareUpdateRevision = 0;
    const uint32_t firmwareUpdateRevision = firmwareUpdater.statusRevision();
    if (firmwareUpdateRevision != lastFirmwareUpdateRevision) {
        lastFirmwareUpdateRevision = firmwareUpdateRevision;
        forceRedraw = true;
    }
    const bool displayWasDimmed = isDimmed;
    const ButtonEvent buttonEvent = pollEncoderButton(now);
    int16_t touchX = 0;
    int16_t touchY = 0;
    const TouchEvent touchEvent = pollTouchEvent(touchX, touchY, now);
    struct tm timeInfo = {};
    const time_t wallClock = time(nullptr);
    const bool timeValid =
        wallClock >= 1483228800 && configuredLocalTime(wallClock, timeInfo);
    char currentTime[12];
    if (timeValid) {
        formatConfiguredClock(currentTime, sizeof(currentTime), timeInfo);
    } else {
        strcpy(currentTime, "--:--");
    }

    const bool alarmWasActive = isAlarming;
    updateAlarm(timeInfo, timeValid, buttonEvent == ButtonEvent::Push, now);
    uiControllerSetAlarmActive(isAlarming);

    // The first accepted contact owns the press. TouchPressLatch stays latched
    // across navigation until release, including consumed dim/alarm presses.
    const UiPage touchPage = uiControllerRenderState().page;

    if (touchEvent == TouchEvent::Release || displayWasDimmed || alarmWasActive) {
        uiControllerTouchEnd();
    }
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
        } else if (touchEvent == TouchEvent::Contact && !displayWasDimmed && !isAlarming) {
            uiControllerTouchContact(uiHitTest(state, touchX, touchY), now);
        }
    }
    if (buttonEvent != ButtonEvent::None || touchEvent != TouchEvent::None) {
        lastInteraction = now;
    }

    UiCommand command;
    while (uiControllerTakeCommand(command)) {
        switch (command.kind) {
        case UiCommandKind::ChangeVolume:
            setRadioVolumeIndex(mainVal + command.value);
            queueSettingsSave();
            break;
        case UiCommandKind::SetVolume:
            setRadioVolumeIndex(command.value);
            queueSettingsSave();
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
        case UiCommandKind::PreviewTone:
            // Preview affects the audio processor only. Unrelated saves (e.g.
            // encoder volume or sleep) continue to persist committed tone.
            audio.setTone(constrain(command.value, -15, 15),
                          constrain(command.secondary, -15, 15),
                          constrain(command.tertiary, -15, 15));
            break;
        case UiCommandKind::ApplyTone:
            gB = constrain(command.value, -15, 15);
            gM = constrain(command.secondary, -15, 15);
            gT = constrain(command.tertiary, -15, 15);
            audio.setTone(gB, gM, gT);
            saveSettings();
            forceRedraw = true;
            break;
        case UiCommandKind::ApplyAutoDim:
            autoDimSeconds = normalizeAutoDimSeconds(command.value);
            // A newly committed timeout starts from this explicit interaction,
            // rather than immediately dimming because an older timeout expired.
            lastInteraction = now;
            saveSettings();
            forceRedraw = true;
            break;
        case UiCommandKind::RequestFirmwareUpdateCheck:
            firmwareUpdater.requestCheckNow();
            forceRedraw = true;
            break;
        case UiCommandKind::RequestFirmwareUpdateInstall:
            firmwareUpdater.requestInstallNow();
            forceRedraw = true;
            break;
        case UiCommandKind::SetFirmwareAutoInstall: {
            const bool enabled = command.value != 0;
            if (!saveFirmwareAutoUpdate(enabled)) {
                Serial.println("Could not save firmware update policy");
                break;
            }
            firmwareUpdater.setAutoInstall(enabled);
            forceRedraw = true;
            break;
        }
        case UiCommandKind::StartTouchCalibration:
            // Calibration is intentionally an explicit maintenance flow. It is
            // the one local operation that must temporarily take over the TFT.
            startTouchCalibration();
            lastInteraction = millis();
            forceRedraw = true;
            break;
        case UiCommandKind::RestartDevice:
            ESP.restart();
            break;
        case UiCommandKind::FactoryResetDevice:
            if (!factoryReset()) {
                uiControllerReportDeviceActionFailure();
            }
            forceRedraw = true;
            break;
        case UiCommandKind::ConnectSavedWiFi:
            if (activateSavedWiFiNetwork(command.value)) {
                ESP.restart();
            } else {
                uiControllerReportWifiActionFailure();
            }
            forceRedraw = true;
            break;
        case UiCommandKind::ForgetActiveWiFi:
            if (forgetActiveWiFiNetwork()) {
                ESP.restart();
            } else {
                uiControllerReportWifiActionFailure();
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
    serviceSettingsSave(now);

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
    const bool homeStationTitleDue = state.page == UiPage::Home && homeStationTitleRefreshDue(now);
    if (state.dirty || forceRedraw || podcastProgressDue) {
        renderRadioUi(state, currentTime, timeValid);
        forceRedraw = false;
        uiControllerMarkRendered();
        if (state.page == UiPage::PodcastPlayer) lastPodcastProgressRenderAt = now;
        strncpy(lastRenderedTime, currentTime, sizeof(lastRenderedTime));
        lastRenderedTime[sizeof(lastRenderedTime) - 1] = '\0';
        lastRenderedTimeValid = timeValid;
    } else if (lastRenderedTimeValid != timeValid || strcmp(lastRenderedTime, currentTime) != 0) {
        // Each screen owns its complete header. Rebuild it on a minute change
        // instead of overlaying the retired legacy status strip on top.
        renderRadioUi(state, currentTime, timeValid);
        if (state.page == UiPage::PodcastPlayer) lastPodcastProgressRenderAt = now;
        strncpy(lastRenderedTime, currentTime, sizeof(lastRenderedTime));
        lastRenderedTime[sizeof(lastRenderedTime) - 1] = '\0';
        lastRenderedTimeValid = timeValid;
    } else if (homeStationTitleDue) {
        renderHomeStationTitleTick();
    }
}
