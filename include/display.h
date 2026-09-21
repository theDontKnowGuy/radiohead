#pragma once

#include <Arduino.h>

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
