#pragma once

#include <array>
#include <cstdint>

using TouchCalibration = std::array<uint16_t, 8>;

bool loadTouchCalibration(TouchCalibration& calibration);
void loadSettings();
bool saveTouchCalibration(const TouchCalibration& calibration);
void saveSettings();

// Favorites live in their own versioned namespace so legacy radio settings and
// their keys remain compatible.  Station IDs are catalog slots, not UI rows.
bool saveFavorites();
bool isStationFavorite(int stationIndex);
bool setStationFavorite(int stationIndex, bool favorite);
bool toggleStationFavorite(int stationIndex);
bool clearStationFavorite(int stationIndex);
