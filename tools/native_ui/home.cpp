#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <lgfx/v1/LGFX_Sprite.hpp>
#include "ConfigQrCode.h"
#include "display_fonts.h"
#include "ui_controller.h"
#include "ui_text.h"
#include "ui_background_asset.h"
#include "ui_header_assets.h"
#include "ui_home_assets.h"
#include "ui_home_clock_atlas.h"
#include "ui_home_temperature_atlas.h"
#include "ui_podcast_assets.h"
#include "ui_list_assets.h"
#include "ui_player_assets.h"

// Only device state is adapted. Rendering, layout, glyph metrics, PNG decoding
// and RGB565 blending are production code.
namespace lgfx { inline namespace v1 {
unsigned long millis() { return 0; }
unsigned long micros() { return 0; }
void delay(unsigned long) {}
void delayMicroseconds(unsigned int) {}
}}
using namespace lgfx::v1;
lgfx::LGFX_Sprite frame;
bool antialias = true;
#define uiFrameReady antialias
lgfx::LGFXBase& canvas() { return frame; }
const lgfx::IFont* uiFont(const lgfx::IFont* font) {
    return antialias ? display_fonts::smooth(font) : font;
}
unsigned audioServiceCalls = 0;
void serviceUiAudio() { ++audioServiceCalls; }
void drawNetworkQrHandoff(bool) {}
void drawQrFixture(int x, int y, int maximum, int modules) {
    const int side = (maximum / modules) * modules;
    frame.fillRoundRect(x, y, side, side, 8, TFT_WHITE);
    const int scale = side / modules;
    const int originX = x;
    const int originY = y;
    const int farFinder = modules - 11;
    const int codeEnd = modules - 4;
    auto finder = [&](int moduleX, int moduleY) {
        frame.fillRect(originX + moduleX * scale, originY + moduleY * scale,
                       7 * scale, 7 * scale, TFT_BLACK);
        frame.fillRect(originX + (moduleX + 1) * scale, originY + (moduleY + 1) * scale,
                       5 * scale, 5 * scale, TFT_WHITE);
        frame.fillRect(originX + (moduleX + 2) * scale, originY + (moduleY + 2) * scale,
                       3 * scale, 3 * scale, TFT_BLACK);
    };
    finder(4, 4);
    finder(farFinder, 4);
    finder(4, farFinder);
    for (int row = 4; row < codeEnd; ++row) {
        for (int column = 4; column < codeEnd; ++column) {
            const bool finderArea =
                (row < 11 && column < 11) || (row < 11 && column >= farFinder) ||
                (row >= farFinder && column < 11);
            if (!finderArea && ((row * 7 + column * 11 + row * column) % 5 < 2)) {
                frame.fillRect(originX + column * scale, originY + row * scale,
                               scale, scale, TFT_BLACK);
            }
        }
    }
}
void drawConfigurationQrBadge(int x, int y, int maximum) {
    drawQrFixture(x, y, maximum, 33);
}
void drawSetupWifiQrBadge(int x, int y, int maximum) {
    drawQrFixture(x, y, maximum, 37);
}
UiRenderState homeFocused(uint8_t index) {
    UiRenderState state;
    state.homeFocus = index;
    return state;
}
bool isAlphaNumeric(char c) { return std::isalnum(static_cast<unsigned char>(c)); }
bool isAP = false, alarmActive = true;
uint16_t autoDimSeconds = 30;
constexpr uint16_t AUTO_DIM_NEVER_SECONDS = 0;
constexpr const char* kRadioMdnsAddress = "radio.local";
constexpr const char* kSetupAccessPointSsid = "Radio_Setup";
// The production layout now reads firmware-update status on Home. Keep that
// unrelated state inert in this visual fixture without pulling OTA networking
// or persistence into the host test binary.
class FirmwareUpdater {
public:
    enum class Status : uint8_t {
        Idle,
        Checking,
        UpToDate,
        UpdateFound,
        Downloading,
        Installed,
        Failed,
    };
    struct Snapshot {
        Status status = Status::Idle;
        String message;
        String availableVersion;
        String notes;
        bool busy = false;
        bool autoInstall = false;
        bool awaitingConfirmation = false;
    };
    Snapshot snapshot() const { return {}; }
};
FirmwareUpdater firmwareUpdater;
int mainVal = 12, alarmH = 7, alarmM = 30;
String songTitle = "פרק 15 - 15 בספטמבר 2025";
bool isStationMuted() { return false; }
bool stationFavorites[] = {true, false, false};
bool isStationFavorite(int slot) { return slot >= 0 && slot < 3 && stationFavorites[slot]; }
int playableStationCount() { return 3; }
int playableStationSlotAt(int i) { return i >= 0 && i < 3 ? i : -1; }
constexpr int STATION_COUNT = 10;
struct Station { String name; String url; };
Station stations[] = {{"NPR 24", "https://example.test/npr"}, {"גלי צהל", "https://example.test/101"}, {"A very long station title", "https://example.test/long"}};
int currentStationIdx = 0;
bool podcastMode = false, useCelsius = true, weatherDataValid = true;
// Fixture defaults mirror the persisted Home configuration defaults.
bool showWeatherOnHome = true, use24HourClock = true;
unsigned long weatherLastSuccessAt = 0;
constexpr unsigned long WEATHER_STALE_AFTER_MS = 30UL * 60UL * 1000UL;
struct FixtureIpAddress { String toString() const { return "192.168.4.1"; } };
constexpr int WL_CONNECTED = 3;
struct FixtureWiFi {
    int status() const { return WL_CONNECTED; }
    int32_t RSSI() const { return -58; }
    String SSID() const { return "Studio WiFi"; }
    String softAPSSID() const { return "Radio_Setup"; }
    FixtureIpAddress softAPIP() const { return {}; }
    FixtureIpAddress localIP() const { return {}; }
} WiFi;
String st_ssid = "Studio WiFi";
int savedWiFiAlternativeCount() { return 0; }
int savedWiFiAlternativeNetworkAt(int) { return -1; }
String savedWiFiNetworkSsid(int) { return {}; }
int activeSavedWiFiNetworkIndex() { return 0; }
bool configuredLocalTime(time_t utcTime, tm& localTime) {
    return localtime_r(&utcTime, &localTime) != nullptr;
}
String podcastShowTft, owmCity = "Tel Aviv, IL";
constexpr int PODCAST_SHOW_COUNT = 10;
constexpr int MAX_EPISODES = 8;
struct FixturePodcastShow { const char* webName; const char* tftName; };
FixturePodcastShow podcastShows[PODCAST_SHOW_COUNT] = {
    {"אילנה דיין", "Ilana Dayan"}, {"יהיה בסדר", "Yihye Beseder"},
    {"ארבע אחרי הצהריים", "Arba"}, {"חמש בערב", "Hamesh"},
    {"רינו צרור", "Rino"}, {"בוקר טוב ישראל", "Boker"},
    {"יומן הצהריים", "Yoman"}, {"גל עברי ירוק", "Gal"},
    {"לילה ישראלי", "Laila"}, {"גילוי דעת", "Giluy"},
};
struct PodcastEpisode { String title; String publishedUtc; uint32_t durationSeconds; };
PodcastEpisode podcastEpisodes[MAX_EPISODES] = {
    {"פרק 15 - 15 בספטמבר 2025", "2025-09-15", 1697},
    {"A very long mixed Hebrew/Latin episode title 101 FM", "2025-09-08", 1925},
};
int podcastEpisodeCount = 2;
bool isPodcastShowFavorite(int show) { return show == 0; }
PodcastLoadState fixturePodcastLoad = PodcastLoadState::Ready;
int podcastRequestedShow() { return 0; }
PodcastLoadState podcastLoadState() { return fixturePodcastLoad; }
bool podcastEpisodesReadyFor(int show) { return show == 0; }
PodcastPlaybackSnapshot podcastPlaybackSnapshot() {
    PodcastPlaybackSnapshot snapshot;
    snapshot.active = true; snapshot.showIndex = 0; snapshot.episodeIndex = 0;
    snapshot.elapsedSeconds = 742; snapshot.durationSeconds = 1697;
    snapshot.canPause = true; snapshot.canSeek = true;
    return snapshot;
}
const PodcastEpisode* podcastActiveEpisode() { return &podcastEpisodes[0]; }
float tempC = 30;
int weatherID = 801, weatherStateMux = 0;
// The production renderer may use persisted station artwork.  Fixtures use the
// existing generated fallback art, so provide the settings seam without pulling
// persistence into the native visual test binary.
uint32_t stationArtworkContentRevision(int) { return 0; }
bool loadStationArtwork(int, int, uint16_t*, size_t) { return false; }
#define portENTER_CRITICAL(mux) ((void)0)
#define portEXIT_CRITICAL(mux) ((void)0)
time_t fixtureTime(time_t*) { return 1789992000; }  // Mon, 21 Sep 2026 UTC
#define time fixtureTime
#include "home_layout.inc"
#undef time

