#pragma once

#include <array>
#include <cstdint>

using TouchCalibration = std::array<uint16_t, 8>;

bool loadTouchCalibration(TouchCalibration& calibration);
void loadSettings();
bool saveTouchCalibration(const TouchCalibration& calibration);
void saveSettings();
