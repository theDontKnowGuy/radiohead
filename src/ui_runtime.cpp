#include "ui_runtime.h"

#include "app_state.h"
#include "device_control.h"
#include "display.h"
#include "firmware_updater.h"
#include "media.h"
#include "settings.h"
#include "spotify_adapter.h"
#include "ui_controller.h"

void serviceUiInput(const DeviceInput& input, unsigned long now) {
    // The first accepted contact owns the press. TouchPressLatch stays latched
    // across navigation until release, including consumed dim-wake presses.
    const UiPage touchPage = uiControllerRenderState().page;

    if (input.touch == TouchEvent::Release || input.displayWasDimmed) {
        uiControllerTouchEnd();
    }
    const int detents = consumeEncoderDetents();
    uiControllerTurn(detents, now, input.displayWasDimmed);
    if (input.button == ButtonEvent::Push) {
        uiControllerPush(now, input.displayWasDimmed);
    } else if (input.button == ButtonEvent::Hold) {
        uiControllerHold(now, input.displayWasDimmed);
    }
    const UiRenderState touchState = uiControllerRenderState();
    if (input.touch == TouchEvent::Begin && touchState.page == touchPage && !input.displayWasDimmed) {
        uiControllerTap(uiHitTest(touchState, input.touchX, input.touchY), input.touchX, now, false);
    } else if (input.touch == TouchEvent::Contact && !input.displayWasDimmed) {
        uiControllerTouchContact(uiHitTest(touchState, input.touchX, input.touchY), now);
    }
    if (input.button != ButtonEvent::None || input.touch != TouchEvent::None) {
        lastInteraction = now;
    }
}

void serviceUiCommands(unsigned long now) {
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
            if (!playPodcastEpisode(show, episode)) {
                uiControllerReportPodcastStartFailure();
            }
            break;
        }
        case UiCommandKind::TogglePodcastPause:
            if (!togglePodcastPause()) Serial.println("Podcast pause unavailable");
            break;
        case UiCommandKind::SpotifyPrevious:
        case UiCommandKind::SpotifyTogglePause:
        case UiCommandKind::SpotifyNext:
#if defined(RADIO_SPOTIFY_EXPERIMENT)
            if (mediaSnapshot().source == MediaSource::Spotify &&
                (mediaSnapshot().status == MediaStatus::Playing ||
                 mediaSnapshot().status == MediaStatus::Paused)) {
                const SpotifyControl control = command.kind == UiCommandKind::SpotifyPrevious ?
                    SpotifyControl::Previous : command.kind == UiCommandKind::SpotifyNext ?
                    SpotifyControl::Next : SpotifyControl::TogglePause;
                spotifyAdapterControl(control);
            }
#endif
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
}
