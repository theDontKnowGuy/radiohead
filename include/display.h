#pragma once

#include <Arduino.h>

uint16_t hexTo565(String hex);
void updateColors();
void setBrightness(int duty);
void drawAnalogVU();
void drawWeatherIcon(int x, int y, int weatherId);
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
