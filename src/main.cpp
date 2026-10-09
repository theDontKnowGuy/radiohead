//Radiohead  by Artisan Electronics

#include <Arduino.h>
#include <cstring>
#include <time.h>

#include "app_state.h"
#include "boot_screen.h"
#include "device_control.h"
#include "display.h"
#include "firmware_updater.h"
#include "media.h"
#include "wifi_network.h"
#include "settings.h"
#include "ui_controller.h"
#include "ui_runtime.h"
#include "validation_diagnostics.h"
#include "web_server.h"

void setup() {
    mediaPrepareBootOutput();
    Serial.begin(115200);
    validationBegin();

    initializeDisplay();
    // A held encoder requests browser-based Wi-Fi setup. Touch calibration is
    // an explicit Settings action and never blocks startup.
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

    mediaConfigureOutput();
    const unsigned long networkJoinStartedAt = millis();
    if (!isAP && st_ssid.isEmpty()) {
        startSetupAccessPoint(SetupAccessReason::NoCredentials);
    } else if (!isAP) {
        beginNetworkConnection();
    }
    BootScreen::draw();
    const unsigned long bootScreenStartedAt = millis();
    if (BootScreen::AudioStartDelayMs == 0) BootScreen::startAudio();
    BootScreen::hold(bootScreenStartedAt);
    showNetworkQrScreen();
    startWebServer();
    if (!isAP) {
        const unsigned long configurationScreenStartedAt = millis();
        bool connectedShown = !networkJoinPending();
        // The QR screen gets its normal ten seconds even while joining. Only
        // entry into the radio waits for any remaining Wi-Fi join budget.
        while (millis() - configurationScreenStartedAt < 10000 ||
               (networkJoinPending() &&
                millis() - networkJoinStartedAt < NETWORK_JOIN_TIMEOUT_MS)) {
            serviceWebConnectivity();
            server.handleClient();
            serviceWebNetworkRequests(millis());
            if (mediaLocalAudioAvailable()) audio.loop();
            const bool connected = !networkJoinPending();
            if (connected != connectedShown) {
                connectedShown = connected;
                showNetworkQrScreen();
            }
            delay(2);
        }
    }
    finishNetworkConnection();
    serviceWebConnectivity();
    if (isAP) showNetworkQrScreen();
    validationEvent(isAP ? "setup_ap" : "wifi_ready");

    mediaStartSavedPlayback();
    validationEvent("startup_playback_requested");

    startDeviceControl();
    lastInteraction = millis();
    uiControllerBegin();
    if (!isAP) {
        firmwareUpdater.begin(firmwareAutoUpdate);
    }
    forceRedraw = true;
}

void loop() {
    validationLoopEnter();
    const uint32_t audioStartedUs = micros();
    if (mediaLocalAudioAvailable()) audio.loop();
    validationAudioServiced(micros() - audioStartedUs);
    const uint32_t webStartedUs = micros();
    serviceWebConnectivity();
    server.handleClient();
    validationWebServiced(micros() - webStartedUs);
    serviceWebNetworkRequests(millis());
    serviceWifiRoaming(millis());
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
    const DeviceInput input = pollDeviceInput(now);
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

    serviceUiInput(input, now);
    serviceUiCommands(now);
    serviceSettingsSave(now);

    mediaTick(now);
    updatePowerState(now);
    uiControllerTick(now);
    serviceDisplayRefresh(now, currentTime, timeValid);
    validationTick();
}
