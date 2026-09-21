#pragma once

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>

using TouchCalibration = std::array<uint16_t, 8>;

struct TimeZoneOption {
    const char* id;
    const char* label;
    const char* posixRule;
};

struct ArtworkStorageInfo {
    bool available = false;
    size_t usedBytes = 0;
    size_t totalBytes = 0;
};

bool loadTouchCalibration(TouchCalibration& calibration);
void loadSettings();
bool saveTouchCalibration(const TouchCalibration& calibration);
void saveSettings();
// Coalesce rapid, user-driven web or encoder changes before touching flash.
void queueSettingsSave();
void serviceSettingsSave(unsigned long now);

// The TFT exposes only these measured, bounded automatic-dim choices.  Keep
// old or corrupt persisted values on a predictable supported value.
uint16_t normalizeAutoDimSeconds(uint16_t seconds);

// Keep the existing Central European rule as the migration default. The table
// contains only zones with an implemented deterministic DST/no-DST rule.
const TimeZoneOption* supportedTimeZones(size_t& count);
bool isSupportedTimeZone(const String& id);
// Returns a safe, unambiguous timezone match for a supported city/country
// weather location, or nullptr when the location needs an explicit choice.
const char* timeZoneForWeatherLocation(const String& location);
void applyConfiguredTimeZone();
// Converts UTC using the selected rule set.  Most entries use newlib's POSIX
// TZ support; zones with rules it cannot express use deterministic firmware
// calculations instead.
bool configuredLocalTime(time_t utcTime, tm& localTime);
void formatConfiguredClock(char* destination, size_t destinationSize, const tm& value);

// These operations write only the settings they own.  They never return a
// Wi-Fi password or weather key, and let web handlers distinguish a durable
// write from an attempted network/weather operation.
bool saveWiFiCredentials(const String& ssid, const String& password);
bool clearWiFiCredentials();
bool saveWeatherTimeSettings();

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
bool clearAllStationArtwork();
bool loadStationArtwork(int stationIndex, int size, uint16_t* pixels, size_t pixelCount);
ArtworkStorageInfo artworkStorageInfo();
