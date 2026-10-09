#pragma once

#include <Arduino.h>

struct WeatherStatus {
    bool available = false;
    bool refreshing = false;
    time_t lastSuccess = 0;
};

enum class UiTarget : uint8_t;
struct UiRenderState;

void setBrightness(int duty);
// Starts the bounded background fetch only. Rendering uses the native UI.
void updateWeatherData();
// A configuration save invalidates an older result and requests one bounded
// refresh.  Starting the task is not treated as a successful weather fetch.
void invalidateWeatherData();
WeatherStatus weatherStatus();
bool drawPngAsset(
    const uint8_t* pngData,
    size_t pngLength,
    int32_t x = 0,
    int32_t y = 0,
    int32_t maxWidth = 0,
    int32_t maxHeight = 0);
UiTarget uiHitTest(const UiRenderState& state, int16_t x, int16_t y);
void renderRadioUi(const UiRenderState& state, const char* currentTime, bool timeValid);
uint8_t wifiSignalLevel();
// The Home subtitle is a bounded marquee. Keeping its small refresh separate
// from a complete page render leaves the audio service path responsive.
bool homeStationTitleRefreshDue(unsigned long now);
void renderHomeStationTitleTick();
// Presents the branded network handoff for a joining/connected LAN or the
// setup access point. The caller refreshes on connectivity changes and keeps
// HTTP servicing responsive while the timed startup handoff is visible.
void showNetworkQrScreen();
// OTA writes update this overlay from the web-server upload handler.  It is
// intentionally status-only: image validation and flash writes remain in the
// Update library and web_server ownership.
void setFirmwareUpdateProgress(bool active, uint8_t percent);

// Restore the backlight after sleep and initialize the landscape TFT.
void initializeDisplay();
// Own redraw scheduling and its cached clock/progress/signal state.
void serviceDisplayRefresh(unsigned long now, const char* currentTime, bool timeValid);
