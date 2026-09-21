#include "settings.h"

#include <cctype>
#include <cstring>
#include <LittleFS.h>

#include "app_state.h"
#include "display.h"

namespace {

constexpr uint8_t TOUCH_CALIBRATION_VERSION = 1;
constexpr uint8_t FAVORITES_VERSION = 2;
constexpr uint16_t STATION_FAVORITE_BITS = (1U << STATION_COUNT) - 1U;
constexpr uint16_t PODCAST_SHOW_FAVORITE_BITS = (1U << PODCAST_SHOW_COUNT) - 1U;
constexpr unsigned long SETTINGS_SAVE_DEBOUNCE_MS = 1000;
constexpr size_t STATION_ARTWORK_PAYLOAD_BYTES = (32 * 32 + 64 * 64 + 88 * 88) * 2;
constexpr size_t STATION_ARTWORK_HEADER_BYTES = 16;
constexpr size_t STATION_ARTWORK_PACKAGE_BYTES = STATION_ARTWORK_HEADER_BYTES + STATION_ARTWORK_PAYLOAD_BYTES;
constexpr uint8_t STATION_ARTWORK_MAGIC[] = {'R', 'H', 'A', '1'};

bool settingsSavePending = false;
unsigned long settingsSaveQueuedAt = 0;
bool artworkFilesystemReady = false;
File artworkUploadFile;
int artworkUploadSlot = -1;
uint32_t artworkUploadRevision = 0;
size_t artworkUploadBytes = 0;
bool artworkUploadFailed = false;
uint32_t artworkContentEpoch[STATION_COUNT] = {};

String artworkPath(int stationIndex, const char* suffix = "") {
    return "/rh-art-" + String(stationIndex) + suffix;
}

uint32_t fnv1a(const uint8_t* data, size_t length, uint32_t value = 2166136261UL) {
    for (size_t i = 0; i < length; ++i) {
        value ^= data[i];
        value *= 16777619UL;
    }
    return value;
}

uint32_t stationArtworkIdentity(int stationIndex) {
    if (stationIndex < 0 || stationIndex >= STATION_COUNT || stations[stationIndex].url.isEmpty()) return 0;
    return fnv1a(reinterpret_cast<const uint8_t*>(stations[stationIndex].url.c_str()), stations[stationIndex].url.length()) & 0x7fffffffUL;
}

uint32_t readLe32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) |
        (static_cast<uint32_t>(bytes[3]) << 24);
}

bool validateArtworkFile(File& file, uint32_t expectedIdentity) {
    if (!file || file.size() != STATION_ARTWORK_PACKAGE_BYTES) return false;
    uint8_t header[STATION_ARTWORK_HEADER_BYTES];
    if (file.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, STATION_ARTWORK_MAGIC, sizeof(STATION_ARTWORK_MAGIC)) != 0 ||
        readLe32(header + 4) != expectedIdentity) return false;
    uint32_t checksum = 2166136261UL;
    uint8_t buffer[256];
    size_t remaining = STATION_ARTWORK_PAYLOAD_BYTES;
    while (remaining > 0) {
        const size_t wanted = min(remaining, sizeof(buffer));
        if (file.read(buffer, wanted) != wanted) return false;
        checksum = fnv1a(buffer, wanted, checksum);
        remaining -= wanted;
    }
    return checksum == readLe32(header + 8);
}

