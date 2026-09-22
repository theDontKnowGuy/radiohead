#pragma once

#include <Arduino.h>

struct WeatherStatus {
    bool available = false;
    bool refreshing = false;
    time_t lastSuccess = 0;
};

enum class UiTarget : uint8_t;
struct UiRenderState;

uint16_t hexTo565(String hex);
void updateColors();
void setBrightness(int duty);
void drawAnalogVU();
void drawWeatherIcon(int x, int y, int weatherId);
// Starts the bounded background fetch only.  Rendering is deliberately separate
// so the legacy weather painter cannot overwrite the native UI composition.
void updateWeatherData();
void updateWeatherUI();
// A configuration save invalidates an older result and requests one bounded
// refresh.  Starting the task is not treated as a successful weather fetch.
void invalidateWeatherData();
WeatherStatus weatherStatus();
void drawWifiSignal(int x, int y);
void drawSpectrum();
bool drawPngAsset(
    const uint8_t* pngData,
    size_t pngLength,
    int32_t x = 0,
    int32_t y = 0,
    int32_t maxWidth = 0,
    int32_t maxHeight = 0);
UiTarget uiHitTest(const UiRenderState& state, int16_t x, int16_t y);
void renderRadioUi(const UiRenderState& state, const char* currentTime, bool timeValid);
// The Home subtitle is a bounded marquee. Keeping its small refresh separate
// from a complete page render leaves the audio service path responsive.
bool homeStationTitleRefreshDue(unsigned long now);
void renderHomeStationTitleTick();
// Presents the connected-network QR handoff before normal playback starts.
// The caller keeps HTTP servicing responsive while the screen is visible.
void showConfigurationQrScreen();
// OTA writes update this overlay from the web-server upload handler.  It is
// intentionally status-only: image validation and flash writes remain in the
// Update library and web_server ownership.
void setFirmwareUpdateProgress(bool active, uint8_t percent);
