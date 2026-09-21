#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

using TouchCalibration = std::array<uint16_t, 8>;

bool loadTouchCalibration(TouchCalibration& calibration);
void loadSettings();
bool saveTouchCalibration(const TouchCalibration& calibration);
void saveSettings();
// Coalesce rapid, user-driven web or encoder changes before touching flash.
void queueSettingsSave();
void serviceSettingsSave(unsigned long now);

// Favorites live in their own versioned namespace so legacy radio settings and
// their keys remain compatible.  Station IDs are catalog slots, not UI rows.
bool saveFavorites();
bool isStationFavorite(int stationIndex);
bool setStationFavorite(int stationIndex, bool favorite);
bool toggleStationFavorite(int stationIndex);
bool clearStationFavorite(int stationIndex);
bool isPodcastShowFavorite(int showIndex);
bool togglePodcastShowFavorite(int showIndex);

// Station artwork is a separately versioned LittleFS asset.  It is tied to the
// current stream URL, so a reused slot cannot inherit another station's logo.
bool stationArtworkBegin();
bool stationArtworkExists(int stationIndex);
uint32_t stationArtworkRevision(int stationIndex);
uint32_t stationArtworkContentRevision(int stationIndex);
bool stationArtworkUploadBegin(int stationIndex, uint32_t expectedRevision);
bool stationArtworkUploadWrite(const uint8_t* data, size_t length);
bool stationArtworkUploadFinish();
void stationArtworkUploadAbort();
bool removeStationArtwork(int stationIndex);
bool loadStationArtwork(int stationIndex, int size, uint16_t* pixels, size_t pixelCount);