bool isHexColor(const String& value) {
    if (value.length() != 7 || value[0] != '#') {
        return false;
    }
    for (size_t i = 1; i < value.length(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

void normalizeColor(String& value, const char* fallback) {
    if (!isHexColor(value)) {
        value = fallback;
    }
}

void truncate(String& value, size_t maximumLength) {
    if (value.length() > maximumLength) {
        value.remove(maximumLength);
    }
}

bool isValidTouchCalibration(const TouchCalibration& calibration) {
    uint16_t minimumX = 4095;
    uint16_t maximumX = 0;
    uint16_t minimumY = 4095;
    uint16_t maximumY = 0;

    for (size_t i = 0; i < calibration.size(); i += 2) {
        const uint16_t x = calibration[i];
        const uint16_t y = calibration[i + 1];
        if (x <= 128 || x > 3968 || y <= 128 || y > 3968) {
            return false;
        }
        minimumX = min(minimumX, x);
        maximumX = max(maximumX, x);
        minimumY = min(minimumY, y);
        maximumY = max(maximumY, y);
    }

    return maximumX - minimumX >= 1000 && maximumY - minimumY >= 1000;
}

}  // namespace

bool saveFavorites() {
    if (!pref.begin("favorites", false)) {
        return false;
    }
    const uint16_t normalizedMask = stationFavoriteMask & STATION_FAVORITE_BITS;
    const uint16_t normalizedShowMask = podcastShowFavoriteMask & PODCAST_SHOW_FAVORITE_BITS;
    // Write the version last so an interrupted write is treated as absent on
    // the next boot rather than as a partially migrated favorite set.
    pref.putUChar("version", 0);
    const bool maskSaved = pref.putUShort("stations", normalizedMask) == sizeof(uint16_t);
    const bool showsSaved = maskSaved &&
        pref.putUShort("shows", normalizedShowMask) == sizeof(uint16_t);
    const bool versionSaved = showsSaved &&
        pref.putUChar("version", FAVORITES_VERSION) == sizeof(uint8_t);
    pref.end();
    return showsSaved && versionSaved;
}

bool isStationFavorite(int stationIndex) {
    return stationIndex >= 0 && stationIndex < STATION_COUNT &&
        (stationFavoriteMask & (1U << stationIndex)) != 0;
}

bool setStationFavorite(int stationIndex, bool favorite) {
    if (stationIndex < 0 || stationIndex >= STATION_COUNT) {
        return false;
    }
    const uint16_t bit = 1U << stationIndex;
    const uint16_t updatedMask = favorite
        ? static_cast<uint16_t>(stationFavoriteMask | bit)
        : static_cast<uint16_t>(stationFavoriteMask & ~bit);
    if (updatedMask == stationFavoriteMask) {
        return false;
    }
    stationFavoriteMask = updatedMask;
    return true;
}

bool toggleStationFavorite(int stationIndex) {
    return setStationFavorite(stationIndex, !isStationFavorite(stationIndex));
}

bool clearStationFavorite(int stationIndex) {
    return setStationFavorite(stationIndex, false);
}

bool isPodcastShowFavorite(int showIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT) return false;
    const uint8_t slot = podcastShows[showIndex].favoriteSlot;
    return slot < PODCAST_SHOW_COUNT && (podcastShowFavoriteMask & (1U << slot)) != 0;
}

bool togglePodcastShowFavorite(int showIndex) {
    if (showIndex < 0 || showIndex >= PODCAST_SHOW_COUNT) return false;
    const uint8_t slot = podcastShows[showIndex].favoriteSlot;
    if (slot >= PODCAST_SHOW_COUNT || podcastShows[showIndex].favoriteId == nullptr ||
        podcastShows[showIndex].favoriteId[0] == '\0') return false;
    podcastShowFavoriteMask ^= static_cast<uint16_t>(1U << slot);
    return true;
}

bool stationArtworkBegin() {
    if (artworkFilesystemReady) return true;
    // Never format from normal firmware startup: a mount problem must not erase
    // logos that may still be recoverable.
    artworkFilesystemReady = LittleFS.begin(false, "/littlefs", 4, "spiffs");
    if (!artworkFilesystemReady) return false;
    for (int slot = 0; slot < STATION_COUNT; ++slot) {
        const String active = artworkPath(slot);
        const String backup = artworkPath(slot, ".bak");
        if (!LittleFS.exists(active) && LittleFS.exists(backup)) {
            LittleFS.rename(backup, active);
        } else if (LittleFS.exists(backup)) {
            LittleFS.remove(backup);
        }
        LittleFS.remove(artworkPath(slot, ".tmp"));
    }
    return true;
}

uint32_t stationArtworkRevision(int stationIndex) {
    return stationArtworkIdentity(stationIndex);
}

uint32_t stationArtworkContentRevision(int stationIndex) {
    if (stationIndex < 0 || stationIndex >= STATION_COUNT) return 0;
    return stationArtworkIdentity(stationIndex) ^ artworkContentEpoch[stationIndex];
}

bool stationArtworkExists(int stationIndex) {
    if (!stationArtworkBegin() || stationArtworkIdentity(stationIndex) == 0) return false;
    File file = LittleFS.open(artworkPath(stationIndex), FILE_READ);
    uint8_t header[STATION_ARTWORK_HEADER_BYTES];
    const bool valid = file && file.size() == STATION_ARTWORK_PACKAGE_BYTES &&
        file.read(header, sizeof(header)) == sizeof(header) &&
        memcmp(header, STATION_ARTWORK_MAGIC, sizeof(STATION_ARTWORK_MAGIC)) == 0 &&
        readLe32(header + 4) == stationArtworkIdentity(stationIndex);
    file.close();
    return valid;
}

bool stationArtworkUploadBegin(int stationIndex, uint32_t expectedRevision) {
    stationArtworkUploadAbort();
    if (!stationArtworkBegin() || stationIndex < 0 || stationIndex >= STATION_COUNT ||
        expectedRevision == 0 || expectedRevision != stationArtworkIdentity(stationIndex)) return false;
    const String temporary = artworkPath(stationIndex, ".tmp");
    LittleFS.remove(temporary);
    artworkUploadFile = LittleFS.open(temporary, FILE_WRITE);
    if (!artworkUploadFile) return false;
    artworkUploadSlot = stationIndex;
    artworkUploadRevision = expectedRevision;
    artworkUploadBytes = 0;
    artworkUploadFailed = false;
    return true;
}

bool stationArtworkUploadWrite(const uint8_t* data, size_t length) {
    if (!artworkUploadFile || !data || artworkUploadFailed ||
        length > STATION_ARTWORK_PACKAGE_BYTES - artworkUploadBytes) {
        artworkUploadFailed = true;
        return false;
    }
    if (artworkUploadFile.write(data, length) != length) {
        artworkUploadFailed = true;
        return false;
    }
    artworkUploadBytes += length;
    return true;
}

void stationArtworkUploadAbort() {
    if (artworkUploadFile) artworkUploadFile.close();
    if (artworkUploadSlot >= 0 && artworkFilesystemReady) LittleFS.remove(artworkPath(artworkUploadSlot, ".tmp"));
    artworkUploadSlot = -1;
    artworkUploadRevision = 0;
    artworkUploadBytes = 0;
    artworkUploadFailed = false;
}

bool stationArtworkUploadFinish() {
    const int slot = artworkUploadSlot;
    if (!artworkUploadFile || artworkUploadFailed || artworkUploadBytes != STATION_ARTWORK_PACKAGE_BYTES ||
        slot < 0 || artworkUploadRevision != stationArtworkIdentity(slot)) {
        stationArtworkUploadAbort();
        return false;
    }
    artworkUploadFile.close();
    File candidate = LittleFS.open(artworkPath(slot, ".tmp"), FILE_READ);
    const bool valid = validateArtworkFile(candidate, artworkUploadRevision);
    candidate.close();
    if (!valid || artworkUploadRevision != stationArtworkIdentity(slot)) {
        stationArtworkUploadAbort();
        return false;
    }
    const String active = artworkPath(slot);
    const String backup = artworkPath(slot, ".bak");
    LittleFS.remove(backup);
    ++artworkContentEpoch[slot];
    if (LittleFS.exists(active) && !LittleFS.rename(active, backup)) {
        stationArtworkUploadAbort();
        return false;
    }
    if (!LittleFS.rename(artworkPath(slot, ".tmp"), active)) {
        if (LittleFS.exists(backup)) LittleFS.rename(backup, active);
        stationArtworkUploadAbort();
        return false;
    }
    LittleFS.remove(backup);
    artworkUploadSlot = -1;
    artworkUploadRevision = 0;
    artworkUploadBytes = 0;
    artworkUploadFailed = false;
    return true;
}

bool removeStationArtwork(int stationIndex) {
    if (!stationArtworkBegin() || stationIndex < 0 || stationIndex >= STATION_COUNT) return false;
    bool removed = false;
    for (const char* suffix : {"", ".tmp", ".bak"}) {
        const String path = artworkPath(stationIndex, suffix);
        if (LittleFS.exists(path)) removed = LittleFS.remove(path) || removed;
    }
    if (removed) ++artworkContentEpoch[stationIndex];
    return removed;
}

bool loadStationArtwork(int stationIndex, int size, uint16_t* pixels, size_t pixelCount) {
    const size_t offset = size == 32 ? 0 : size == 64 ? 32 * 32 * 2 : size == 88 ? (32 * 32 + 64 * 64) * 2 : SIZE_MAX;
    const size_t bytes = size > 0 ? static_cast<size_t>(size) * size * 2 : 0;
    if (offset == SIZE_MAX || !pixels || pixelCount < static_cast<size_t>(size) * size ||
        !stationArtworkBegin()) return false;
    File file = LittleFS.open(artworkPath(stationIndex), FILE_READ);
    if (!validateArtworkFile(file, stationArtworkIdentity(stationIndex)) ||
        !file.seek(STATION_ARTWORK_HEADER_BYTES + offset)) {
        file.close();
        return false;
    }
    uint8_t buffer[128];
    size_t pixel = 0;
    size_t remaining = bytes;
    while (remaining > 0) {
        const size_t wanted = min(remaining, sizeof(buffer));
        if (file.read(buffer, wanted) != wanted) {
            file.close();
            return false;
        }
        for (size_t i = 0; i < wanted; i += 2) {
            pixels[pixel++] = static_cast<uint16_t>(buffer[i] << 8) | buffer[i + 1];
        }
        remaining -= wanted;
    }
    file.close();
    return true;
}

bool loadTouchCalibration(TouchCalibration& calibration) {
    if (!pref.begin("touch", true)) {
        return false;
    }

    const bool hasCurrentVersion =
        pref.getUChar("version", 0) == TOUCH_CALIBRATION_VERSION;
    const bool hasExpectedSize = pref.getBytesLength("data") == sizeof(calibration);
    const size_t bytesRead = hasCurrentVersion && hasExpectedSize
        ? pref.getBytes("data", calibration.data(), sizeof(calibration))
        : 0;
    pref.end();

    return bytesRead == sizeof(calibration) && isValidTouchCalibration(calibration);
}

bool saveTouchCalibration(const TouchCalibration& calibration) {
    if (!isValidTouchCalibration(calibration) || !pref.begin("touch", false)) {
        return false;
    }

    pref.putUChar("version", 0);
    const bool dataSaved =
        pref.putBytes("data", calibration.data(), sizeof(calibration)) == sizeof(calibration);
    const bool versionSaved =
        dataSaved && pref.putUChar("version", TOUCH_CALIBRATION_VERSION) == sizeof(uint8_t);
    pref.end();
    return dataSaved && versionSaved;
}

void saveSettings() {
    settingsSavePending = false;
    pref.begin("radio", false);
    pref.putInt("idx", currentStationIdx);
    pref.putInt("vol", mainVal);
    pref.putInt("bass", gB);
    pref.putInt("mid", gM);
    pref.putInt("treb", gT);
    pref.putBool("spec", showSpectrum);
    pref.putInt("almH", alarmH);
    pref.putInt("almM", alarmM);
    pref.putBool("almA", alarmActive);
    pref.putString("cTop", currentSkin.hexTop);
    pref.putString("cBot", currentSkin.hexBottom);
    pref.putString("cMain", currentSkin.hexMain);
    pref.putString("cAcc", currentSkin.hexAccent);
    pref.putString("cWifi", currentSkin.hexWifi);
    pref.putString("owmCity", owmCity);
    pref.putString("owmKey", owmKey);
    pref.putBool("useCelsius", useCelsius);
    pref.putString("cSel", currentSkin.hexSel);
    pref.putString("cClk", currentSkin.hexClk);
    pref.putString("cHInf", currentSkin.hexHInfo);
    pref.putString("cBarL", currentSkin.hexBarL);
    pref.putString("cBarM", currentSkin.hexBarM);
    pref.putString("cBarH", currentSkin.hexBarH);
    pref.putString("cVol", currentSkin.hexVol);
    pref.putString("cAlm", currentSkin.hexAlm);

    for (int i = 0; i < STATION_COUNT; ++i) {
        pref.putString(("n" + String(i)).c_str(), stations[i].name);
        pref.putString(("u" + String(i)).c_str(), stations[i].url);
    }
    pref.end();
}

void queueSettingsSave() {
    settingsSaveQueuedAt = millis();
    settingsSavePending = true;
}

void serviceSettingsSave(unsigned long now) {
    if (settingsSavePending && now - settingsSaveQueuedAt >= SETTINGS_SAVE_DEBOUNCE_MS) {
        saveSettings();
    }
}

void loadSettings() {
    pref.begin("radio", true);
    currentStationIdx = pref.getInt("idx", 0);
    mainVal = pref.getInt("vol", 5);
    gB = pref.getInt("bass", 0);
    gM = pref.getInt("mid", 0);
    gT = pref.getInt("treb", 0);
    showSpectrum = pref.getBool("spec", true);
    alarmH = pref.getInt("almH", 7);
    alarmM = pref.getInt("almM", 0);
    alarmActive = pref.getBool("almA", false);
    st_ssid = pref.getString("ssid", "");
    st_pass = pref.getString("pass", "");
    owmCity = pref.getString("owmCity", "Budapest,HU");
    owmKey = pref.getString("owmKey", "");
    useCelsius = pref.getBool("useCelsius", true);

    if (st_ssid.length() > 32) {
        st_ssid = "";
        st_pass = "";
    } else if (st_pass.length() > 63) {
        st_pass = "";
    }
    if (owmCity.isEmpty() || owmCity.length() > 80) {
        owmCity = "Budapest,HU";
    }
    if (owmKey.length() > 128) {
        owmKey = "";
    }
    currentSkin.hexTop = pref.getString("cTop", "#000000");
    currentSkin.hexBottom = pref.getString("cBot", "#000000");
    currentSkin.hexMain = pref.getString("cMain", "#FFFFFF");
    currentSkin.hexAccent = pref.getString("cAcc", "#00FFFF");
    currentSkin.hexWifi = pref.getString("cWifi", "#00FF00");
    currentSkin.hexSel = pref.getString("cSel", "#0000FF");
    currentSkin.hexClk = pref.getString("cClk", "#FFFFFF");
    currentSkin.hexHInfo = pref.getString("cHInf", "#FFFFFF");
    currentSkin.hexBarL = pref.getString("cBarL", "#00FF00");
    currentSkin.hexBarM = pref.getString("cBarM", "#FFFF00");
    currentSkin.hexBarH = pref.getString("cBarH", "#FF0000");
    currentSkin.hexVol = pref.getString("cVol", "#00FFFF");
    currentSkin.hexAlm = pref.getString("cAlm", "#FF0000");

    normalizeColor(currentSkin.hexTop, "#000000");
    normalizeColor(currentSkin.hexBottom, "#000000");
    normalizeColor(currentSkin.hexMain, "#FFFFFF");
    normalizeColor(currentSkin.hexAccent, "#00FFFF");
    normalizeColor(currentSkin.hexWifi, "#00FF00");
    normalizeColor(currentSkin.hexSel, "#0000FF");
    normalizeColor(currentSkin.hexClk, "#FFFFFF");
    normalizeColor(currentSkin.hexHInfo, "#FFFFFF");
    normalizeColor(currentSkin.hexBarL, "#00FF00");
    normalizeColor(currentSkin.hexBarM, "#FFFF00");
    normalizeColor(currentSkin.hexBarH, "#FF0000");
    normalizeColor(currentSkin.hexVol, "#00FFFF");
    normalizeColor(currentSkin.hexAlm, "#FF0000");
    updateColors();

    for (int i = 0; i < STATION_COUNT; ++i) {
        stations[i].name = pref.getString(("n" + String(i)).c_str(), stations[i].name);
        stations[i].url = pref.getString(("u" + String(i)).c_str(), stations[i].url);
        truncate(stations[i].name, 80);
        truncate(stations[i].url, 512);
    }
    pref.end();

    currentStationIdx = constrain(currentStationIdx, 0, STATION_COUNT - 1);
    mainVal = constrain(mainVal, 0, 21);
    gB = constrain(gB, -15, 15);
    gM = constrain(gM, -15, 15);
    gT = constrain(gT, -15, 15);
    alarmH = constrain(alarmH, 0, 23);
    alarmM = constrain(alarmM, 0, 59);
    tempStationIdx = currentStationIdx;

    stationFavoriteMask = 0;
    podcastShowFavoriteMask = 0;
    if (pref.begin("favorites", true)) {
        const uint8_t favoritesVersion = pref.getUChar("version", 0);
        if (favoritesVersion == 1 || favoritesVersion == FAVORITES_VERSION) {
            stationFavoriteMask = pref.getUShort("stations", 0) & STATION_FAVORITE_BITS;
            if (favoritesVersion == FAVORITES_VERSION) {
                podcastShowFavoriteMask = pref.getUShort("shows", 0) & PODCAST_SHOW_FAVORITE_BITS;
            }
        }
        pref.end();
    }
}