void save(const char* path) {
    FILE* file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n320 240\n255\n");
    uint8_t row[320 * 3];
    for (int y = 0; y < 240; ++y) {
        frame.readRectRGB(0, y, 320, 1, row);
        fwrite(row, 1, sizeof(row), file);
    }
    fclose(file);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    void* pixels = malloc(320 * 240 * 2);
    assert(pixels);
    frame.setBuffer(pixels, 320, 240, 16);
    assert(frame.isReadable());
    assert(display_fonts::init());
    const auto visibleLeft = [](const char* value, int16_t x, int16_t y,
                                const lgfx::IFont* font, lgfx::textdatum_t datum) {
        frame.fillScreen(TFT_BLACK);
        frame.setTextColor(TFT_WHITE);
        frame.setTextDatum(datum);
        frame.drawString(value, x, y, font);
        for (int16_t pixelX = 0; pixelX < 320; ++pixelX) {
            for (int16_t pixelY = 0; pixelY < 60; ++pixelY) {
                if (frame.readPixel(pixelX, pixelY) != TFT_BLACK) return pixelX;
            }
        }
        return static_cast<int16_t>(-1);
    };
    // The physical TFT photo needs one more pixel of inset than equal native
    // glyph bounds suggest. Keep that panel-calibrated correction explicit.
    assert(visibleLeft("R", kHomeStationTitleLeft, kHomeStationTitleTop,
                       uiFont(&fonts::Font0), TL_DATUM) ==
           visibleLeft("R", 38, 22, display_fonts::homeTitle(), ML_DATUM) + 1);
    const UiTextLayout mixed = uiTextLayout("פרק 15 - 15 בספטמבר 2025");
    assert(mixed.rightToLeft);
    assert(mixed.visual == "2025 רבמטפסב 15 - 15 קרפ");
    const UiTextLayout mixedLatin = uiTextLayout("GALATZ 99");
    assert(!mixedLatin.rightToLeft && mixedLatin.visual == "GALATZ 99");
    assert(homeStationTitleVisual("NPR 24", "Live Radio") == "NPR 24 • Live Radio");
    assert(homeStationTitleVisual("גלי צהל", "Live Radio") ==
           "להצ ילג • Live Radio");
    assert(homeStationTitleVisual("תחנה 101 FM", "Live Radio") ==
           "101 FM הנחת • Live Radio");
    assert(homeStationTitleVisual("", "Live Radio") == "Live Radio");
    // Fit the actual Home labels into their 72 px tiles with 3 px side insets.
    for (const char* label : {"Live Radio", "Recorded", "Shows", "Favorites", "Settings"}) {
        assert(frame.textWidth(label, display_fonts::homeLabel()) <= 66);
    }
    assert(frame.textWidth("Current weather", display_fonts::caption()) <= 102);
    assert(kHomeStationTitleLeft == 40);
    assert(kHomeStationTitleRight == 180);
    assert(kHomeStationTitleLeft + kHomeStationTitleWidth == kHomeStationTitleRight);
    assert(ui_home_clock_glyph_count == 12);
    assert(ui_home_clock_cell_width == 36 && ui_home_clock_cell_height == 42);
    assert(ui_home_clock_baseline_y == 36);
    assert(ui_home_clock_advance_scale == 10000 && ui_home_clock_tracking_units == 2500);
    assert(ui_home_clock_colon_side_spacing_units == 10000);
    const int32_t unitsBeforeColon =
        ui_home_clock_advance_units[1] + ui_home_clock_tracking_units +
        ui_home_clock_advance_units[4] + ui_home_clock_tracking_units;
    const auto roundedClockPixel = [](int32_t units) {
        return (units + ui_home_clock_advance_scale / 2) / ui_home_clock_advance_scale;
    };
    assert(roundedClockPixel(unitsBeforeColon + ui_home_clock_colon_side_spacing_units) -
           roundedClockPixel(unitsBeforeColon) == 1);
    assert(roundedClockPixel(unitsBeforeColon + ui_home_clock_colon_side_spacing_units +
                             ui_home_clock_advance_units[10] + ui_home_clock_tracking_units +
                             ui_home_clock_colon_side_spacing_units) -
           roundedClockPixel(unitsBeforeColon + ui_home_clock_advance_units[10] +
                             ui_home_clock_tracking_units) == 2);
    assert(ui_home_clock_ink_right == 301 && ui_home_clock_ink_top == 40);
    assert(ui_home_temperature_ink_left == kHomeWeatherTextLeft &&
           ui_home_temperature_ink_top == 70);
    assert(kHomeWeatherTemperatureTop == ui_home_temperature_ink_top + 7);
    assert(kHomeWeatherCityTop == 104 && kHomeWeatherConditionTop == 120);
    // The reference uses a substantial degree ring aligned with the numeral
    // cap height, not a small superscript tucked against the final digit.
    const uint8_t* temperatureEight = ui_home_temperature_alpha +
        homeNumeralGlyphIndex('8') * ui_home_temperature_cell_width *
        ui_home_temperature_cell_height;
    const uint8_t* temperatureDegree = ui_home_temperature_alpha +
        homeNumeralGlyphIndex('*') * ui_home_temperature_cell_width *
        ui_home_temperature_cell_height;
    int eightRight = 0;
    int eightTop = ui_home_temperature_cell_height;
    int degreeLeft = ui_home_temperature_cell_width;
    int degreeRight = 0;
    int degreeTop = ui_home_temperature_cell_height;
    int degreeBottom = 0;
    for (int y = 0; y < ui_home_temperature_cell_height; ++y) {
        for (int x = 0; x < ui_home_temperature_cell_width; ++x) {
            if (temperatureEight[y * ui_home_temperature_cell_width + x] != 0) {
                eightRight = std::max(eightRight, x + 1);
                eightTop = std::min(eightTop, y);
            }
            if (temperatureDegree[y * ui_home_temperature_cell_width + x] != 0) {
                degreeLeft = std::min(degreeLeft, x);
                degreeRight = std::max(degreeRight, x + 1);
                degreeTop = std::min(degreeTop, y);
                degreeBottom = std::max(degreeBottom, y + 1);
            }
        }
    }
    const int eightStart = (ui_home_temperature_advance_units[2] +
        ui_home_temperature_advance_scale / 2) / ui_home_temperature_advance_scale;
    const int degreeStart = (ui_home_temperature_advance_units[2] +
        ui_home_temperature_advance_units[8] + ui_home_temperature_advance_scale / 2) /
        ui_home_temperature_advance_scale;
    assert(degreeRight - degreeLeft == 11 && degreeBottom - degreeTop == 12);
    assert(degreeTop == eightTop);
    assert(degreeStart + degreeLeft - (eightStart + eightRight) == 3);
    // Home icon PNGs have unequal transparent padding, so the renderer aligns
    // their visible ink rather than their canvas origins.
    assert(kHomeTileIconVisibleTop == kHomeTileY + 8);
    assert(kHomeTileIconTransparentTop[0] == 0);
    assert(kHomeTileIconTransparentTop[1] == 4);
    assert(kHomeTileIconTransparentTop[2] == 4);
    assert(kHomeTileIconTransparentTop[3] == 1);
    assert(kHomeTileIconFallbackYOffset[0] == -3);
    assert(kHomeTileIconFallbackYOffset[1] == -4);
    assert(kHomeTileIconFallbackYOffset[2] == 0);
    assert(kHomeTileIconFallbackYOffset[3] == 1);
    assert(kPageHeaderCenterY == 22);
    int fractionalClockPixels = 0;
    for (size_t index = 0; index < sizeof(ui_home_clock_alpha); ++index) {
        fractionalClockPixels += ui_home_clock_alpha[index] != 0 && ui_home_clock_alpha[index] != 255;
    }
    assert(fractionalClockPixels > 100);  // 8-bit Lanczos alpha, not bitmap text.
    // Exercise the production renderer directly: it must retain the generated
    // fractional advances/tracking, compose alpha, then blend once in RGB565.
    constexpr int kClockTestWidth = 160;
    auto assertClock = [&](const char* value) {
        uint8_t expectedClockAlpha[kClockTestWidth * ui_home_clock_cell_height] = {};
        int32_t clockPenUnits = 0;
        int composedWidth = 0;
        for (const char* character = value; *character; ++character) {
            const int glyph = homeNumeralGlyphIndex(*character);
            if (*character == ':') clockPenUnits += ui_home_clock_colon_side_spacing_units;
            const int glyphX = (clockPenUnits + ui_home_clock_advance_scale / 2) /
                ui_home_clock_advance_scale;
            composedWidth = std::max(composedWidth, glyphX + ui_home_clock_cell_width);
            clockPenUnits += ui_home_clock_advance_units[glyph] + ui_home_clock_tracking_units;
            if (*character == ':') clockPenUnits += ui_home_clock_colon_side_spacing_units;
        }
        assert(composedWidth <= kClockTestWidth);
        clockPenUnits = 0;
        for (const char* character = value; *character; ++character) {
            const int glyph = homeNumeralGlyphIndex(*character);
            if (*character == ':') clockPenUnits += ui_home_clock_colon_side_spacing_units;
            const int glyphX = (clockPenUnits + ui_home_clock_advance_scale / 2) /
                ui_home_clock_advance_scale;
            const uint8_t* glyphAlpha = ui_home_clock_alpha + glyph * ui_home_clock_cell_width * ui_home_clock_cell_height;
            for (int y = 0; y < ui_home_clock_cell_height; ++y) {
                for (int x = 0; x < ui_home_clock_cell_width && glyphX + x < composedWidth; ++x) {
                    const uint8_t source = glyphAlpha[y * ui_home_clock_cell_width + x];
                    uint8_t& destination = expectedClockAlpha[y * kClockTestWidth + glyphX + x];
                    destination = static_cast<uint8_t>(source +
                        (static_cast<uint16_t>(destination) * (255U - source) + 127U) / 255U);
                }
            }
            clockPenUnits += ui_home_clock_advance_units[glyph] + ui_home_clock_tracking_units;
            if (*character == ':') clockPenUnits += ui_home_clock_colon_side_spacing_units;
        }
        int inkRight = 0;
        int inkTop = ui_home_clock_cell_height;
        for (int y = 0; y < ui_home_clock_cell_height; ++y) {
            for (int x = 0; x < composedWidth; ++x) {
                if (expectedClockAlpha[y * kClockTestWidth + x] == 0) continue;
                inkRight = std::max(inkRight, x + 1);
                inkTop = std::min(inkTop, y);
            }
        }
        frame.fillScreen(0x0000);
        drawHomeClockAtlas(value);
        for (int y = 0; y < ui_home_clock_cell_height; ++y) {
            for (int x = 0; x < composedWidth; ++x) {
                const uint8_t alpha = expectedClockAlpha[y * kClockTestWidth + x];
                assert(frame.readPixel(ui_home_clock_ink_right - inkRight + x,
                                       ui_home_clock_ink_top - inkTop + y) == blendHomeNumeralPixel(0x0000, alpha, kClockText));
            }
        }
    };
    assertClock("14:37");
    assertClock("00:00");
    assertClock("--:--");
    assertClock("18:32");
    assertClock("9:05");  // One-digit hour in 12-hour mode.
    // Check the actual pixels of the new left-anchored temperature role and
    // its fallback, including signs and three-digit Fahrenheit readings.
    for (bool smooth : {true, false}) {
        antialias = smooth;
        for (const char* value : {"28*", "-12*", "104*", "0*"}) {
            frame.fillScreen(0);
            drawHomeTemperatureAtlas(value);
            int left = 320, right = 0, top = 240, bottom = 0;
            for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
                if (!frame.readPixel(x, y)) continue;
                left = std::min(left, x);
                right = std::max(right, x + 1);
                top = std::min(top, y);
                bottom = std::max(bottom, y + 1);
            }
            assert(left >= 82 && left <= 83 && right <= 166);
            assert(top >= 70 && top <= 71 && bottom >= 93 && bottom <= 94); // Top-aligned degree / enlarged digits.
        }
    }
    antialias = true;
    // Keep the temperature-to-city rhythm while pulling only the condition
    // line two pixels toward the fixed location line.
    frame.fillScreen(0);
    drawHomeTemperatureAtlas("30*", kHomeWeatherTemperatureTop);
    text("Tel Aviv", kHomeWeatherTextLeft, kHomeWeatherCityTop,
         homeCaptionFont(), kWhite, 106);
    text("Partly cloudy", kHomeWeatherTextLeft, kHomeWeatherConditionTop,
         homeCaptionFont(), kWhite, 106);
    auto rowHasInk = [&](int y) {
        for (int x = kHomeWeatherTextLeft; x < 196; ++x) {
            if (frame.readPixel(x, y) != 0) return true;
        }
        return false;
    };
    int temperatureBottom = 0;
    int cityTop = 240;
    int cityBottom = 0;
    int conditionTop = 240;
    for (int y = kHomeWeatherTop; y < kHomeWeatherCityTop; ++y) {
        if (rowHasInk(y)) temperatureBottom = y + 1;
    }
    for (int y = kHomeWeatherCityTop; y < kHomeWeatherConditionTop; ++y) {
        if (!rowHasInk(y)) continue;
        cityTop = std::min(cityTop, y);
        cityBottom = y + 1;
    }
    for (int y = kHomeWeatherConditionTop; y < 140; ++y) {
        if (rowHasInk(y)) {
            conditionTop = y;
            break;
        }
    }
    const int temperatureCityGap = cityTop - temperatureBottom;
    const int cityConditionGap = conditionTop - cityBottom;
    assert(temperatureCityGap == 7);
    assert(cityConditionGap == 5);
    const auto glyphVerticalCenterTwice = [](int glyph) {
        const uint8_t* alpha = ui_home_clock_alpha + glyph * ui_home_clock_cell_width * ui_home_clock_cell_height;
        int top = ui_home_clock_cell_height;
        int bottom = 0;
        for (int y = 0; y < ui_home_clock_cell_height; ++y) {
            for (int x = 0; x < ui_home_clock_cell_width; ++x) {
                if (alpha[y * ui_home_clock_cell_width + x] == 0) continue;
                top = std::min(top, y);
                bottom = std::max(bottom, y + 1);
            }
        }
        return top + bottom;
    };
    int centerMin = 2 * ui_home_clock_cell_height;
    int centerMax = 0;
    for (char digit = '0'; digit <= '9'; ++digit) {
        const int center = glyphVerticalCenterTwice(homeNumeralGlyphIndex(digit));
        centerMin = std::min(centerMin, center);
        centerMax = std::max(centerMax, center);
    }
    // The colon is optically balanced one pixel below the numeral ink center.
    assert(glyphVerticalCenterTwice(homeNumeralGlyphIndex(':')) ==
           glyphVerticalCenterTwice(homeNumeralGlyphIndex('0')) + 2);
    // The source digits have different tight crop heights. Their visual
    // centers must nevertheless agree to within half a native pixel.
    assert(centerMax - centerMin <= 1);
    assert(frame.textWidth("Wed, 30 Sep", display_fonts::caption()) <= 80);
    lgfx::FontMetrics metrics;
    display_fonts::label()->getDefaultMetric(&metrics);
    assert(display_fonts::label()->updateFontMetric(&metrics, ' '));
    assert(metrics.x_advance == 2); // source's space, not the VLW line-height guess
    display_fonts::homeLabel()->getDefaultMetric(&metrics);
    assert(display_fonts::homeLabel()->updateFontMetric(&metrics, ' '));
    assert(metrics.x_advance == 3);
    for (uint16_t code = 0x5D0; code <= 0x5EA; ++code) {
        assert(display_fonts::caption()->updateFontMetric(&metrics, code));
    }
    // Geometry and input use the production hit-test and controller state types.
    assert(uiHitTest({}, 250, 50) == UiTarget::None);
    assert(uiHitTest({}, 250, 140) == UiTarget::None);
    assert(uiHitTest({}, 81, 200) == UiTarget::None);
    for (int i = 0; i < 4; ++i) {
        const UiTarget target = static_cast<UiTarget>(static_cast<int>(UiTarget::HomeLiveRadio) + i);
        assert(uiHitTest({}, kHomeTileX[i] + kHomeTileWidth / 2,
                         kHomeTileY + kHomeTileHeight / 2) == target);
        assert(uiHitTest({}, kHomeTileX[i], kHomeTileY) == target);
        assert(uiHitTest({}, kHomeTileX[i] + kHomeTileWidth - 1, kHomeTileY + kHomeTileHeight - 1) == target);
    }
    assert(uiHitTest({}, 80, kHomeTileY + kHomeTileHeight / 2) == UiTarget::None);
    assert(uiHitTest({}, 160, kHomeTileY + kHomeTileHeight / 2) == UiTarget::None);
    assert(uiHitTest({}, 240, kHomeTileY + kHomeTileHeight / 2) == UiTarget::None);
    assert(uiHitTest({}, 160, kHomeTileY - 1) == UiTarget::None);
    assert(uiHitTest({}, 160, kHomeTileY + kHomeTileHeight) == UiTarget::None);
    assert(frame.textWidth("GALATZ", uiFont(&fonts::FreeSansBold12pt7b)) <= 91);
    UiRenderState playerHitState;
    playerHitState.page = UiPage::Listening;
    assert(uiHitTest(playerHitState, 22, 22) == UiTarget::PlayerBack);
    assert(uiHitTest(playerHitState, 71, 47) == UiTarget::PlayerBack);
    assert(uiHitTest(playerHitState, 76, 164) == UiTarget::PlayerPrevious);
    assert(uiHitTest(playerHitState, 160, 164) == UiTarget::PlayerStopOrPlay);
    assert(uiHitTest(playerHitState, 244, 164) == UiTarget::PlayerNext);
    assert(uiHitTest(playerHitState, 156, 218) == UiTarget::ListeningVolume);
    UiRenderState stationHitState;
    stationHitState.page = UiPage::Stations;
    assert(uiHitTest(stationHitState, 71, 47) == UiTarget::ListBack);
    assert(uiHitTest(stationHitState, 150, 68) == UiTarget::ListRow0);
    assert(uiHitTest(stationHitState, 232, 68) == UiTarget::ListRowFavorite0);
    assert(uiHitTest(stationHitState, 150, 212) == UiTarget::ListRow3);
    assert(uiHitTest(stationHitState, 232, 212) == UiTarget::ListRowFavorite3);
    assert(uiHitTest(stationHitState, 289, 70) == UiTarget::ListPrevious);
    assert(uiHitTest(stationHitState, 289, 160) == UiTarget::ListNext);
    assert(uiHitTest(stationHitState, 160, 214) == UiTarget::ListRow3);
    UiRenderState stationOptionsHitState;
    stationOptionsHitState.page = UiPage::StationOptions;
    assert(uiHitTest(stationOptionsHitState, 71, 47) == UiTarget::OptionsBack);
    UiRenderState stationInfoHitState;
    stationInfoHitState.page = UiPage::StationInfo;
    assert(uiHitTest(stationInfoHitState, 71, 47) == UiTarget::InfoBack);
    UiRenderState favoritesHitState;
    favoritesHitState.page = UiPage::Favorites;
    assert(uiHitTest(favoritesHitState, 71, 47) == UiTarget::FavoritesBack);
    assert(uiHitTest(favoritesHitState, 82, 65) == UiTarget::FavoritesStationsTab);
    assert(uiHitTest(favoritesHitState, 238, 65) == UiTarget::FavoritesShowsTab);
    assert(uiHitTest(favoritesHitState, 150, 108) == UiTarget::FavoritesRow0);
    assert(uiHitTest(favoritesHitState, 232, 108) == UiTarget::FavoritesRowFavorite0);
    assert(uiHitTest(favoritesHitState, 150, 204) == UiTarget::FavoritesRow2);
    assert(uiHitTest(favoritesHitState, 232, 204) == UiTarget::FavoritesRowFavorite2);
    assert(uiHitTest(favoritesHitState, kListPagerLeft + 25, kFavoriteListTop) == UiTarget::FavoritesPrevious);
    assert(uiHitTest(favoritesHitState, kListPagerLeft + 25, kFavoriteListBottom - 1) == UiTarget::FavoritesNext);
    UiRenderState showsHitState;
    showsHitState.page = UiPage::RecordedShows;
    assert(uiHitTest(showsHitState, 22, 22) == UiTarget::ShowsBack);
    assert(uiHitTest(showsHitState, 71, 47) == UiTarget::ShowsBack);
    assert(uiHitTest(showsHitState, 150, 68) == UiTarget::ShowRow0);
    assert(uiHitTest(showsHitState, 232, 68) == UiTarget::ShowRowFavorite0);
    UiRenderState episodesHitState;
    episodesHitState.page = UiPage::ShowEpisodes;
    episodesHitState.episodeShow = 0;
    assert(uiHitTest(episodesHitState, 71, 47) == UiTarget::EpisodesBack);
    assert(uiHitTest(episodesHitState, 150, 68) == UiTarget::EpisodeRow0);
    for (const auto& page : {stationHitState, showsHitState, episodesHitState}) {
        assert(uiHitTest(page, kListPagerLeft + 25, kStationListTop) == UiTarget::ListPrevious);
        assert(uiHitTest(page, kListPagerLeft + 25, kStationListTop + kListRailHeight / 2 - 1) == UiTarget::ListPrevious);
        assert(uiHitTest(page, kListPagerLeft + 25, kStationListTop + kListRailHeight / 2) == UiTarget::ListNext);
        assert(uiHitTest(page, kListPagerLeft + 25, kListRailBottom - 1) == UiTarget::ListNext);
        assert(uiHitTest(page, kListPagerLeft + 25, kListRailBottom) == UiTarget::None);
        assert(uiHitTest(page, 160, 214) == UiTarget::ListRow3 ||
               uiHitTest(page, 160, 214) == UiTarget::ShowRow3 ||
               uiHitTest(page, 160, 214) == UiTarget::EpisodeRow3);
    }
    UiRenderState podcastHitState;
    podcastHitState.page = UiPage::PodcastPlayer;
    assert(uiHitTest(podcastHitState, 71, 47) == UiTarget::PodcastBack);
    assert(uiHitTest(podcastHitState, 150, 128) == UiTarget::PodcastProgress);
    assert(uiHitTest(podcastHitState, 294, 137) == UiTarget::PodcastProgress);
    assert(uiHitTest(podcastHitState, 310, 70) == UiTarget::None);
    assert(uiHitTest(podcastHitState, 82, 194) == UiTarget::PodcastSeekBack);
    assert(uiHitTest(podcastHitState, 160, 194) == UiTarget::PodcastPause);
    assert(uiHitTest(podcastHitState, 238, 194) == UiTarget::PodcastSeekForward);
    UiRenderState settingsHitState;
    settingsHitState.page = UiPage::Settings;
    assert(uiHitTest(settingsHitState, 22, 22) == UiTarget::SettingsBack);
    assert(uiHitTest(settingsHitState, 160, 72) == UiTarget::SettingsRow0);
    assert(uiHitTest(settingsHitState, 160, 120) == UiTarget::SettingsRow1);
    assert(uiHitTest(settingsHitState, 160, 168) == UiTarget::SettingsRow2);
    assert(uiHitTest(settingsHitState, 160, 216) == UiTarget::SettingsRow3);
    assert(uiHitTest(settingsHitState, 289, 160) == UiTarget::SettingsNext);
    UiRenderState toneHitState;
    toneHitState.page = UiPage::SettingsAudio;
    assert(uiHitTest(toneHitState, 22, 22) == UiTarget::SettingsBack);
    assert(uiHitTest(toneHitState, 226, 72) == UiTarget::ToneBassDecrease);
    assert(uiHitTest(toneHitState, 282, 118) == UiTarget::ToneMidIncrease);
    assert(uiHitTest(toneHitState, 236, 210) == UiTarget::ToneSave);
    assert(uiHitTest(toneHitState, 203, 72) == UiTarget::None);
    assert(uiHitTest(toneHitState, 204, 44) == UiTarget::ToneBassDecrease);
    assert(uiHitTest(toneHitState, 247, 89) == UiTarget::ToneBassDecrease);
    assert(uiHitTest(toneHitState, 248, 72) == UiTarget::None);
    assert(uiHitTest(toneHitState, 259, 72) == UiTarget::None);
    assert(uiHitTest(toneHitState, 260, 140) == UiTarget::ToneTrebleIncrease);
    assert(uiHitTest(toneHitState, 303, 185) == UiTarget::ToneTrebleIncrease);
    assert(uiHitTest(toneHitState, 304, 162) == UiTarget::None);
    UiRenderState deviceHitState;
    deviceHitState.page = UiPage::SettingsDevice;
    assert(uiHitTest(deviceHitState, 22, 22) == UiTarget::SettingsBack);
    assert(uiHitTest(deviceHitState, 160, 68) == UiTarget::DeviceFirmware);
    assert(uiHitTest(deviceHitState, 160, 116) == UiTarget::DeviceCalibration);
    assert(uiHitTest(deviceHitState, 160, 203) == UiTarget::DeviceFactoryReset);
    UiRenderState firmwareHitState;
    firmwareHitState.page = UiPage::SettingsFirmware;
    assert(uiHitTest(firmwareHitState, 22, 22) == UiTarget::SettingsBack);
    assert(uiHitTest(firmwareHitState, 160, 68) == UiTarget::FirmwareCheckNow);
    assert(uiHitTest(firmwareHitState, 160, 124) == UiTarget::FirmwareToggleAutoInstall);
    UiRenderState settingsHandoffHitState;
    settingsHandoffHitState.page = UiPage::SettingsWebHandoff;
    assert(uiHitTest(settingsHandoffHitState, 22, 22) == UiTarget::SettingsBack);
    assert(homeCityLabel("Tel Aviv, ISRAEL") == "Tel Aviv");
    assert(homeCityLabel("  Haifa  ") == "Haifa");
    assert(homeCityLabel("") == "Weather");
    assert(std::string(homeWeatherDescription(801)) == "Partly cloudy");
    assert(std::string(homeWeatherDescription(800)) == "Clear sky");
    assert(std::string(homeWeatherDescription(500)) == "Rain");
    assert(std::string(homeWeatherDescription(211)) == "Thunderstorms");
    assert(std::string(homeWeatherDescription(601)) == "Snow");
    assert(std::string(homeWeatherDescription(741)) == "Fog");
    assert(std::string(homeWeatherDescription(999)) == "Unknown");
    const std::string dir = argv[1];
    renderHome({}, "15:01", true);
    save((dir + "/home-smooth.ppm").c_str());
    currentStationIdx = 1;
    renderHome({}, "15:01", true);
    assert(homeStationTitleMarquee.stationVisual == "להצ ילג");
    assert(homeStationTitleMarquee.sourceSuffix == " • Live Radio");
    assert(homeStationTitleMarquee.stationWidth > 0);
    save((dir + "/home-hebrew-station.ppm").c_str());
    currentStationIdx = 2;
    renderHome({}, "12:05", true);
    assert(homeStationTitleMarquee.overflows);
    save((dir + "/home-long-station.ppm").c_str());
    currentStationIdx = 0;
    renderHome({}, "15:56", true);
    save((dir + "/home-clock-1556.ppm").c_str());
    renderHome({}, "09:58", true);
    save((dir + "/home-clock-0958.ppm").c_str());
    renderHome({}, "14:37", true);
    save((dir + "/home-clock-1437.ppm").c_str());
    assert(audioServiceCalls >= 5); // after background and between all four tiles
    // Exercise all current condition families, not only the partly-cloudy sample.
    for (int id : {800, 804, 500, 211, 601, 741, 999}) {
        weatherID = id;
        renderHome({}, "15:01", true);
        save((dir + "/home-weather-" + std::to_string(id) + ".ppm").c_str());
    }
    weatherID = 801;
    // Same font assets and production pixel path on photo and solid surfaces.
    // Ensure anti-aliasing actually produces intermediate pixels in RGB565.
    frame.fillScreen(0x0000);
    frame.setTextColor(0xFFFF);
    frame.setTextDatum(TL_DATUM);
    frame.drawString("GALATZ", 10, 10, uiFont(&fonts::FreeSans9pt7b));
    int intermediate = 0;
    for (int y = 0; y < 40; ++y) for (int x = 0; x < 100; ++x) {
        const auto pixel = frame.readPixel(x, y);
        intermediate += pixel != 0 && pixel != 0xFFFF;
    }
    assert(intermediate > 30);
    // Typed sprite transfer must preserve color channels / byte ordering.
    lgfx::LGFX_Sprite target;
    target.setColorDepth(16);
    assert(target.createSprite(320, 240));
    frame.fillRect(0, 0, 10, 10, 0xF800);
    frame.fillRect(10, 0, 10, 10, 0x07E0);
    frame.fillRect(20, 0, 10, 10, 0x001F);
    target.pushImage(0, 0, 320, 240, static_cast<const lgfx::swap565_t*>(frame.getBuffer()));
    assert(target.readPixel(1, 1) == 0xF800);
    assert(target.readPixel(11, 1) == 0x07E0);
    assert(target.readPixel(21, 1) == 0x001F);
    antialias = false;
    renderHome({}, "15:01", true);
    save((dir + "/home-bitmap.ppm").c_str());
    antialias = true;
    tempC = -12;
    owmCity = "A very long city name";
    renderHome(homeFocused(2), "23:59", true);
    save((dir + "/home-long.ppm").c_str());
    useCelsius = false;
    tempC = 40;
    renderHome({}, "15:01", true);
    save((dir + "/home-fahrenheit.ppm").c_str());
    useCelsius = true;
    weatherDataValid = false;
    renderHome(homeFocused(3), "", false);
    save((dir + "/home-unavailable.ppm").c_str());
    renderStations({}, "09:59", true);
    renderStations({}, "10:00", true);
    save((dir + "/header-updated.ppm").c_str());
    renderStations({}, "10:00", true);
    save((dir + "/header-fresh.ppm").c_str());
    // The accepted direction is a slim, rounded, anti-aliased stroke—not the
    // chunky filled polygon from the first Live Radio preview.
    frame.fillScreen(TFT_BLACK);
    drawPageBackChevron();
    int chevronIntermediatePixels = 0;
    int chevronCenterPixels = 0;
    for (int y = 8; y < 36; ++y) {
        for (int x = 5; x < 30; ++x) {
            const uint16_t pixel = frame.readPixel(x, y);
            chevronIntermediatePixels += pixel != TFT_BLACK && pixel != kWhite;
            if (y == kPageHeaderCenterY && pixel != TFT_BLACK) ++chevronCenterPixels;
        }
    }
    assert(chevronIntermediatePixels >= 12);
    assert(chevronCenterPixels >= 2 && chevronCenterPixels <= 5);
    renderStations({}, "15:01", true);
    save((dir + "/stations-smooth.ppm").c_str());
    renderListening({}, "15:01", true);
    save((dir + "/player-smooth.ppm").c_str());
    UiRenderState options;
    options.page = UiPage::StationOptions;
    options.optionStation = 0;
    renderStationOptions(options, "15:01", true);
    save((dir + "/station-options.ppm").c_str());
    renderStationInfo(options, "15:01", true);
    save((dir + "/station-info.ppm").c_str());
    renderFavorites(favoritesHitState, "15:01", true);
    save((dir + "/favorites.ppm").c_str());
    renderRecordedShows(showsHitState, "15:01", true);
    save((dir + "/recorded-shows.ppm").c_str());
    UiRenderState showsPageTwo = showsHitState;
    showsPageTwo.showOffset = 4;
    // Paging only changes the viewport; it does not select a row on that page.
    assert(showsPageTwo.showFocus == 0);
    renderRecordedShows(showsPageTwo, "15:01", true);
    save((dir + "/recorded-shows-page-two.ppm").c_str());
    UiRenderState showsPageThree = showsHitState;
    showsPageThree.showOffset = 6; // Partial final viewport: shows 7–10.
    renderRecordedShows(showsPageThree, "15:01", true);
    save((dir + "/recorded-shows-page-three.ppm").c_str());
    renderShowEpisodes(episodesHitState, "15:01", true);
    save((dir + "/show-episodes.ppm").c_str());
    renderPodcastPlayer(podcastHitState, "15:01", true);
    save((dir + "/podcast-player.ppm").c_str());
    UiRenderState overlay;
    overlay.volumeOverlay = true;
    renderListening(overlay, "15:01", true);
    save((dir + "/volume-smooth.ppm").c_str());
    renderConfirm({}, "15:01", true);
    save((dir + "/confirm-smooth.ppm").c_str());
    renderSettings(settingsHitState, "15:01", true);
    save((dir + "/settings-smooth.ppm").c_str());
    toneHitState.toneBassDraft = -15;
    toneHitState.toneMidDraft = 0;
    toneHitState.toneTrebleDraft = 15;
    renderToneSettings(toneHitState, "15:01", true);
    save((dir + "/settings-audio-smooth.ppm").c_str());
    UiRenderState displaySettings;
    displaySettings.page = UiPage::SettingsDisplay;
    assert(uiHitTest(displaySettings, 22, 22) == UiTarget::SettingsBack);
    displaySettings.dimSecondsDraft = 60;
    renderDisplaySettings(displaySettings, "15:01", true);
    save((dir + "/settings-display-smooth.ppm").c_str());
    renderDeviceSettings(deviceHitState, "15:01", true);
    save((dir + "/settings-device-smooth.ppm").c_str());
    renderFirmwareSettings(firmwareHitState, "15:01", true);
    save((dir + "/settings-firmware-smooth.ppm").c_str());
    renderSettingsWebHandoff(settingsHandoffHitState, "15:01", true);
    save((dir + "/settings-network-smooth.ppm").c_str());
    UiRenderState restartConfirm;
    restartConfirm.page = UiPage::SettingsConfirm;
    restartConfirm.settingsConfirmAction = 1;
    assert(uiHitTest(restartConfirm, 22, 22) == UiTarget::SettingsBack);
    renderSettingsConfirmation(restartConfirm, "15:01", true);
    save((dir + "/settings-restart-confirm-smooth.ppm").c_str());
    UiRenderState unavailable;
    unavailable.page = UiPage::Unavailable;
    unavailable.unavailableDestination = 4;
    renderUnavailable(unavailable, "15:01", true);
    save((dir + "/unavailable-smooth.ppm").c_str());
    drawNetworkBootScreen(true, "192.168.11.199");
    save((dir + "/configuration-boot-smooth.ppm").c_str());
    drawNetworkBootScreen(false, "192.168.4.1");
    save((dir + "/wifi-setup-boot-smooth.ppm").c_str());
    // Full restoration: a second render must exactly match a clean first render.
    weatherDataValid = true;
    tempC = 30;
    owmCity = "Tel Aviv, IL";
    renderHome({}, "15:01", true);
    save((dir + "/home-restored.ppm").c_str());
    printf("Native LGFX checks passed; %d intermediate glyph pixels.\n", intermediate);
}
