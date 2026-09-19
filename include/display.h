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

