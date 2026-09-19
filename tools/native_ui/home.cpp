#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <lgfx/v1/LGFX_Sprite.hpp>
#include "display_fonts.h"
#include "ui_controller.h"
#include "ui_text.h"
#include "ui_background_asset.h"
#include "ui_home_assets.h"

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
UiRenderState homeFocused(uint8_t index) {
    UiRenderState state;
    state.homeFocus = index;
    return state;
}
bool isAlphaNumeric(char c) { return std::isalnum(static_cast<unsigned char>(c)); }
bool isAP = false, alarmActive = true;
int mainVal = 12, alarmH = 7, alarmM = 30;
String songTitle = "פרק 15 - 15 בספטמבר 2025";
bool isStationMuted() { return false; }
int playableStationCount() { return 3; }
int playableStationSlotAt(int i) { return i >= 0 && i < 3 ? i : -1; }
struct Station { String name; };
Station stations[] = {{"GALATZ"}, {"תחנה 101 FM"}, {"A very long station title"}};
int currentStationIdx = 0;
bool podcastMode = false, useCelsius = true, weatherDataValid = true;
String podcastShowTft, owmCity = "Tel Aviv, IL";
float tempC = 30;
int weatherID = 801, weatherStateMux = 0;
#define portENTER_CRITICAL(mux) ((void)0)
#define portEXIT_CRITICAL(mux) ((void)0)
time_t fixtureTime(time_t*) { return 1789819200; }
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
    const UiTextLayout mixed = uiTextLayout("פרק 15 - 15 בספטמבר 2025");
    assert(mixed.rightToLeft);
    assert(mixed.visual == "2025 רבמטפסב 15 - 15 קרפ");
    const UiTextLayout mixedLatin = uiTextLayout("GALATZ 99");
    assert(!mixedLatin.rightToLeft && mixedLatin.visual == "GALATZ 99");
    // Fit the actual Home labels into their 70 px tiles with 3 px side insets.
    for (const char* label : {"Live Radio", "Recorded", "Shows", "Favorites", "Settings"}) {
        assert(frame.textWidth(label, display_fonts::label()) <= 64);
    }
    assert(frame.textWidth("Current weather", display_fonts::caption()) <= 102);
    assert(frame.textWidth("104°", uiFont(&fonts::FreeSansBold18pt7b)) <= 106);
    lgfx::FontMetrics metrics;
    display_fonts::label()->getDefaultMetric(&metrics);
    assert(display_fonts::label()->updateFontMetric(&metrics, ' '));
    assert(metrics.x_advance == 3); // source's space, not the VLW line-height guess
    for (uint16_t code = 0x5D0; code <= 0x5EA; ++code) {
        assert(display_fonts::caption()->updateFontMetric(&metrics, code));
    }
    // Geometry and input use the production hit-test and controller state types.
    assert(uiHitTest({}, 250, 50) == UiTarget::HomeNowPlaying);
    assert(uiHitTest({}, 250, 140) == UiTarget::None);
    assert(uiHitTest({}, 81, 200) == UiTarget::None);
    for (int i = 0; i < 4; ++i) {
        const UiTarget target = static_cast<UiTarget>(static_cast<int>(UiTarget::HomeLiveRadio) + i);
        assert(uiHitTest({}, 8 + i*78 + 35, kHomeTileY + 35) == target);
        assert(uiHitTest({}, 8 + i*78, kHomeTileY) == target);
        assert(uiHitTest({}, 8 + i*78 + 69, kHomeTileY + 69) == target);
    }
    UiRenderState playerHitState;
    playerHitState.page = UiPage::Listening;
    assert(uiHitTest(playerHitState, 22, 22) == UiTarget::PlayerBack);
    assert(uiHitTest(playerHitState, 76, 164) == UiTarget::PlayerPrevious);
    assert(uiHitTest(playerHitState, 160, 164) == UiTarget::PlayerStopOrPlay);
    assert(uiHitTest(playerHitState, 244, 164) == UiTarget::PlayerNext);
    assert(uiHitTest(playerHitState, 156, 218) == UiTarget::ListeningVolume);
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
    drawHeaderClock("10:00", true);
    save((dir + "/header-updated.ppm").c_str());
    renderStations({}, "10:00", true);
    save((dir + "/header-fresh.ppm").c_str());
    renderStations({}, "15:01", true);
    save((dir + "/stations-smooth.ppm").c_str());
    renderListening({}, "15:01", true);
    save((dir + "/player-smooth.ppm").c_str());
    UiRenderState overlay;
    overlay.volumeOverlay = true;
    renderListening(overlay, "15:01", true);
    save((dir + "/volume-smooth.ppm").c_str());
    renderConfirm({}, "15:01", true);
    save((dir + "/confirm-smooth.ppm").c_str());
    // Full restoration: a second render must exactly match a clean first render.
    weatherDataValid = true;
    tempC = 30;
    owmCity = "Tel Aviv, IL";
    renderHome({}, "15:01", true);
    save((dir + "/home-restored.ppm").c_str());
    printf("Native LGFX checks passed; %d intermediate glyph pixels.\n", intermediate);
}
