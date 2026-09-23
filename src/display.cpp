#include "display.h"
#include "settings.h"
#include <esp_heap_caps.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <cmath>
#include <time.h>

#include "app_state.h"
#include "ConfigQrCode.h"
#include "display_fonts.h"
#include "firmware_updater.h"
#include "media.h"
#include "settings.h"
#include "SetupWifiQrCode.h"
#include "ui_controller.h"
#include "ui_text.h"
#include "ui_background_asset.h"
#include "ui_home_assets.h"
#include "ui_header_assets.h"
#include "ui_home_clock_atlas.h"
#include "ui_home_temperature_atlas.h"
#include "ui_podcast_assets.h"
#include "ui_list_assets.h"
#include "ui_player_assets.h"

bool drawPngAsset(
    const uint8_t* pngData,
    size_t pngLength,
    int32_t x,
    int32_t y,
    int32_t maxWidth,
    int32_t maxHeight) {
    if (pngData == nullptr || pngLength == 0 || x >= tft.width() || y >= tft.height()) {
        return false;
    }
    return tft.drawPng(pngData, pngLength, x, y, maxWidth, maxHeight);
}

namespace {

volatile bool weatherFetchInProgress = false;
volatile bool weatherDataValid = false;
uint32_t weatherConfigurationGeneration = 1;
uint32_t weatherFetchGeneration = 0;
bool weatherRefreshRequested = false;
portMUX_TYPE weatherStateMux = portMUX_INITIALIZER_UNLOCKED;
String weatherCitySnapshot;
String weatherKeySnapshot;
time_t weatherLastSuccess = 0;
unsigned long weatherLastSuccessAt = 0;
constexpr unsigned long WEATHER_STALE_AFTER_MS = 30UL * 60UL * 1000UL;
// The audio decoder owns a priority-2 task on core 0.  TLS setup and JSON
// parsing may run for milliseconds at a time, so keep weather's background
// network work on the Arduino loop core instead of starving IDLE0.
constexpr BaseType_t NETWORK_WORKER_CORE = ARDUINO_RUNNING_CORE;
constexpr UBaseType_t NETWORK_WORKER_PRIORITY = 1;
bool firmwareUpdateOverlayActive = false;
uint8_t firmwareUpdatePercent = 0;

String urlEncode(const String& value) {
    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t character = static_cast<uint8_t>(value[i]);
        if (isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~') {
            encoded += static_cast<char>(character);
        } else {
            encoded += '%';
            encoded += HEX_DIGITS[character >> 4];
            encoded += HEX_DIGITS[character & 0x0F];
        }
    }
    return encoded;
}

bool fetchWeather(float& temperatureResult, int& weatherIdResult) {
    bool success = false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(3000);
    const String url = "https://api.openweathermap.org/data/2.5/weather?q=" +
        urlEncode(weatherCitySnapshot) + "&appid=" + urlEncode(weatherKeySnapshot) + "&units=metric";
    if (http.begin(client, url) && http.GET() == HTTP_CODE_OK) {
        JsonDocument filter;
        filter["main"]["temp"] = true;
        filter["weather"][0]["id"] = true;
        JsonDocument document;
        const DeserializationError error = deserializeJson(
            document,
            http.getStream(),
            DeserializationOption::Filter(filter));
        const JsonVariantConst temperature = document["main"]["temp"];
        const JsonVariantConst condition = document["weather"][0]["id"];
        if (!error && !temperature.isNull() && !condition.isNull()) {
            temperatureResult = temperature.as<float>();
            weatherIdResult = condition.as<int>();
            success = true;
        }
    }
    http.end();
    return success;
}

void fetchWeatherTask(void* parameter) {
    (void)parameter;
    float newTemperature = 0.0F;
    int newWeatherId = 0;

    if (fetchWeather(newTemperature, newWeatherId)) {
        portENTER_CRITICAL(&weatherStateMux);
        // A late task must never make data from a replaced location/key look
        // current.  The next requested refresh will use the committed values.
        if (weatherFetchGeneration == weatherConfigurationGeneration) {
            tempC = newTemperature;
            weatherID = newWeatherId;
            weatherDataValid = true;
            weatherLastSuccess = time(nullptr);
            weatherLastSuccessAt = millis();
        }
        portEXIT_CRITICAL(&weatherStateMux);
        forceRedraw = true;
    }
    weatherFetchInProgress = false;
    vTaskDelete(nullptr);
}

}  // namespace

uint16_t hexTo565(String hex) {
    if (hex.startsWith("#")) {
        hex.remove(0, 1);
    }
    const uint32_t rgb = strtoul(hex.c_str(), nullptr, 16);
    return tft.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void updateColors() {
    currentSkin.bgTop = hexTo565(currentSkin.hexTop);
    currentSkin.bgBottom = hexTo565(currentSkin.hexBottom);
    currentSkin.textMain = hexTo565(currentSkin.hexMain);
    currentSkin.textAccent = hexTo565(currentSkin.hexAccent);
    currentSkin.wifiSig = hexTo565(currentSkin.hexWifi);
    currentSkin.selMode = hexTo565(currentSkin.hexSel);
    currentSkin.clk = hexTo565(currentSkin.hexClk);
    currentSkin.hInfo = hexTo565(currentSkin.hexHInfo);
    currentSkin.barLow = hexTo565(currentSkin.hexBarL);
    currentSkin.barMid = hexTo565(currentSkin.hexBarM);
    currentSkin.barHigh = hexTo565(currentSkin.hexBarH);
    currentSkin.volBar = hexTo565(currentSkin.hexVol);
    currentSkin.almWarn = hexTo565(currentSkin.hexAlm);
    forceRedraw = true;
}

void setBrightness(int duty) {
    ledcWrite(TFT_BLK, duty);
}

void drawAnalogVU() {
    if (isAP || !showSpectrum) {
        return;
    }

    static float needlePosition = 0;
    static int lastVisualMode = -1;
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 35) {
        return;
    }
    lastUpdate = millis();

    if (visualMode != lastVisualMode) {
        tft.fillRect(0, 160, 320, 80, TFT_BLACK);
        lastVisualMode = visualMode;
    }

    const uint32_t level = mediaVuLevel();
    needlePosition +=
        (map(level, 0, UINT8_MAX, 0, 100) - needlePosition) * 0.15F;
    constexpr int boxWidth = 70;
    constexpr int boxHeight = 70;
    constexpr int boxX = (320 - boxWidth) / 2;
    constexpr int boxY = 160;
    constexpr int centerX = 160;
    constexpr int centerY = boxY + boxHeight + 5;
    constexpr int radius = 55;
    const uint16_t amber = tft.color565(255, 185, 40);

    tft.startWrite();
    tft.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 4, amber);
    for (int angle = -35; angle <= 35; angle += 10) {
        const float radians = (angle - 90) * 0.0174F;
        tft.drawLine(
            centerX + cos(radians) * (radius - 4),
            centerY + sin(radians) * (radius - 4),
            centerX + cos(radians) * radius,
            centerY + sin(radians) * radius,
            angle > 20 ? TFT_RED : TFT_BLACK);
    }
    const float angle = needlePosition * 0.7F - 35;
    const float radians = (angle - 90) * 0.0174F;
    tft.drawLine(
        centerX,
        centerY - 8,
        centerX + cos(radians) * radius,
        centerY + sin(radians) * radius,
        TFT_RED);
    tft.endWrite();
}

void drawWeatherIcon(int x, int y, int weatherId) {
    tft.fillRect(x, y, 35, 35, currentSkin.bgTop);
    if (weatherId == 800) {
        tft.fillCircle(x + 17, y + 17, 7, TFT_YELLOW);
        for (int i = 0; i < 8; ++i) {
            const float angle = i * 45 * 0.01745F;
            tft.drawLine(
                x + 17 + cos(angle) * 9,
                y + 17 + sin(angle) * 9,
                x + 17 + cos(angle) * 14,
                y + 17 + sin(angle) * 14,
                TFT_YELLOW);
        }
    } else if (weatherId >= 801 && weatherId <= 804) {
        constexpr uint16_t cloud = 0xCE79;
        tft.fillCircle(x + 12, y + 22, 6, cloud);
        tft.fillCircle(x + 18, y + 16, 9, cloud);
        tft.fillCircle(x + 25, y + 22, 6, cloud);
        tft.fillRect(x + 12, y + 22, 13, 6, cloud);
    } else if (weatherId >= 500 && weatherId <= 531) {
        tft.fillCircle(x + 14, y + 14, 5, 0xAD55);
        tft.fillCircle(x + 20, y + 10, 7, 0xAD55);
        tft.fillCircle(x + 26, y + 14, 5, 0xAD55);
        for (int i = 0; i < 3; ++i) {
            tft.drawLine(x + 15 + i * 5, y + 18, x + 13 + i * 5, y + 24, 0x041F);
        }
    } else if (weatherId >= 200 && weatherId <= 232) {
        tft.fillCircle(x + 18, y + 14, 8, 0x738E);
        tft.drawLine(x + 18, y + 18, x + 14, y + 24, TFT_YELLOW);
        tft.drawLine(x + 14, y + 24, x + 19, y + 24, TFT_YELLOW);
        tft.drawLine(x + 19, y + 24, x + 15, y + 30, TFT_YELLOW);
    } else if (weatherId >= 600 && weatherId <= 622) {
        tft.fillCircle(x + 18, y + 14, 8, 0xAD55);
        for (int i = 0; i < 3; ++i) {
            const int flakeX = x + 12 + i * 6;
            const int flakeY = y + 22;
            tft.drawLine(flakeX - 2, flakeY, flakeX + 2, flakeY, TFT_WHITE);
            tft.drawLine(flakeX, flakeY - 2, flakeX, flakeY + 2, TFT_WHITE);
        }
    }
}

void updateWeatherData() {
    static unsigned long lastWeatherUpdate = 0;
    if (!weatherFetchInProgress &&
        (weatherRefreshRequested || millis() - lastWeatherUpdate > 900000 || lastWeatherUpdate == 0)) {
        if (WiFi.status() == WL_CONNECTED && !isAP && !owmKey.isEmpty()) {
            weatherCitySnapshot = owmCity;
            weatherKeySnapshot = owmKey;
            weatherFetchGeneration = weatherConfigurationGeneration;
            weatherFetchInProgress = true;
            weatherRefreshRequested = false;
            if (xTaskCreatePinnedToCore(
                    fetchWeatherTask,
                    "Weather",
                    8192,
                    nullptr,
                    NETWORK_WORKER_PRIORITY,
                    nullptr,
                    NETWORK_WORKER_CORE) != pdPASS) {
                weatherFetchInProgress = false;
            } else {
                lastWeatherUpdate = millis();
            }
        } else if (weatherRefreshRequested && (isAP || owmKey.isEmpty())) {
            // No request can run until Wi-Fi and provider access exist.  Keep
            // the state unavailable without rechecking the same request on
            // every audio-service pass.
            weatherRefreshRequested = false;
        }
    }
}

void invalidateWeatherData() {
    portENTER_CRITICAL(&weatherStateMux);
    ++weatherConfigurationGeneration;
    weatherDataValid = false;
    weatherLastSuccess = 0;
    weatherLastSuccessAt = 0;
    portEXIT_CRITICAL(&weatherStateMux);
    weatherRefreshRequested = true;
    forceRedraw = true;
}

WeatherStatus weatherStatus() {
    WeatherStatus status;
    portENTER_CRITICAL(&weatherStateMux);
    status.available = weatherDataValid && millis() - weatherLastSuccessAt <= WEATHER_STALE_AFTER_MS;
    status.lastSuccess = weatherLastSuccess;
    portEXIT_CRITICAL(&weatherStateMux);
    status.refreshing = weatherFetchInProgress;
    return status;
}

void updateWeatherUI() {
    updateWeatherData();

    float displayedTemperature = 0.0F;
    int displayedWeatherId = 0;
    bool hasWeatherData = false;
    portENTER_CRITICAL(&weatherStateMux);
    displayedTemperature = tempC;
    displayedWeatherId = weatherID;
    hasWeatherData = weatherDataValid && millis() - weatherLastSuccessAt <= WEATHER_STALE_AFTER_MS;
    portEXIT_CRITICAL(&weatherStateMux);

    if (hasWeatherData) {
        drawWeatherIcon(70, 2, displayedWeatherId);
        tft.setTextColor(currentSkin.hInfo);
        tft.setFont(&fonts::Font0);
        tft.setCursor(103, 12);
        if (useCelsius) {
            tft.printf("%.1fC", displayedTemperature);
        } else {
            tft.printf("%.1fF", displayedTemperature * 9.0F / 5.0F + 32.0F);
        }
    }
}

void drawWifiSignal(int x, int y) {
    if (isAP) {
        tft.setTextColor(currentSkin.textAccent);
        tft.setFont(&fonts::Font0);
        tft.drawCenterString("AP", x + 12, y + 4);
        return;
    }

    const int32_t rssi = WiFi.RSSI();
    const int bars = rssi > -50 ? 5 : rssi > -63 ? 4 : rssi > -75 ? 3 : rssi > -85 ? 2 : rssi > -95 ? 1 : 0;
    for (int i = 0; i < 5; ++i) {
        const int height = i * 3 + 3;
        tft.fillRect(x + i * 6 + 1, y + (15 - height) + 1, 4, height, TFT_BLACK);
        const uint16_t color = i < bars
            ? (i < 2 ? currentSkin.barLow : i < 4 ? currentSkin.barMid : currentSkin.wifiSig)
            : tft.color565(40, 40, 40);
        tft.fillRect(x + i * 6, y + (15 - height), 4, height, color);
    }
}

void drawSpectrum() {
    if (!showSpectrum || isAP) {
        return;
    }

    static unsigned long lastSpectrumUpdate = 0;
    if (millis() - lastSpectrumUpdate < 40) {
        return;
    }
    lastSpectrumUpdate = millis();

    const uint32_t level = mediaVuLevel();
    for (int i = 0; i < 2; ++i) {
        const int targetHeight = map(level, 0, UINT8_MAX, 0, 65);
        barHeights[i] += barHeights[i] < targetHeight ? 4 : -3;
        const int height = constrain(barHeights[i], 0, 65);
        tft.fillRect(10 + i * 28, 160, 24, 65 - height, currentSkin.bgBottom);
        for (int segment = 0; segment < height; segment += 4) {
            const uint16_t color = segment < 25
                ? currentSkin.barLow
                : segment < 45 ? currentSkin.barMid : currentSkin.barHigh;
            tft.fillRect(10 + i * 28, 225 - segment, 24, 3, color);
        }
    }
}

namespace {

// Anti-alias only in readable RAM. No smooth glyph is ever drawn directly
// on the SPI panel, whose readback is not reliable on this hardware.
lgfx::LGFX_Sprite uiFrame;
bool uiFrameReady = false;

void initCanvas() {
    static bool attempted = false;
    if (attempted) return;
    attempted = true;
    // Explicit PSRAM capability prevents a full frame consuming audio's internal
    // heap when PSRAM is missing. Retain the usable bitmap path on failure.
    void* buffer = heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == nullptr) {
        Serial.println("[UI] Bitmap fallback: PSRAM canvas allocation failed");
        return;
    }
    if (!display_fonts::init()) {
        Serial.println("[UI] Bitmap fallback: smooth font allocation failed");
        heap_caps_free(buffer);
        return;
    }
    uiFrame.setBuffer(buffer, 320, 240, 16);
    uiFrameReady = true;
    Serial.println("[UI] Smooth fonts enabled; 153600-byte RGB565 canvas in PSRAM");
}

lgfx::LGFXBase& canvas() {
    return uiFrameReady ? static_cast<lgfx::LGFXBase&>(uiFrame) : tft;
}

const lgfx::IFont* uiFont(const lgfx::IFont* bitmap) {
    return uiFrameReady ? display_fonts::smooth(bitmap) : bitmap;
}

void serviceUiAudio() { audio.loop(); }

void presentCanvas(int16_t top, int16_t height) {
    if (!uiFrameReady) return;
    // Sprite storage is swap565, not host uint16_t RGB565. The typed overload
    // handles byte order explicitly. Each synchronous write ends before audio.
    const auto* pixels = static_cast<const lgfx::swap565_t*>(uiFrame.getBuffer());
    for (int16_t y = top; y < top + height; y += 8) {
        const int16_t rows = std::min<int16_t>(8, top + height - y);
        tft.pushImage(0, y, 320, rows, pixels + y * 320);
        serviceUiAudio();
    }
}

constexpr uint16_t kNavy = 0x0822;
constexpr uint16_t kSurface = 0x10A5;
constexpr uint16_t kSurfaceRaised = 0x212B;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kClockText = 0xF7BE;  // #F5F5F5 in RGB565
constexpr uint16_t kClockDate = 0xD6FB;  // #D8DDE3 in RGB565
constexpr uint16_t kTextMuted = 0xB596;
constexpr uint16_t kBlue = 0x13DE;
constexpr uint16_t kBlueDark = 0x0B16;
constexpr uint16_t kBlueFocus = 0x8E5F;
constexpr uint16_t kRed = 0xF945;
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kGreen = 0x154B;
constexpr uint16_t kPurple = 0x8218;
constexpr uint16_t kSlate = 0x4391;
// Home controls use this soft off-white instead of fully saturated white.
constexpr uint16_t kHomeText = 0xE77E;  // #E8EEF3 in RGB565
constexpr uint16_t kHomeBlue = 0x09D0;
constexpr uint16_t kHomeGreen = 0x1266;
constexpr uint16_t kHomePurple = 0x494E;
constexpr uint16_t kHomeSlate = 0x2A4B;
constexpr uint16_t kRecordedProgressTrack = 0x6B4D;

// NETWORK_QR_BEGIN
struct QrGrid {
    const uint8_t* modules;
    int size;
    int quietZone;
    int stride;
};

constexpr bool sameText(const char* left, const char* right) {
    return *left == *right && (*left == '\0' || sameText(left + 1, right + 1));
}

constexpr bool startsWith(const char* text, const char* prefix) {
    return *prefix == '\0' || (*text == *prefix && startsWith(text + 1, prefix + 1));
}

constexpr const char* skipPrefix(const char* text, const char* prefix) {
    return *prefix == '\0' ? text : skipPrefix(text + 1, prefix + 1);
}

constexpr bool setupPayloadMatches(const char* payload) {
    constexpr const char* prefix = "WIFI:T:nopass;S:";
    constexpr const char* suffix = ";;";
    return startsWith(payload, prefix) &&
        startsWith(skipPrefix(payload, prefix), kSetupAccessPointSsid) &&
        sameText(skipPrefix(skipPrefix(payload, prefix), kSetupAccessPointSsid), suffix);
}

static_assert(startsWith(ConfigQrCode::Url, "http://") &&
                  sameText(ConfigQrCode::Url + 7, kRadioMdnsAddress),
              "Config QR and mDNS name disagree; run tools/generate_network_qr_codes.py");
static_assert(startsWith(kRadioMdnsAddress, kRadioMdnsHostname) &&
                  sameText(skipPrefix(kRadioMdnsAddress, kRadioMdnsHostname), ".local"),
              "mDNS address must be the hostname plus .local");
static_assert(setupPayloadMatches(SetupWifiQrCode::Payload),
              "Setup QR and AP SSID disagree; run tools/generate_network_qr_codes.py");

constexpr QrGrid kConfigQr = {ConfigQrCode::Modules, ConfigQrCode::Size,
                               ConfigQrCode::QuietZone, ConfigQrCode::Stride};
constexpr QrGrid kSetupWifiQr = {SetupWifiQrCode::Modules, SetupWifiQrCode::Size,
                                  SetupWifiQrCode::QuietZone, SetupWifiQrCode::Stride};

int qrBadgeSide(const QrGrid& qr, int maximum) {
    const int modules = qr.size + 2 * qr.quietZone;
    return (maximum / modules) * modules;
}

bool qrDark(const QrGrid& qr, int x, int y) {
    const uint8_t packed = pgm_read_byte(&qr.modules[y * qr.stride + x / 8]);
    return (packed >> (7 - (x % 8))) & 1U;
}

void drawQrBadge(const QrGrid& qr, int x, int y, int maximum) {
    const int side = qrBadgeSide(qr, maximum);
    const int modules = qr.size + 2 * qr.quietZone;
    const int scale = side / modules;
    if (scale < 1) return;

    // A QR is camera-readable, so use opaque black/white whole-pixel modules
    // instead of the interface's translucent surfaces or antialiased scaling.
    canvas().fillRoundRect(x, y, side, side, 2 * scale, kWhite);
    const int codeX = x + qr.quietZone * scale;
    const int codeY = y + qr.quietZone * scale;
    canvas().startWrite();
    for (int row = 0; row < qr.size; ++row) {
        int runStart = -1;
        for (int column = 0; column <= qr.size; ++column) {
            const bool dark = column < qr.size && qrDark(qr, column, row);
            if (dark && runStart < 0) {
                runStart = column;
            } else if (!dark && runStart >= 0) {
                canvas().fillRect(codeX + runStart * scale, codeY + row * scale,
                                  (column - runStart) * scale, scale, TFT_BLACK);
                runStart = -1;
            }
        }
    }
    canvas().endWrite();
}

void drawConfigurationQrBadge(int x, int y, int maximum) {
    drawQrBadge(kConfigQr, x, y, maximum);
}

const char* setupReasonText() {
    switch (setupAccessReason) {
    case SetupAccessReason::ConnectionFailed: return "Couldn't join saved Wi-Fi";
    case SetupAccessReason::Requested: return "Wi-Fi setup requested";
    case SetupAccessReason::NoCredentials: return "No Wi-Fi network saved";
    }
    return "Wi-Fi setup";
}

void text(const String& value, int16_t x, int16_t y, const lgfx::IFont* font,
          uint16_t color, int16_t width);

void drawNetworkQrHandoff(bool connected) {
    const QrGrid& qr = connected ? kConfigQr : kSetupWifiQr;
    const int qrSide = qrBadgeSide(qr, 148);
    drawQrBadge(qr, (160 - qrSide) / 2, 48 + (148 - qrSide) / 2, 148);

    canvas().fillRoundRect(164, 54, 148, 142, 8, kSurface);
    if (connected) {
        text("Scan to configure", 176, 70, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
        text(kRadioMdnsAddress, 176, 98, uiFont(&fonts::FreeSansBold12pt7b), kBlueFocus, 124);
        text("If it does not open:", 176, 130, uiFont(&fonts::Font0), kTextMuted, 124);
        text(WiFi.localIP().toString(), 176, 148, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
        text("Use the Network page", 176, 174, uiFont(&fonts::Font0), kTextMuted, 124);
    } else {
        text(setupReasonText(), 176, 68, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
        text("Scan to join", 176, 96, uiFont(&fonts::FreeSansBold12pt7b), kBlueFocus, 124);
        text(kSetupAccessPointSsid, 176, 122, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
        text("Then open", 176, 150, uiFont(&fonts::Font0), kTextMuted, 124);
        text(WiFi.softAPIP().toString(), 176, 168, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
    }
}
// NETWORK_QR_END

#ifndef UI_P2_FIXTURE
#define UI_P2_FIXTURE 0
#endif

constexpr int16_t kHomeTileY = 149;
constexpr int16_t kHomeTileWidth = 72;
constexpr int16_t kHomeTileHeight = 70;
constexpr int16_t kHomeWeatherTextLeft = 82;
constexpr int16_t kHomeWeatherCityTop = 97;
constexpr int16_t kHomeWeatherConditionTop = 113;
constexpr int16_t kHomeTileIconSize = 34;
constexpr int16_t kHomeTileX[] = {7, 85, 163, 241};
// The four 34 px PNG canvases have different transparent top padding.  Anchor
// their visible artwork at one shared line, instead of making the radio aerial
// look higher than the list, heart, and settings marks.
constexpr int16_t kHomeTileIconVisibleTop = kHomeTileY + 8;
constexpr int8_t kHomeTileIconTransparentTop[] = {0, 4, 4, 1};
// The direct-TFT primitives are not the PNGs, so retain their equivalent
// optical corrections when PSRAM is unavailable.
constexpr int8_t kHomeTileIconFallbackYOffset[] = {-3, -4, 0, 1};
// The visible chevron is small, but its target reaches into the title margin
// and a little below the header so a normal finger press reliably returns.
constexpr int16_t kHeaderBackHitWidth = 72;
constexpr int16_t kHeaderBackHitHeight = 48;
constexpr int kStationSlotCount = 10;
constexpr int16_t kStationListTop = 44;
constexpr int16_t kStationListRowHeight = 48;
constexpr int kStationRowsPerPage = UI_LIST_ROWS;
constexpr int16_t kListCardWidth = 248;
constexpr int16_t kListCardHeight = 46;
constexpr int16_t kListOuterInset = 5;
constexpr int16_t kListRailGap = 5;
constexpr int16_t kListRailLeft = kListOuterInset + kListCardWidth + kListRailGap;
constexpr int16_t kListRailWidth = 320 - kListRailLeft - kListOuterInset;
constexpr int16_t kListRailBottom = kStationListTop +
    (kStationRowsPerPage - 1) * kStationListRowHeight + kListCardHeight;
constexpr int16_t kListRailHeight = kListRailBottom - kStationListTop;
constexpr int16_t kListPagerLeft = kListRailLeft + 4;
constexpr int kFavoriteRowsPerPage = 3;
constexpr int16_t kFavoriteListTop = 88;
constexpr int16_t kFavoriteListRowHeight = 48;
constexpr int16_t kFavoriteListBottom = kFavoriteListTop +
    (kFavoriteRowsPerPage - 1) * kFavoriteListRowHeight + kListCardHeight;
// The font's visible glyphs sit below its top-left origin.  This offset keeps
// a single-line label optically centered in a 46 px list card.
constexpr int16_t kListPrimaryTextTop = 11;
constexpr int16_t kPageHeaderCenterY = 22;

// The physical TFT makes the compact face read farther left than the native
// renderer. Keep a two-pixel drawing inset from the Home title's x=38 anchor,
// and stop well before the large clock so neither antialiasing nor camera bloom
// makes the two regions appear to touch.
constexpr int16_t kHomeStationTitleLeft = 40;
constexpr int16_t kHomeStationTitleTop = 30;
constexpr int16_t kHomeStationTitleRight = 180;
constexpr int16_t kHomeStationTitleWidth =
    kHomeStationTitleRight - kHomeStationTitleLeft;
constexpr int16_t kHomeStationTitleHeight = 14;
constexpr unsigned long kHomeStationTitleRestMs = 3000;
constexpr unsigned long kHomeStationTitlePixelsPerSecond = 25;
constexpr unsigned long kHomeStationTitleTickMs = 40;

struct HomeStationTitleMarquee {
    String identity;
    String stationVisual;
    String sourceSuffix;
    int16_t stationWidth = 0;
    int16_t width = 0;
    unsigned long cycleStartedAt = 0;
    unsigned long nextRefreshAt = 0;
    bool overflows = false;
};

HomeStationTitleMarquee homeStationTitleMarquee;
// The full Home canvas lives in PSRAM. Retaining just the subtitle backdrop
// lets the marquee repaint a 14 px band instead of pushing a new full frame.
uint16_t homeStationTitleBackdrop[kHomeStationTitleWidth * kHomeStationTitleHeight];
bool homeStationTitleBackdropReady = false;

const lgfx::IFont* homeLabelFont() {
    return uiFrameReady ? display_fonts::homeLabel() : &fonts::Font0;
}

const lgfx::IFont* homeCaptionFont() {
    return uiFrameReady ? display_fonts::caption() : &fonts::Font0;
}

void captureHomeStationTitleBackdrop() {
    if (!uiFrameReady) {
        homeStationTitleBackdropReady = false;
        return;
    }
    canvas().readRect(kHomeStationTitleLeft, kHomeStationTitleTop,
                      kHomeStationTitleWidth, kHomeStationTitleHeight,
                      homeStationTitleBackdrop);
    homeStationTitleBackdropReady = true;
}

void restoreHomeStationTitleBackdrop() {
    if (uiFrameReady && homeStationTitleBackdropReady) {
        canvas().pushImage(kHomeStationTitleLeft, kHomeStationTitleTop,
                           kHomeStationTitleWidth, kHomeStationTitleHeight,
                           homeStationTitleBackdrop);
        return;
    }

    // Without the PSRAM canvas, redraw only the clipped photo region rather
    // than relying on unreliable TFT readback.
    canvas().setClipRect(kHomeStationTitleLeft, kHomeStationTitleTop,
                         kHomeStationTitleWidth, kHomeStationTitleHeight);
    canvas().drawPng(ui_home_background, sizeof(ui_home_background), 0, 0);
    canvas().clearClipRect();
}

unsigned long homeStationTitleOffset(unsigned long now) {
    if (!homeStationTitleMarquee.overflows) return 0;
    const unsigned long travel = homeStationTitleMarquee.width - kHomeStationTitleWidth;
    const unsigned long scrollMs = std::max<unsigned long>(1,
        travel * 1000UL / kHomeStationTitlePixelsPerSecond);
    const unsigned long cycleMs = kHomeStationTitleRestMs + scrollMs + kHomeStationTitleRestMs;
    unsigned long phase = (now - homeStationTitleMarquee.cycleStartedAt) % cycleMs;
    if (phase <= kHomeStationTitleRestMs) return 0;
    phase -= kHomeStationTitleRestMs;
    if (phase >= scrollMs) return travel;
    return phase * travel / scrollMs;
}

void scheduleHomeStationTitleRefresh(unsigned long now) {
    if (!homeStationTitleMarquee.overflows) return;
    const unsigned long travel = homeStationTitleMarquee.width - kHomeStationTitleWidth;
    const unsigned long scrollMs = std::max<unsigned long>(1,
        travel * 1000UL / kHomeStationTitlePixelsPerSecond);
    const unsigned long cycleMs = kHomeStationTitleRestMs + scrollMs + kHomeStationTitleRestMs;
    const unsigned long phase = (now - homeStationTitleMarquee.cycleStartedAt) % cycleMs;
    if (phase < kHomeStationTitleRestMs) {
        homeStationTitleMarquee.nextRefreshAt = now + (kHomeStationTitleRestMs - phase);
    } else if (phase < kHomeStationTitleRestMs + scrollMs) {
        homeStationTitleMarquee.nextRefreshAt = now + kHomeStationTitleTickMs;
    } else {
        homeStationTitleMarquee.nextRefreshAt = now + (cycleMs - phase);
    }
}

String homeStationTitleVisual(const String& station, const char* source) {
    if (station.isEmpty()) return String(source);
    const UiTextLayout stationLayout = uiTextLayout(station);

    // Keep the two semantic fields in the same UI order for every script.
    // Passing the complete "station • source" string through the RTL adapter
    // makes a Hebrew station move after "• Live Radio".
    String visual = stationLayout.visual;
    visual += " • ";
    visual += source;
    return visual;
}

void prepareHomeStationTitle(const String& station, const char* source, unsigned long now) {
    const String identity = station.isEmpty()
        ? String(source)
        : station + " • " + source;
    if (identity == homeStationTitleMarquee.identity) return;

    canvas().setFont(uiFont(&fonts::Font0));
    const String stationVisual = station.isEmpty()
        ? String(source)
        : uiTextLayout(station).visual;
    const String sourceSuffix = station.isEmpty()
        ? String()
        : String(" • ") + source;
    homeStationTitleMarquee.identity = identity;
    homeStationTitleMarquee.stationVisual = stationVisual;
    homeStationTitleMarquee.sourceSuffix = sourceSuffix;
    homeStationTitleMarquee.stationWidth = canvas().textWidth(stationVisual.c_str());
    homeStationTitleMarquee.width = homeStationTitleMarquee.stationWidth
        + canvas().textWidth(sourceSuffix.c_str());
    homeStationTitleMarquee.overflows = homeStationTitleMarquee.width > kHomeStationTitleWidth;
    homeStationTitleMarquee.cycleStartedAt = now;
    homeStationTitleMarquee.nextRefreshAt = now;
    scheduleHomeStationTitleRefresh(now);
}

void drawHomeStationTitle(unsigned long now) {
    restoreHomeStationTitleBackdrop();
    const int16_t offset = static_cast<int16_t>(homeStationTitleOffset(now));
    canvas().setFont(uiFont(&fonts::Font0));
    canvas().setTextColor(kWhite);
    canvas().setTextDatum(TL_DATUM);
    // This header deliberately stays left-anchored even when its content is
    // Hebrew. The generic RTL helper right-aligns bounded labels, which would
    // make this subtitle drift toward the clock.
    canvas().setClipRect(kHomeStationTitleLeft, kHomeStationTitleTop,
                         kHomeStationTitleWidth, kHomeStationTitleHeight);
    const int16_t stationX = kHomeStationTitleLeft - offset;
    // Draw the semantic runs separately. The Hebrew station always owns the
    // left edge; the Latin source can only follow it on the right and cannot
    // be moved ahead of it by mixed-script bidi layout.
    canvas().drawString(homeStationTitleMarquee.stationVisual.c_str(),
                        stationX, kHomeStationTitleTop);
    if (!homeStationTitleMarquee.sourceSuffix.isEmpty()) {
        canvas().drawString(homeStationTitleMarquee.sourceSuffix.c_str(),
                            stationX + homeStationTitleMarquee.stationWidth,
                            kHomeStationTitleTop);
    }
    canvas().clearClipRect();
    scheduleHomeStationTitleRefresh(now);
}

bool contains(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

int listRowAt(int16_t y) {
    if (y < kStationListTop) return -1;
    const int row = (y - kStationListTop) / kStationListRowHeight;
    if (row < 0 || row >= kStationRowsPerPage ||
        y >= kStationListTop + row * kStationListRowHeight + kListCardHeight) {
        return -1;
    }
    return row;
}

String ellipsize(const String& source, int16_t width) {
    if (canvas().textWidth(source.c_str()) <= width) {
        return source;
    }
    String result = source;
    while (!result.isEmpty()) {
        size_t end = result.length() - 1;
        while (end > 0 && (static_cast<uint8_t>(result[end]) & 0xC0) == 0x80) --end;
        result.remove(end);
        const String candidate = result + "...";
        if (canvas().textWidth(candidate.c_str()) <= width) {
            return candidate;
        }
    }
    return "...";
}

String ellipsizeRightToLeft(const String& source, int16_t width) {
    if (canvas().textWidth(source.c_str()) <= width) {
        return source;
    }
    // The visual-order string starts with the logical tail. Keep its suffix so
    // an RTL reader retains the title's logical beginning at the right edge.
    String result = source;
    while (!result.isEmpty()) {
        size_t end = 1;
        while (end < result.length() && (static_cast<uint8_t>(result[end]) & 0xC0U) == 0x80U) ++end;
        result.remove(0, end);
        const String candidate = String("...") + result;
        if (canvas().textWidth(candidate.c_str()) <= width) {
            return candidate;
        }
    }
    return "...";
}

void text(const String& value, int16_t x, int16_t y, const lgfx::IFont* font, uint16_t color, int16_t width = 0) {
    canvas().setFont(font);
    canvas().setTextColor(color);
    canvas().setTextDatum(TL_DATUM);
    const UiTextLayout layout = uiTextLayout(value);
    const String rendered = width > 0
        ? (layout.rightToLeft ? ellipsizeRightToLeft(layout.visual, width) : ellipsize(layout.visual, width))
        : layout.visual;
    if (layout.rightToLeft && width > 0) {
        x += std::max<int16_t>(0, width - canvas().textWidth(rendered.c_str()));
    }
    canvas().drawString(rendered.c_str(), x, y);
}

void listPrimaryText(const String& value, int16_t x, int16_t rowY, int16_t width) {
    text(value, x, rowY + kListPrimaryTextTop, uiFont(&fonts::FreeSans9pt7b), kWhite, width);
}

void footerButton(int16_t x, int16_t width, const char* label, bool highlighted = false) {
    canvas().fillRoundRect(x + 2, 190, width - 4, 46, 6, highlighted ? kBlue : kSurface);
    canvas().drawRoundRect(x + 2, 190, width - 4, 46, 6, highlighted ? kWhite : kTextMuted);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(label, x + width / 2, 214, uiFont(&fonts::FreeSans9pt7b));
}

template<size_t N>
bool drawSurfaceAsset(const uint8_t (&asset)[N], int16_t x, int16_t y) {
    return canvas().drawPng(asset, N, x, y);
}

// Alpha surfaces must only be composited on the readable PSRAM canvas.  The
// opaque branch deliberately stays legible when that allocation is unavailable.
void drawListCard(int16_t x, int16_t y, bool focused, bool wide = false) {
    bool drawn = false;
    if (uiFrameReady) {
        if (wide) {
            drawn = focused ? drawSurfaceAsset(ui_list_card_wide_focused, x, y)
                            : drawSurfaceAsset(ui_list_card_wide, x, y);
        } else {
            drawn = focused ? drawSurfaceAsset(ui_list_card_focused, x, y)
                            : drawSurfaceAsset(ui_list_card, x, y);
        }
    }
    if (!drawn) {
        const int16_t width = wide ? 304 : kListCardWidth;
        const int16_t height = wide ? 42 : kListCardHeight;
        canvas().fillRoundRect(x, y, width, height, wide ? 6 : 7, focused ? kBlue : kSurface);
        canvas().drawRoundRect(x, y, width, height, wide ? 6 : 7, focused ? kBlueFocus : kTextMuted);
    }
}

void drawPagerChevron(int16_t centerX, int16_t centerY, bool up, bool enabled) {
    const uint16_t color = enabled ? kWhite : kTextMuted;
    // Three adjacent strokes give a confident, touch-button-scale chevron on
    // the RGB565 panel instead of the thin, fragmented old arrow style.
    for (int16_t offset = -1; offset <= 1; ++offset) {
        const int16_t wingY = centerY + (up ? 6 : -6) + offset;
        const int16_t tipY = centerY + (up ? -7 : 7) + offset;
        canvas().drawLine(centerX - 11, wingY, centerX, tipY, color);
        canvas().drawLine(centerX, tipY, centerX + 11, wingY, color);
    }
}

void drawListPager(int offset, int count, int rows, int16_t top = kStationListTop,
                   int16_t bottom = kListRailBottom,
                   bool showPosition = true) {
    const bool canPrevious = offset > 0;
    const bool canNext = offset + rows < count;
    const int16_t upY = top + 6;
    const int16_t downY = bottom - 50;
    const bool primaryListRail = top == kStationListTop && bottom == kListRailBottom;
    const int16_t pagerLeft = kListPagerLeft;
    const int16_t pagerCenterX = pagerLeft + 25;
    bool railDrawn = false;
    bool upDrawn = false;
    bool downDrawn = false;
    if (uiFrameReady) {
        if (primaryListRail) {
            railDrawn = drawSurfaceAsset(ui_list_rail, kListRailLeft, kStationListTop);
        } else if (top == kFavoriteListTop && bottom == kFavoriteListBottom) {
            railDrawn = drawSurfaceAsset(ui_list_rail_favorites, kListRailLeft, top);
        }
        upDrawn = canPrevious ? drawSurfaceAsset(ui_list_pager_active, pagerLeft, upY)
                              : drawSurfaceAsset(ui_list_pager, pagerLeft, upY);
        downDrawn = canNext ? drawSurfaceAsset(ui_list_pager_active, pagerLeft, downY)
                             : drawSurfaceAsset(ui_list_pager, pagerLeft, downY);
    }
    if (!railDrawn) {
        canvas().fillRoundRect(kListRailLeft, top, kListRailWidth, bottom - top, 7, kNavy);
        canvas().drawRoundRect(kListRailLeft, top, kListRailWidth, bottom - top, 7, kTextMuted);
    }
    if (!upDrawn) {
        canvas().fillRoundRect(pagerLeft, upY, 50, 44, 8, canPrevious ? kBlue : kSurface);
        canvas().drawRoundRect(pagerLeft, upY, 50, 44, 8, canPrevious ? kBlueFocus : kTextMuted);
    }
    if (!downDrawn) {
        canvas().fillRoundRect(pagerLeft, downY, 50, 44, 8, canNext ? kBlue : kSurface);
        canvas().drawRoundRect(pagerLeft, downY, 50, 44, 8, canNext ? kBlueFocus : kTextMuted);
    }
    drawPagerChevron(pagerCenterX, upY + 22, true, canPrevious);
    drawPagerChevron(pagerCenterX, downY + 22, false, canNext);
    if (showPosition) {
        // All catalog-backed lists are bounded to three pages (10 stations or
        // shows, and eight episodes).  Prepared variants avoid runtime PNG
        // scaling while the thumb's size and position explain list progress.
        const int pages = std::max(1, std::min(3, (count + rows - 1) / rows));
        const int maxOffset = std::max(0, count - rows);
        // The final page can be partial: for ten entries with four visible,
        // offsets are 0, 4 and 6. Map those real viewport offsets over the
        // whole track, rather than treating 6 / 4 as the middle page.
        const int page = maxOffset == 0 ? 0 :
            std::max(0, std::min(pages - 1, (offset * (pages - 1) + maxOffset / 2) / maxOffset));
        constexpr int16_t kTrackTop = 98;
        constexpr int16_t kThumbX = kListRailLeft + 7;
        bool thumbDrawn = false;
        if (uiFrameReady) {
            switch (pages) {
            case 1: thumbDrawn = drawSurfaceAsset(ui_list_pager_thumb_1, kThumbX, kTrackTop); break;
            case 2: thumbDrawn = drawSurfaceAsset(ui_list_pager_thumb_2, kThumbX, kTrackTop + page * 42); break;
            default: thumbDrawn = drawSurfaceAsset(ui_list_pager_thumb_3, kThumbX, kTrackTop + page * 28); break;
            }
        }
        if (!thumbDrawn) {
            const int16_t thumbHeight = 84 / pages;
            canvas().fillRoundRect(kThumbX, kTrackTop + page * thumbHeight, 44, thumbHeight, 6, kBlueDark);
            canvas().drawRoundRect(kThumbX, kTrackTop + page * thumbHeight, 44, thumbHeight, 6, kBlueFocus);
        }
    }
}

void drawBackground() {
    if (!canvas().drawPng(ui_background_png, ui_background_png_len, 0, 0, 320, 240)) {
        canvas().fillScreen(kNavy);
    }
}

void drawSettingsBackground() {
    drawBackground();
    if (uiFrameReady) {
        // Settings are information-dense. Retain the coastal image while
        // suppressing its highlights so labels and controls read immediately.
        canvas().fillRectAlpha(0, 0, 320, 240, 152, TFT_BLACK);
    } else {
        // Alpha blending requires a readable backing surface. The direct-TFT
        // fallback therefore uses the product navy instead of risking a slow
        // or unsupported full-screen readback.
        canvas().fillScreen(kNavy);
    }
}

void drawHomeHeader(bool timeValid, const String& station) {
    // Home intentionally has no opaque navigation bar: the top of the sunset
    // photo is part of this screen's composition. Other pages use the compact
    // shared page header below.
    constexpr int16_t centerY = 22;
    if (uiFrameReady) {
        canvas().drawPng(ui_home_brand_radio, sizeof(ui_home_brand_radio), 12, 10);
    } else {
        canvas().drawRoundRect(14, 18, 15, 10, 3, kHomeText);
        canvas().drawLine(17, 16, 27, 12, kHomeText);
        canvas().fillCircle(19, 23, 2, kHomeText);
        canvas().fillCircle(25, 23, 2, kHomeText);
    }
    canvas().setTextColor(kHomeText);
    canvas().setTextDatum(ML_DATUM);
    canvas().drawString("Radiohead", 38, centerY, display_fonts::homeTitle());
    const char* source = podcastMode ? "Recorded Show" : "Live Radio";
    captureHomeStationTitleBackdrop();
    prepareHomeStationTitle(station, source, millis());
    drawHomeStationTitle(millis());
    char date[16] = "";
    if (timeValid) {
        const time_t now = time(nullptr);
        tm localTime = {};
        if (configuredLocalTime(now, localTime)) {
            strftime(date, sizeof(date), "%a, %d %b", &localTime);
        }
    }
    canvas().setTextDatum(MR_DATUM);
    canvas().setTextColor(kClockDate);
    // The reference treats the day/date as a readable, light caption—not the
    // 11 px utility face. Keep it close to Wi-Fi while preserving its larger
    // 13 px optical height.
    canvas().drawString(date[0] == '\0' ? "" : date, 280, centerY - 1, display_fonts::caption());
    if (uiFrameReady) {
        // Visible Wi-Fi pixels span local y=4..16: optical center is y=10.
        canvas().drawPng(ui_home_wifi, sizeof(ui_home_wifi), 287, centerY - 10);
    } else {
        canvas().drawArc(298, centerY, 4, 6, 210, 330, kWhite);
        canvas().drawArc(298, centerY, 8, 10, 210, 330, kWhite);
        canvas().fillCircle(298, centerY + 5, 1, kWhite);
    }
}

void drawHomeWeatherIconFallback(int16_t x, int16_t y, bool hasWeather, int weatherId) {
    const bool cloudy = !hasWeather || (weatherId >= 801 && weatherId <= 804);
    const bool rainy = hasWeather && weatherId >= 500 && weatherId <= 531;
    const uint16_t sun = 0xFE40;
    canvas().fillCircle(x + 15, y + 14, 9, sun);
    for (int i = 0; i < 8; ++i) {
        const float angle = i * 45 * 0.0174533F;
        canvas().drawLine(x + 15 + cos(angle) * 12, y + 14 + sin(angle) * 12,
                     x + 15 + cos(angle) * 17, y + 14 + sin(angle) * 17, sun);
    }
    if (cloudy || rainy) {
        canvas().fillCircle(x + 23, y + 27, 9, kWhite);
        canvas().fillCircle(x + 33, y + 23, 12, kWhite);
        canvas().fillCircle(x + 45, y + 28, 8, kWhite);
        canvas().fillRoundRect(x + 20, y + 28, 33, 10, 5, kWhite);
    }
    if (rainy) {
        for (int i = 0; i < 3; ++i) {
            canvas().drawLine(x + 26 + i * 8, y + 40, x + 23 + i * 8, y + 46, kBlue);
        }
    }
}

void drawHomeTileIconFallback(int16_t tileX, int16_t tileY, uint8_t tile) {
    // Keep the no-PSRAM primitive path optically consistent with the prepared
    // 34 px icon assets. Its tile coordinates make the centering explicit.
    const int16_t x = tileX + (kHomeTileWidth - kHomeTileIconSize) / 2;
    const int16_t y = tileY + 4;
    const uint16_t iconColor = kHomeText;
    canvas().setTextColor(iconColor);
    if (tile == 0) {
        canvas().drawRoundRect(x + 4, y + 13, 31, 20, 4, iconColor);
        canvas().drawCircle(x + 13, y + 23, 3, iconColor);
        canvas().drawCircle(x + 27, y + 23, 3, iconColor);
        canvas().drawLine(x + 9, y + 10, x + 30, y + 3, iconColor);
        canvas().drawLine(x + 30, y + 3, x + 35, y + 3, iconColor);
    } else if (tile == 1) {
        for (int row = 0; row < 3; ++row) {
            canvas().fillCircle(x + 7, y + 10 + row * 10, 2, iconColor);
            canvas().fillRoundRect(x + 14, y + 8 + row * 10, 25, 4, 2, iconColor);
        }
    } else if (tile == 2) {
        canvas().drawLine(x + 20, y + 34, x + 4, y + 18, iconColor);
        canvas().drawLine(x + 4, y + 18, x + 4, y + 10, iconColor);
        canvas().drawLine(x + 4, y + 10, x + 10, y + 4, iconColor);
        canvas().drawLine(x + 10, y + 4, x + 20, y + 11, iconColor);
        canvas().drawLine(x + 20, y + 11, x + 30, y + 4, iconColor);
        canvas().drawLine(x + 30, y + 4, x + 36, y + 10, iconColor);
        canvas().drawLine(x + 36, y + 10, x + 36, y + 18, iconColor);
        canvas().drawLine(x + 36, y + 18, x + 20, y + 34, iconColor);
    } else {
        canvas().fillCircle(x + 20, y + 19, 12, iconColor);
        canvas().fillCircle(x + 20, y + 19, 5, kHomeSlate);
        for (int i = 0; i < 8; ++i) {
            const float angle = i * 45 * 0.0174533F;
            canvas().fillCircle(x + 20 + cos(angle) * 16, y + 19 + sin(angle) * 16, 3, iconColor);
        }
    }
}

// OpenWeather condition IDs; labels are deliberately concise, not invented
// descriptions. Unknown codes do not imply sunshine or a successful condition.
const char* homeWeatherDescription(int id) {
    if (id >= 200 && id <= 232) return "Thunderstorms";
    if (id >= 300 && id <= 321) return "Drizzle";
    if (id >= 500 && id <= 531) return "Rain";
    if (id >= 600 && id <= 622) return "Snow";
    switch (id) {
    case 701: return "Mist";
    case 711: return "Smoke";
    case 721: return "Haze";
    case 731: case 761: return "Dust";
    case 741: return "Fog";
    case 751: return "Sand";
    case 762: return "Volcanic ash";
    case 771: return "Squalls";
    case 781: return "Tornado";
    case 800: return "Clear sky";
    case 801: case 802: return "Partly cloudy";
    case 803: return "Cloudy";
    case 804: return "Overcast";
    default: return "Unknown";
    }
}

String homeCityLabel(const String& city) {
    const int comma = city.indexOf(',');
    String label = comma >= 0 ? city.substring(0, comma) : city;
    label.trim();
    return label.isEmpty() ? String("Weather") : label;
}

template<size_t N>
void drawHomeAsset(const uint8_t (&asset)[N], int16_t x, int16_t y) {
    // Called only on the readable canvas. PNG alpha never reads the TFT.
    canvas().drawPng(asset, N, x, y);
}

int8_t homeNumeralGlyphIndex(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character == ':' || character == '*') return 10;
    return character == '-' ? 11 : -1;
}

uint16_t blendHomeNumeralPixel(uint16_t background, uint8_t alpha, uint16_t color) {
    if (alpha == 255) return color;
    const uint16_t inverse = 255U - alpha;
    const uint16_t red = (((background >> 11) & 0x1FU) * inverse
        + ((color >> 11) & 0x1FU) * alpha + 127U) / 255U;
    const uint16_t green = (((background >> 5) & 0x3FU) * inverse
        + ((color >> 5) & 0x3FU) * alpha + 127U) / 255U;
    const uint16_t blue = ((background & 0x1FU) * inverse
        + (color & 0x1FU) * alpha + 127U) / 255U;
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

void drawHomeNumeralAtlas(const char* value, const uint8_t* atlas,
                         uint8_t cellWidth, uint8_t cellHeight,
                         const int32_t* advances, int32_t tracking,
                         int32_t colonSideSpacing,
                         int16_t anchorX, int16_t anchorTop, bool rightAligned,
                         uint16_t color) {
    // Compose overlapping masks once, then blend over the actual RGB565 frame.
    // Shared static storage bounds both roles without adding a second RAM buffer.
    constexpr int16_t kHomeNumeralMaxWidth = 160;
    constexpr uint8_t kHomeNumeralMaxHeight = 42;
    constexpr int32_t advanceScale = 10000;
    int32_t penUnits = 0;
    int16_t composedWidth = 0;
    for (const char* character = value; *character != '\0'; ++character) {
        const int8_t index = homeNumeralGlyphIndex(*character);
        if (index < 0) continue;
        if (*character == ':') penUnits += colonSideSpacing;
        if (penUnits > (kHomeNumeralMaxWidth - cellWidth) * advanceScale) return;
        const int16_t glyphX = static_cast<int16_t>((penUnits + advanceScale / 2) /
                                                    advanceScale);
        composedWidth = std::max<int16_t>(composedWidth, glyphX + cellWidth);
        penUnits += advances[index] + tracking;
        if (*character == ':') penUnits += colonSideSpacing;
    }
    if (penUnits <= 0 || composedWidth > kHomeNumeralMaxWidth ||
        cellHeight > kHomeNumeralMaxHeight) return;

    const size_t kCellBytes = cellWidth * cellHeight;
    static uint8_t composedAlpha[kHomeNumeralMaxWidth * kHomeNumeralMaxHeight];
    for (uint8_t y = 0; y < cellHeight; ++y) {
        for (int16_t x = 0; x < composedWidth; ++x) {
            composedAlpha[static_cast<size_t>(y) * kHomeNumeralMaxWidth + x] = 0;
        }
    }

    penUnits = 0;
    for (const char* character = value; *character != '\0'; ++character) {
        const int8_t glyph = homeNumeralGlyphIndex(*character);
        if (glyph < 0) continue;
        if (*character == ':') penUnits += colonSideSpacing;
        const int16_t glyphX = static_cast<int16_t>((penUnits + advanceScale / 2) /
                                                    advanceScale);
        const uint8_t* alpha = atlas + static_cast<size_t>(glyph) * kCellBytes;
        for (uint8_t y = 0; y < cellHeight; ++y) {
            for (uint8_t x = 0; x < cellWidth && glyphX + x < composedWidth; ++x) {
                const uint8_t coverage = alpha[y * cellWidth + x];
                if (coverage == 0) continue;
                uint8_t& composed = composedAlpha[static_cast<size_t>(y) * kHomeNumeralMaxWidth + glyphX + x];
                composed = static_cast<uint8_t>(coverage +
                    (static_cast<uint16_t>(composed) * (255U - coverage) + 127U) / 255U);
            }
        }
        penUnits += advances[glyph] + tracking;
        if (*character == ':') penUnits += colonSideSpacing;
        serviceUiAudio();
    }

    int16_t inkLeft = composedWidth;
    int16_t inkTop = cellHeight;
    int16_t inkRight = 0;
    int16_t inkBottom = 0;
    for (uint8_t y = 0; y < cellHeight; ++y) {
        for (int16_t x = 0; x < composedWidth; ++x) {
            if (composedAlpha[static_cast<size_t>(y) * kHomeNumeralMaxWidth + x] == 0) continue;
            if (x < inkLeft) inkLeft = x;
            if (y < inkTop) inkTop = y;
            if (x + 1 > inkRight) inkRight = x + 1;
            if (y + 1 > inkBottom) inkBottom = y + 1;
        }
    }
    if (inkRight <= inkLeft || inkBottom <= inkTop) return;

    const int16_t originX = anchorX - (rightAligned ? inkRight : inkLeft);
    const int16_t originY = anchorTop - inkTop;
    for (uint8_t y = 0; y < cellHeight; ++y) {
        for (int16_t x = 0; x < composedWidth; ++x) {
            const uint8_t coverage = composedAlpha[static_cast<size_t>(y) * kHomeNumeralMaxWidth + x];
            if (coverage == 0) continue;
            const int16_t pixelX = originX + x;
            const int16_t pixelY = originY + y;
            if (uiFrameReady) {
                canvas().drawPixel(pixelX, pixelY,
                                   blendHomeNumeralPixel(canvas().readPixel(pixelX, pixelY), coverage, color));
            } else if (coverage >= 128) {
                // The direct TFT path cannot safely read the sunset background
                // back for alpha blending. Paint the already-composed source
                // mask once, rather than falling back to a second font renderer.
                canvas().drawPixel(pixelX, pixelY, color);
            }
        }
        // Keep the audio stream serviced during the bounded final blend.
        if ((y & 0x07U) == 0x07U) serviceUiAudio();
    }
}

void drawHomeClockAtlas(const char* value) {
    drawHomeNumeralAtlas(value, ui_home_clock_alpha, ui_home_clock_cell_width,
        ui_home_clock_cell_height, ui_home_clock_advance_units,
        ui_home_clock_tracking_units, ui_home_clock_colon_side_spacing_units,
        ui_home_clock_ink_right, ui_home_clock_ink_top,
        true, kClockText);
}

void drawHomeTemperatureAtlas(const char* value) {
    // '*' selects the optically raised degree glyph in this numeral-only atlas.
    drawHomeNumeralAtlas(value, ui_home_temperature_alpha, ui_home_temperature_cell_width,
        ui_home_temperature_cell_height, ui_home_temperature_advance_units,
        ui_home_temperature_tracking_units, ui_home_temperature_colon_side_spacing_units,
        ui_home_temperature_ink_left,
        ui_home_temperature_ink_top, false, kWhite);
}

void drawHomeWeatherIcon(int16_t x, int16_t visibleTop, bool valid, int id) {
    if (!uiFrameReady) {
        // The fallback primitives and PNGs have different transparent geometry.
        // Keep their visible ink aligned with the temperature in either path.
        if (!valid || (id != 800 && id != 801 && id != 802)) {
            canvas().fillRoundRect(x + 14, visibleTop, 42, 22, 9, kTextMuted);
        } else {
            drawHomeWeatherIconFallback(x, visibleTop + 7, true, id);
        }
        return;
    }
    // Normalise actual ink, not transparent image rectangles. Clear's art has
    // a one-pixel inset, partly cloudy begins at zero, and cloud states begin
    // 12 pixels down in their common 64×56 canvas.
    if (!valid) drawHomeAsset(ui_home_weather_unknown, x, visibleTop - 12);
    else if (id == 800) drawHomeAsset(ui_home_weather_clear, x, visibleTop - 1);
    else if (id == 801 || id == 802) drawHomeAsset(ui_home_weather_partly, x, visibleTop);
    else if (id == 803 || id == 804) drawHomeAsset(ui_home_weather_cloudy, x, visibleTop - 12);
    else if (id >= 200 && id <= 232) drawHomeAsset(ui_home_weather_storm, x, visibleTop - 12);
    else if ((id >= 300 && id <= 321) || (id >= 500 && id <= 531)) drawHomeAsset(ui_home_weather_rain, x, visibleTop - 12);
    else if (id >= 600 && id <= 622) drawHomeAsset(ui_home_weather_snow, x, visibleTop - 12);
    else if (id >= 700 && id < 800) drawHomeAsset(ui_home_weather_mist, x, visibleTop - 12);
    else drawHomeAsset(ui_home_weather_unknown, x, visibleTop - 12);
}

void drawHomeTileIcon(int16_t x, int16_t y, uint8_t tile) {
    if (!uiFrameReady) {
        const uint8_t icon = tile < 4 ? tile : 0;
        drawHomeTileIconFallback(x - (kHomeTileWidth - kHomeTileIconSize) / 2,
                                 y - 4 + kHomeTileIconFallbackYOffset[icon], icon);
        return;
    }
    switch (tile) {
    case 0: drawHomeAsset(ui_home_icon_radio, x, y); break;
    case 1: drawHomeAsset(ui_home_icon_shows, x, y); break;
    case 2: drawHomeAsset(ui_home_icon_favorites, x, y); break;
    case 3: drawHomeAsset(ui_home_icon_settings, x, y); break;
    }
}

void drawArtwork(const String& name, int16_t x, int16_t y, int16_t size, uint16_t accent = kBlue,
                 bool framed = true, int stationSlot = -1) {
    if (framed) {
        canvas().fillRoundRect(x, y, size, size, 7, kSurfaceRaised);
        canvas().fillRoundRect(x + 4, y + 4, size - 8, size - 8, 5, accent);
    } else {
        canvas().fillRoundRect(x, y, size, size, 5, accent);
    }
    // Artwork is prepared in the browser and read only when this station/size
    // changes.  Rendering never decodes or downloads arbitrary image data.
    static uint16_t artworkPixels[88 * 88];
    static int cachedSlot = -2;
    static int cachedSize = 0;
    static uint32_t cachedRevision = 0;
    const uint32_t revision = stationArtworkContentRevision(stationSlot);
    if (stationSlot >= 0 && revision != 0 &&
        (cachedSlot != stationSlot || cachedSize != size || cachedRevision != revision)) {
        cachedSlot = stationSlot;
        cachedSize = size;
        cachedRevision = revision;
        if (!loadStationArtwork(stationSlot, size, artworkPixels, sizeof(artworkPixels) / sizeof(artworkPixels[0]))) {
            cachedSlot = -1;
        }
    }
    if (cachedSlot == stationSlot && cachedSize == size && cachedRevision == revision) {
        canvas().pushImage(x, y, size, size, artworkPixels);
        return;
    }
    String initials;
    for (size_t i = 0; i < name.length() && initials.length() < 2; ++i) {
        const char character = name[i];
        if (isAlphaNumeric(character)) initials += static_cast<char>(toupper(character));
    }
    if (initials.isEmpty()) initials = "R";
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(initials.c_str(), x + size / 2, y + size / 2 - 1,
                         size >= 64 ? uiFont(&fonts::FreeSansBold12pt7b) : uiFont(&fonts::FreeSans9pt7b));
}

void drawStar(int16_t cx, int16_t cy, bool filled = false) {
    // A ten-point silhouette is much clearer than five crossing thin lines at
    // the physical panel's resolution. Filled favorites use the reference's
    // warm yellow; unfavorited rows retain a crisp white outline.
    constexpr int8_t points[][2] = {
        {0, -13}, {4, -5}, {12, -4}, {6, 3}, {8, 11},
        {0, 7}, {-8, 11}, {-6, 3}, {-12, -4}, {-4, -5},
    };
    const uint16_t fill = 0xFE40;
    if (filled) {
        for (size_t point = 0; point < 10; ++point) {
            const size_t next = (point + 1) % 10;
            canvas().fillTriangle(cx, cy, cx + points[point][0], cy + points[point][1],
                                  cx + points[next][0], cy + points[next][1], fill);
        }
    }
    const uint16_t outline = filled ? fill : kWhite;
    for (size_t point = 0; point < 10; ++point) {
        const size_t next = (point + 1) % 10;
        canvas().drawLine(cx + points[point][0], cy + points[point][1],
                          cx + points[next][0], cy + points[next][1], outline);
    }
}

void drawTransport(int16_t x, int16_t y, const char* glyph, bool primary) {
    canvas().fillCircle(x, y, 24, kNavy);
    canvas().drawCircle(x, y, 24, primary ? kBlue : kTextMuted);
    if (primary) canvas().drawCircle(x, y, 25, kBlue);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(glyph, x, y - 1, uiFont(&fonts::FreeSansBold12pt7b));
}

void drawPageBackChevron() {
    if (uiFrameReady && canvas().drawPng(
            ui_header_back_chevron, sizeof(ui_header_back_chevron), 5, 10)) {
        return;
    }

    // The low-memory path cannot alpha-blend the prepared icon. Keep its
    // primitive fallback slim; two adjacent strokes read better than the old
    // filled polygon and remain visible without a readable backing canvas.
    canvas().drawLine(21, 13, 12, 22, kWhite);
    canvas().drawLine(12, 22, 21, 31, kWhite);
    canvas().drawLine(22, 13, 13, 22, kWhite);
    canvas().drawLine(13, 22, 22, 31, kWhite);
}

void drawPageHeader(const String& title, const char* currentTime, bool timeValid,
                    bool back = true) {
    const lgfx::IFont* headerFont = uiFont(&fonts::FreeSansBold12pt7b);
    if (back) drawPageBackChevron();

    constexpr int16_t kTitleRight = 210;
    const int16_t titleLeft = back ? 40 : 14;
    const int16_t titleWidth = kTitleRight - titleLeft;
    canvas().setFont(headerFont);
    canvas().setTextSize(1);
    canvas().setTextColor(kWhite);
    const UiTextLayout titleLayout = uiTextLayout(title);
    const String renderedTitle = titleLayout.rightToLeft
        ? ellipsizeRightToLeft(titleLayout.visual, titleWidth)
        : ellipsize(titleLayout.visual, titleWidth);
    canvas().setTextDatum(titleLayout.rightToLeft ? MR_DATUM : ML_DATUM);
    canvas().drawString(renderedTitle.c_str(),
                        titleLayout.rightToLeft ? kTitleRight : titleLeft,
                        kPageHeaderCenterY, headerFont);

    // The accepted reference treatment gives the clock the title's weight and
    // height, and centers every visible element on the same optical line.
    canvas().setTextDatum(MR_DATUM);
    canvas().setTextColor(kClockText);
    canvas().drawString(timeValid ? currentTime : "--:--", 270,
                        kPageHeaderCenterY, headerFont);

    if (uiFrameReady) {
        // The asset's visible y=4..16 pixels center on the shared centerline.
        canvas().drawPng(ui_home_wifi, sizeof(ui_home_wifi), 282,
                         kPageHeaderCenterY - 10);
    } else {
        canvas().drawArc(294, kPageHeaderCenterY - 2, 5, 8, 210, 330, kWhite);
        canvas().drawArc(294, kPageHeaderCenterY - 2, 10, 13, 210, 330, kWhite);
        canvas().fillCircle(294, kPageHeaderCenterY + 4, 2, kWhite);
    }
}

void drawConfigurationBootScreen(const String& ipAddress) {
    drawBackground();

    // The startup handoff is a product welcome screen rather than an ordinary
    // navigable page. It deliberately follows the supplied branded mockup while
    // keeping all live values and the QR code native at 320 x 240.
    if (uiFrameReady) {
        canvas().fillRectAlpha(0, 0, 320, 240, 36, TFT_BLACK);
        canvas().drawPng(ui_home_brand_radio, sizeof(ui_home_brand_radio), 10, 7);
    } else {
        canvas().fillScreen(kNavy);
        canvas().drawRoundRect(12, 15, 15, 10, 3, kHomeText);
        canvas().drawLine(15, 13, 25, 9, kHomeText);
        canvas().fillCircle(17, 20, 2, kHomeText);
        canvas().fillCircle(23, 20, 2, kHomeText);
    }

    canvas().setTextDatum(ML_DATUM);
    canvas().setTextColor(kHomeText);
    canvas().drawString("Radiohead", 36, 16, display_fonts::homeTitle());
    text("INTERNET RADIO", 37, 28, uiFont(&fonts::Font0), kBlueFocus, 130);

    // Center the complete handoff card in the space below the brand subtitle.
    // Its 158 px height leaves matching visual breathing room above and below.
    canvas().fillRoundRect(5, 61, 310, 158, 11, kSurface);
    canvas().drawRoundRect(5, 61, 310, 158, 11, kBlueDark);
    drawConfigurationQrBadge(18, 74, 148);
    canvas().drawFastVLine(163, 73, 134, kSlate);

    text("Set up your radio", 174, 76, uiFont(&fonts::FreeSans9pt7b), kWhite, 132);
    text("Scan QR or open:", 174, 104, uiFont(&fonts::Font0), kTextMuted, 132);
    text(ConfigQrCode::Url, 174, 122, display_fonts::caption(), kBlueFocus, 132);
    text("Or", 174, 157, uiFont(&fonts::Font0), kTextMuted, 132);
    text(ipAddress, 174, 175, uiFont(&fonts::FreeSans9pt7b), kWhite, 132);
}

void drawRecordedProgress(int16_t x, int16_t y, int16_t width, uint32_t elapsed, uint32_t duration) {
    const int16_t fill = duration == 0 ? 0 : std::min<int16_t>(width,
        static_cast<int16_t>((static_cast<uint64_t>(elapsed) * width) / duration));
    canvas().fillRoundRect(x, y, width, 6, 3, kRecordedProgressTrack);
    if (fill > 0) canvas().fillRoundRect(x, y, fill, 6, 3, kBlue);
    canvas().fillCircle(x + fill, y + 3, 4, kWhite);
}

void drawSkipControl(int16_t x, int16_t y, int seconds, bool forward, bool enabled) {
    const uint16_t color = enabled ? kWhite : kTextMuted;
    // Reference-style replay control: one heavy, open circular arrow wrapped
    // around the number.  The arrowhead shares both endpoints with the curve;
    // keeping it as a single silhouette avoids the detached-looking glyph from
    // the earlier thin-arc version on the physical TFT.
    // Outer/inner points form a filled 5px annular stroke.  Filled quads are
    // more stable than overlapping circles on LovyanGFX's RGB565 sprite path.
    constexpr int8_t outer[][2] = {
        {-7, -26}, {5, -23}, {14, -18}, {21, -9}, {24, 1},
        {22, 12}, {16, 21}, {7, 26}, {-4, 27}, {-14, 23},
        {-22, 15}, {-25, 5}, {-24, -5}, {-19, -13}, {-10, -15},
    };
    constexpr int8_t inner[][2] = {
        {-6, -21}, {4, -18}, {11, -14}, {17, -7}, {19, 1},
        {18, 10}, {13, 17}, {6, 21}, {-3, 22}, {-11, 18},
        {-17, 12}, {-20, 4}, {-19, -4}, {-15, -10}, {-8, -11},
    };
    constexpr float kScale = 0.90F;
    const int8_t direction = forward ? -1 : 1;
    for (size_t index = 1; index < sizeof(outer) / sizeof(outer[0]); ++index) {
        const int16_t outerX0 = x + direction * roundf(outer[index - 1][0] * kScale);
        const int16_t outerY0 = y + roundf(outer[index - 1][1] * kScale);
        const int16_t outerX1 = x + direction * roundf(outer[index][0] * kScale);
        const int16_t outerY1 = y + roundf(outer[index][1] * kScale);
        const int16_t innerX0 = x + direction * roundf(inner[index - 1][0] * kScale);
        const int16_t innerY0 = y + roundf(inner[index - 1][1] * kScale);
        const int16_t innerX1 = x + direction * roundf(inner[index][0] * kScale);
        const int16_t innerY1 = y + roundf(inner[index][1] * kScale);
        canvas().fillTriangle(outerX0, outerY0, outerX1, outerY1, innerX1, innerY1, color);
        canvas().fillTriangle(outerX0, outerY0, innerX1, innerY1, innerX0, innerY0, color);
    }
    canvas().fillTriangle(x + direction * roundf(-23 * kScale), y + roundf(-18 * kScale),
                          x + direction * roundf(-6 * kScale), y + roundf(-25 * kScale),
                          x + direction * roundf(-6 * kScale), y + roundf(-11 * kScale), color);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(color);
    canvas().drawString(String(seconds).c_str(), x, y + 8, uiFont(&fonts::FreeSansBold12pt7b));
}

void drawPauseOrPlayControl(int16_t x, int16_t y, bool paused) {
    canvas().fillCircle(x, y, 31, kNavy);
    canvas().drawCircle(x, y, 31, kBlue);
    canvas().drawCircle(x, y, 30, kBlue);
    if (paused) {
        canvas().fillTriangle(x - 8, y - 13, x - 8, y + 13, x + 14, y, kWhite);
    } else {
        canvas().fillRoundRect(x - 14, y - 13, 10, 26, 2, kWhite);
        canvas().fillRoundRect(x + 4, y - 13, 10, 26, 2, kWhite);
    }
}

template<size_t N>
void drawPlayerIcon(const uint8_t (&asset)[N], int16_t x, int16_t y) {
    canvas().drawPng(asset, N, x, y);
}

void drawPodcastArtworkFallback(int16_t x, int16_t y) {
    canvas().fillRoundRect(x, y, 115, 115, 8, kNavy);
    canvas().drawRoundRect(x, y, 115, 115, 8, kTextMuted);
    canvas().fillRoundRect(x + 46, y + 27, 22, 38, 10, kHomeText);
    canvas().drawArc(x + 37, y + 39, 20, 20, 20, 160, kHomeText);
    canvas().drawLine(x + 57, y + 79, x + 57, y + 91, kHomeText);
    canvas().drawLine(x + 43, y + 91, x + 71, y + 91, kHomeText);
}

void drawPodcastBackground() {
    if (!canvas().drawPng(ui_podcast_background, sizeof(ui_podcast_background), 0, 0)) {
        drawBackground();
    }
}

void drawPodcastArtwork(int16_t x, int16_t y) {
    if (uiFrameReady) {
        canvas().drawPng(ui_podcast_artwork_placeholder, sizeof(ui_podcast_artwork_placeholder), x, y);
    } else {
        drawPodcastArtworkFallback(x, y);
    }
}

void textRight(const String& value, int16_t right, int16_t y, const lgfx::IFont* font,
               uint16_t color, int16_t width) {
    canvas().setFont(font);
    canvas().setTextColor(color);
    canvas().setTextDatum(TL_DATUM);
    const UiTextLayout layout = uiTextLayout(value);
    const String rendered = layout.rightToLeft ? ellipsizeRightToLeft(layout.visual, width)
                                               : ellipsize(layout.visual, width);
    canvas().drawString(rendered.c_str(), right - canvas().textWidth(rendered.c_str()), y);
}

void drawSlider(int16_t y) {
    canvas().fillRoundRect(48, y, 214, 8, 4, kSurfaceRaised);
    canvas().fillRoundRect(48, y, mainVal * 214 / 21, 8, 4, kBlue);
    canvas().fillCircle(48 + mainVal * 214 / 21, y + 4, 8, kWhite);
    text(isStationMuted() ? "Muted" : String(mainVal) + " / 21", 270, y - 4, uiFont(&fonts::Font0), kWhite, 46);
}

void drawVolumeOverlay() {
    canvas().fillRoundRect(48, 78, 224, 92, 12, kNavy);
    canvas().drawRoundRect(48, 78, 224, 92, 12, kTextMuted);
    text("Volume", 123, 100, uiFont(&fonts::FreeSans9pt7b), kWhite);
    drawSlider(138);
}

const char* playbackLabel(PlaybackState state) {
    switch (state) {
    case PlaybackState::Connecting: return "CONNECTING";
    case PlaybackState::Playing: return "LIVE";
    case PlaybackState::Failed: return "UNAVAILABLE";
    case PlaybackState::Stopped: return "STOPPED";
    }
    return "";
}

uint16_t playbackColor(PlaybackState state) {
    switch (state) {
    case PlaybackState::Connecting: return kBlue;
    case PlaybackState::Playing: return kRed;
    case PlaybackState::Failed: return kRed;
    case PlaybackState::Stopped: return kSlate;
    }
    return kSlate;
}

String activeStationName() {
    if (podcastMode) {
        return podcastShowTft;
    }
    if (currentStationIdx >= 0 && currentStationIdx < kStationSlotCount) {
        return stations[currentStationIdx].name;
    }
    return "";
}

void renderListening(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader(isAP ? "Setup" : "Live Radio", currentTime, timeValid);
    if (isAP) {
        drawNetworkQrHandoff(false);
    } else {
        const String station = activeStationName();
        drawArtwork(station, 12, 54, 88, kBlue, true, currentStationIdx);
        canvas().fillRoundRect(110, 54, 198, 103, 8, kSurface);
        canvas().fillRoundRect(120, 63, 76, 17, 5, podcastMode ? kGreen : playbackColor(state.playback));
        text(podcastMode ? "SHOW" : playbackLabel(state.playback), 126, 67, uiFont(&fonts::Font0), kWhite, 68);
        text(station, 120, 89, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 164);
        const String detail = state.playback == PlaybackState::Failed
            ? String("Station unavailable")
            : songTitle.isEmpty() ? String("Live radio") : songTitle;
        text(detail, 120, 123, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 164);
        drawStar(286, 75);
        drawTransport(91, 181, "<", false);
        drawTransport(160, 181,
                      state.playback == PlaybackState::Stopped || state.playback == PlaybackState::Failed ? ">" : "[]",
                      true);
        drawTransport(229, 181, ">", false);
        if (state.playerControlFocus && state.playerFocus >= 2 && state.playerFocus <= 4) {
            const int16_t focusX[] = {91, 160, 229};
            canvas().drawCircle(focusX[state.playerFocus - 2], 181, 28, kWhite);
        }
        drawSlider(218);
        if (alarmActive) {
            char alarmText[18];
            snprintf(alarmText, sizeof(alarmText), "Alarm %02d:%02d", alarmH, alarmM);
            text(alarmText, 120, 142, uiFont(&fonts::Font0), kTextMuted, 160);
        }
    }

}

void renderStations(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader("Live Radio", currentTime, timeValid);
    const int count = playableStationCount();
    if (count == 0) {
        text("No stations saved", 28, 88, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Add one at the phone setup page.", 28, 120, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 260);
    }
    for (int row = 0; row < kStationRowsPerPage; ++row) {
        const int visibleIndex = state.stationOffset + row;
        const int y = kStationListTop + row * kStationListRowHeight;
        const int slot = playableStationSlotAt(visibleIndex);
        if (slot < 0) {
            continue;
        }
        const bool focused = visibleIndex == state.stationFocus;
        drawListCard(kListOuterInset, y, focused);
        drawArtwork(stations[slot].name, kListOuterInset + 12, y + 8, 32, focused ? kBlueDark : kSlate, false, slot);
        listPrimaryText(stations[slot].name, kListOuterInset + 54, y, 145);
        drawStar(kListOuterInset + 224, y + 24, isStationFavorite(slot));
        if (focused && state.stationFavoriteFocus) {
            canvas().drawCircle(kListOuterInset + 224, y + 24, 16, kWhite);
        }
    }
    drawListPager(state.stationOffset, count, kStationRowsPerPage);
}

int favoriteStationCount() {
    int count = 0;
    for (int visibleIndex = 0; visibleIndex < playableStationCount(); ++visibleIndex) {
        if (isStationFavorite(playableStationSlotAt(visibleIndex))) ++count;
    }
    return count;
}

int favoriteStationSlotAt(int favoriteIndex) {
    if (favoriteIndex < 0) return -1;
    for (int visibleIndex = 0; visibleIndex < playableStationCount(); ++visibleIndex) {
        const int slot = playableStationSlotAt(visibleIndex);
        if (isStationFavorite(slot) && favoriteIndex-- == 0) return slot;
    }
    return -1;
}

void drawListRow(int16_t y, const String& name, bool focused, bool favorite, bool favoriteFocused) {
    drawListCard(8, y, focused);
    drawArtwork(name, 14, y + 8, 32, focused ? kBlueDark : kSlate, false);
    listPrimaryText(name, 58, y, 145);
    drawStar(232, y + 24, favorite);
    if (favoriteFocused) canvas().drawCircle(232, y + 24, 16, kWhite);
}

void renderFavorites(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader("Favorites", currentTime, timeValid);
    const uint16_t stationTabColor = state.favoriteShowsTab ? kSurfaceRaised : kBlue;
    const uint16_t showTabColor = state.favoriteShowsTab ? kBlue : kSurfaceRaised;
    canvas().fillRoundRect(8, 48, 148, 34, 6, stationTabColor);
    canvas().fillRoundRect(164, 48, 148, 34, 6, showTabColor);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString("Stations", 82, 65, uiFont(&fonts::Font0));
    canvas().drawString("Shows", 238, 65, uiFont(&fonts::Font0));
    if (state.favoriteTabFocus) {
        const int16_t x = state.favoriteShowsTab ? 164 : 8;
        canvas().drawRoundRect(x - 1, 47, 150, 36, 7, kWhite);
    }

    if (state.favoriteShowsTab) {
        int count = 0;
        for (int show = 0; show < PODCAST_SHOW_COUNT; ++show) {
            if (!isPodcastShowFavorite(show)) continue;
            if (count >= state.favoriteOffset && count < state.favoriteOffset + kFavoriteRowsPerPage) {
                const int row = count - state.favoriteOffset;
                drawListRow(kFavoriteListTop + row * kFavoriteListRowHeight,
                            podcastShows[show].webName, false, true, false);
            }
            ++count;
        }
        if (count == 0) {
            text("No favorite shows", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
            text("Use a show's star to add one.", 28, 140, uiFont(&fonts::Font0), kTextMuted, 260);
        }
        drawListPager(state.favoriteOffset, count, kFavoriteRowsPerPage,
                      kFavoriteListTop, kFavoriteListBottom, false);
        return;
    }

    const int count = favoriteStationCount();
    if (count == 0) {
        text("No favorite stations", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Use a station star to add one.", 28, 140, uiFont(&fonts::Font0), kTextMuted, 260);
    }
    for (int row = 0; row < kFavoriteRowsPerPage; ++row) {
        const int favoriteIndex = state.favoriteOffset + row;
        const int slot = favoriteStationSlotAt(favoriteIndex);
        if (slot < 0) continue;
        const bool bodyFocused = state.favoriteFocus == favoriteIndex * 2;
        const bool starFocused = state.favoriteFocus == favoriteIndex * 2 + 1;
        drawListRow(kFavoriteListTop + row * kFavoriteListRowHeight, stations[slot].name,
                    bodyFocused || starFocused, true, starFocused);
    }
    drawListPager(state.favoriteOffset, count, kFavoriteRowsPerPage,
                  kFavoriteListTop, kFavoriteListBottom, false);
}

String durationLabel(uint32_t seconds) {
    if (seconds == 0) return "--:--";
    char value[12];
    snprintf(value, sizeof(value), "%lu:%02lu", static_cast<unsigned long>(seconds / 60),
             static_cast<unsigned long>(seconds % 60));
    return String(value);
}

void renderRecordedShows(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader(state.showFavoritesOnly ? "Favorite Shows" : "Recorded Shows",
                   currentTime, timeValid);
    int drawn = 0;
    for (int index = 0; index < PODCAST_SHOW_COUNT; ++index) {
        if (state.showFavoritesOnly && !isPodcastShowFavorite(index)) continue;
        if (drawn < state.showOffset) { ++drawn; continue; }
        const int row = drawn - state.showOffset;
        if (row >= kStationRowsPerPage) break;
        const bool focused = drawn == state.showFocus;
        const int y = kStationListTop + row * kStationListRowHeight;
        drawListCard(kListOuterInset, y, focused);
        drawArtwork(podcastShows[index].tftName, kListOuterInset + 10, y + 8, 32, focused ? kBlueDark : kGreen, false);
        listPrimaryText(podcastShows[index].webName, kListOuterInset + 54, y, 160);
        drawStar(kListOuterInset + 230, y + 24, isPodcastShowFavorite(index));
        ++drawn;
    }
    if (state.showFavoritesOnly && drawn == 0) {
        text("No favorite shows", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Use a show's star to add one.", 28, 140, uiFont(&fonts::Font0), kTextMuted, 260);
    }
    int count = 0;
    for (int index = 0; index < PODCAST_SHOW_COUNT; ++index) {
        if (!state.showFavoritesOnly || isPodcastShowFavorite(index)) ++count;
    }
    drawListPager(state.showOffset, count, kStationRowsPerPage);
}

void renderShowEpisodes(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    const bool validShow = state.episodeShow >= 0 && state.episodeShow < PODCAST_SHOW_COUNT;
    drawPageHeader(validShow ? podcastShows[state.episodeShow].tftName : "Episodes",
                   currentTime, timeValid);
    if (!validShow) return;
    if (podcastRequestedShow() == state.episodeShow && podcastLoadState() == PodcastLoadState::Loading) {
        text("Loading episodes…", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Back remains available.", 28, 140, uiFont(&fonts::Font0), kTextMuted, 260);
        return;
    }
    if (!podcastEpisodesReadyFor(state.episodeShow)) {
        text("Episodes unavailable", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Go back, then select the show to retry.", 28, 140, uiFont(&fonts::Font0), kTextMuted, 260);
        return;
    }
    for (int row = 0; row < kStationRowsPerPage; ++row) {
        const int index = state.episodeOffset + row;
        if (index >= podcastEpisodeCount) break;
        const int y = kStationListTop + row * kStationListRowHeight;
        const bool focused = index == state.episodeFocus;
        drawListCard(kListOuterInset, y, focused);
        canvas().drawCircle(kListOuterInset + 21, y + 24, 11, kWhite);
        canvas().fillTriangle(kListOuterInset + 18, y + 19, kListOuterInset + 18, y + 29,
                              kListOuterInset + 26, y + 24, kWhite);
        listPrimaryText(podcastEpisodes[index].title, kListOuterInset + 40, y, 165);
    }
    drawListPager(state.episodeOffset, podcastEpisodeCount, kStationRowsPerPage);
}

void renderPodcastPlayer(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawPodcastBackground();
    drawPageHeader("Recorded Show", currentTime, timeValid);
    const PodcastPlaybackSnapshot playback = podcastPlaybackSnapshot();
    const PodcastEpisode* episode = podcastActiveEpisode();
    if (!playback.active || playback.showIndex < 0 || episode == nullptr) {
        text("Episode unavailable", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        return;
    }
    // No show artwork has been supplied in the local catalog, so use the
    // prepared microphone/show placeholder rather than a colored initials tile.
    // Keep metadata RTL/right-aligned to one stable edge beside the 115 px art.
    drawPodcastArtwork(20, 47);
    textRight(episode->title, 305, 53, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 155);
    String published = episode->publishedUtc;
    if (published.length() > 10) published.remove(10);
    textRight(published.isEmpty() ? String("Date unavailable") : published, 305, 88,
              homeCaptionFont(), kTextMuted, 155);
    const uint32_t duration = playback.durationSeconds;
    drawRecordedProgress(150, 126, 150, playback.elapsedSeconds, duration);
    text(durationLabel(playback.elapsedSeconds), 150, 138, uiFont(&fonts::Font0), kWhite, 66);
    const String remainder = duration == 0 ? String("--:--") : durationLabel(duration - std::min(duration, playback.elapsedSeconds));
    textRight("-" + remainder, 300, 138, uiFont(&fonts::Font0), kWhite, 60);
    if (uiFrameReady) {
        drawPlayerIcon(ui_player_rewind_15, 53, 165);
        if (playback.paused) {
            drawPlayerIcon(ui_player_play, 124, 158);
        } else {
            drawPlayerIcon(ui_player_pause, 124, 158);
        }
        drawPlayerIcon(ui_player_forward_30, 209, 165);
    } else {
        // A visible primitive fallback is retained for the no-canvas path;
        // normal rendering always uses the supplied artwork above.
        drawSkipControl(82, 194, 15, false, playback.canSeek);
        drawPauseOrPlayControl(160, 194, playback.paused);
        drawSkipControl(238, 194, 30, true, playback.canSeek);
    }
    // Transport focus is deliberately not an extra white outline: the mockup
    // uses the replay loop and blue primary ring as the visual anchors.
}

void renderStationOptions(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader("Station Options", currentTime, timeValid);
    const int slot = state.optionStation;
    if (slot < 0 || slot >= STATION_COUNT || stations[slot].url.isEmpty()) {
        text("No station selected", 28, 92, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        footerButton(0, 320, "Back");
        return;
    }
    const char* labels[] = {
        isStationFavorite(slot) ? "Remove Favorite" : "Add Favorite",
        "Station Information",
        "Back",
    };
    for (int row = 0; row < 3; ++row) {
        const int y = 44 + row * 48;
        const bool focused = state.optionsFocus == row;
        drawListCard(8, y + 3, focused, true);
        text(labels[row], 28, y + 14, uiFont(&fonts::FreeSans9pt7b), kWhite, 238);
        if (row == 0) drawStar(290, y + 24, isStationFavorite(slot));
        else if (row == 1) {
            canvas().drawCircle(290, y + 24, 11, kWhite);
            text("i", 287, y + 16, uiFont(&fonts::FreeSans9pt7b), kWhite, 10);
        }
    }
}

void renderStationInfo(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader("Station Information", currentTime, timeValid);
    const int slot = state.optionStation;
    if (slot >= 0 && slot < STATION_COUNT && !stations[slot].url.isEmpty()) {
        drawArtwork(stations[slot].name, 16, 58, 64, kBlue, true, slot);
        text(stations[slot].name, 94, 66, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 200);
        text("No verified catalog information", 20, 142, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 280);
        text("is available for this station.", 20, 164, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 280);
    } else {
        text("No station selected", 28, 92, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
    }
    footerButton(0, 320, "Back");
}

void drawSettingsGlyph(uint8_t kind, int16_t x, int16_t y, uint16_t color = kWhite) {
    switch (kind) {
    case 0:  // Network
        // Use the same prepared Wi-Fi mark as the page headers. This keeps the
        // Settings destination immediately recognizable and avoids a second,
        // slightly different network symbol.
        if (uiFrameReady) {
            canvas().drawPng(ui_home_wifi, sizeof(ui_home_wifi), x - 12, y - 10);
        } else {
            canvas().drawArc(x, y - 2, 7, 10, 210, 330, color);
            canvas().drawArc(x, y - 2, 13, 16, 210, 330, color);
            canvas().fillCircle(x, y + 5, 2, color);
        }
        break;
    case 1:  // Display
        canvas().drawRoundRect(x - 13, y - 9, 26, 18, 3, color);
        canvas().drawLine(x - 5, y + 13, x + 5, y + 13, color);
        canvas().drawLine(x, y + 9, x, y + 13, color);
        break;
    case 2:  // Audio
        // A conventional right-facing speaker reads cleanly at this size. The
        // horn now points toward its sound waves instead of away from them.
        canvas().fillRect(x - 13, y - 5, 6, 10, color);
        canvas().fillTriangle(x - 7, y - 9, x - 7, y + 9, x + 2, y, color);
        canvas().drawArc(x + 2, y, 7, 10, 300, 60, color);
        canvas().drawArc(x + 2, y, 12, 15, 300, 60, color);
        break;
    case 3:  // Weather & time
        canvas().drawCircle(x - 5, y - 4, 6, color);
        canvas().fillCircle(x + 5, y + 4, 7, color);
        canvas().fillCircle(x - 3, y + 5, 6, color);
        break;
    default:  // Device
        canvas().drawCircle(x, y, 13, color);
        canvas().fillCircle(x, y - 6, 2, color);
        canvas().fillRect(x - 1, y - 1, 3, 9, color);
        break;
    }
}

void drawSettingsChevron(int16_t x, int16_t y, uint16_t color = kWhite) {
    for (int16_t offset = -1; offset <= 1; ++offset) {
        canvas().drawLine(x - 5, y - 9 + offset, x + 4, y + offset, color);
        canvas().drawLine(x + 4, y + offset, x - 5, y + 9 + offset, color);
    }
}

void drawSettingsListRow(int16_t y, const char* label, uint8_t icon, bool danger = false) {
    drawListCard(kListOuterInset, y, false);
    drawSettingsGlyph(icon, kListOuterInset + 22, y + 23, danger ? kRed : kWhite);
    listPrimaryText(label, kListOuterInset + 50, y, 150);
    drawSettingsChevron(kListOuterInset + 226, y + 23, danger ? kRed : kWhite);
}

// Audio rows use the full-width surface, with equal eight-pixel side margins.
// The 44-pixel +/- targets are shared with hit testing below.
constexpr int16_t kToneMinusX = 204;
constexpr int16_t kTonePlusX = 260;
constexpr int16_t kToneButtonWidth = 44;

// The shared wide-card artwork is 42 px high. These origins center the visible
// two-line glyph block (the fonts' ink sits below its nominal origin), leaving
// equal visual breathing room above and below. Keep this local to Firmware
// updates until it has been checked on the physical panel.
constexpr int16_t kFirmwareCardPrimaryTextInset = 4;
constexpr int16_t kFirmwareCardSecondaryTextInset = 22;

void drawEditorControl(int16_t y, const char* label, int value) {
    drawListCard(8, y + 2, false, true);
    listPrimaryText(label, 24, y, 104);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(String(value).c_str(), 166, y + 23, uiFont(&fonts::FreeSans9pt7b));
    canvas().fillRoundRect(kToneMinusX + 4, y + 5, kToneButtonWidth - 8, 36, 6, kSlate);
    canvas().fillRoundRect(kTonePlusX + 4, y + 5, kToneButtonWidth - 8, 36, 6, kBlue);
    canvas().drawString("-", kToneMinusX + kToneButtonWidth / 2, y + 23, uiFont(&fonts::FreeSans9pt7b));
    canvas().drawString("+", kTonePlusX + kToneButtonWidth / 2, y + 23, uiFont(&fonts::FreeSans9pt7b));
}

void drawEditorFooter() {
    canvas().fillRoundRect(16, 190, 136, 40, 6, kSlate);
    canvas().fillRoundRect(168, 190, 136, 40, 6, kBlue);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString("Cancel", 84, 210, uiFont(&fonts::Font0));
    canvas().drawString("Save", 236, 210, uiFont(&fonts::Font0));
}

void renderSettings(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawSettingsBackground();
    static constexpr const char* kLabels[] = {
        "Wi-Fi", "Display", "Audio", "Weather & Time", "Device",
    };
    for (int row = 0; row < kStationRowsPerPage; ++row) {
        const int item = state.settingsOffset + row;
        if (item >= static_cast<int>(sizeof(kLabels) / sizeof(kLabels[0]))) break;
        drawSettingsListRow(kStationListTop + row * kStationListRowHeight,
                            kLabels[item], static_cast<uint8_t>(item));
    }
    drawListPager(state.settingsOffset, sizeof(kLabels) / sizeof(kLabels[0]), kStationRowsPerPage);
    drawPageHeader("Settings", currentTime, timeValid);
}

void renderToneSettings(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawSettingsBackground();
    drawEditorControl(44, "Bass", state.toneBassDraft);
    drawEditorControl(92, "Mid", state.toneMidDraft);
    drawEditorControl(140, "Treble", state.toneTrebleDraft);
    drawEditorFooter();
    drawPageHeader("Audio", currentTime, timeValid);
}

void renderDisplaySettings(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawSettingsBackground();
    drawListCard(8, 68, false, true);
    text("Auto dimming", 24, 79, uiFont(&fonts::FreeSans9pt7b), kWhite, 124);
    const String dimLabel = String(state.dimSecondsDraft) + " sec";
    text(dimLabel, 152, 79, uiFont(&fonts::FreeSans9pt7b), kWhite, 66);
    canvas().fillRoundRect(222, 71, 36, 36, 6, kSlate);
    canvas().fillRoundRect(272, 71, 36, 36, 6, kBlue);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString("-", 240, 89, uiFont(&fonts::FreeSans9pt7b));
    canvas().drawString("+", 290, 89, uiFont(&fonts::FreeSans9pt7b));
    text("The screen wakes on touch or control input.", 24, 132,
         uiFont(&fonts::Font0), kTextMuted, 250);
    drawEditorFooter();
    drawPageHeader("Display", currentTime, timeValid);
}

void renderDeviceSettings(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawSettingsBackground();
    drawSettingsListRow(44, "Firmware updates", 4);
    drawSettingsListRow(92, "Touch calibration", 4);
    drawSettingsListRow(140, "Restart", 4);
    drawSettingsListRow(188, state.deviceActionFailed ? "Reset failed" : "Factory reset", 4, true);
    drawListPager(0, 4, kStationRowsPerPage);
    drawPageHeader("Device", currentTime, timeValid);
}

String firmwareUpdateStatusLabel(const FirmwareUpdater::Snapshot& update) {
    switch (update.status) {
    case FirmwareUpdater::Status::Checking:
        return "Checking GitHub releases...";
    case FirmwareUpdater::Status::UpToDate:
        return "Up to date";
    case FirmwareUpdater::Status::UpdateFound:
        if (!update.availableVersion.isEmpty()) {
            return String("Update available: ") + update.availableVersion;
        }
        return "Update available";
    case FirmwareUpdater::Status::Downloading:
        return "Downloading update...";
    case FirmwareUpdater::Status::Installed:
        return "Installed; restarting...";
    case FirmwareUpdater::Status::Failed:
        return "Update failed - tap to retry";
    case FirmwareUpdater::Status::Idle:
    default:
        return "No update check yet";
    }
}

void renderFirmwareSettings(const UiRenderState& state, const char* currentTime, bool timeValid) {
    (void)state;
    const FirmwareUpdater::Snapshot update = firmwareUpdater.snapshot();
    drawSettingsBackground();

    drawListCard(8, 52, false, true);
    text("Check for updates", 24, 52 + kFirmwareCardPrimaryTextInset,
         uiFont(&fonts::FreeSans9pt7b), kWhite, 246);
    text(firmwareUpdateStatusLabel(update), 24, 52 + kFirmwareCardSecondaryTextInset,
         uiFont(&fonts::Font0),
         update.status == FirmwareUpdater::Status::Failed ? kAmber : kTextMuted, 264);

    drawListCard(8, 108, false, true);
    text("Update mode", 24, 108 + kFirmwareCardPrimaryTextInset,
         uiFont(&fonts::FreeSans9pt7b), kWhite, 120);
    canvas().setTextDatum(MR_DATUM);
    canvas().setTextColor(update.autoInstall ? kBlueFocus : kAmber);
    canvas().drawString(update.autoInstall ? "Automatic" : "Manual", 294, 129,
                        uiFont(&fonts::FreeSans9pt7b));
    text("Tap to switch", 24, 108 + kFirmwareCardSecondaryTextInset,
         uiFont(&fonts::Font0), kTextMuted, 246);

    if (update.awaitingConfirmation) {
        drawListCard(8, 164, true, true);
        text("Update now", 24, 164 + kFirmwareCardPrimaryTextInset,
             uiFont(&fonts::FreeSans9pt7b), kWhite, 246);
        const String release = update.availableVersion.isEmpty()
            ? String("Download and install the checked release")
            : String("Download and install ") + update.availableVersion;
        text(release, 24, 164 + kFirmwareCardSecondaryTextInset,
             uiFont(&fonts::Font0), kWhite, 264);
    }
    drawPageHeader("Firmware updates", currentTime, timeValid);
}

void renderSettingsWebHandoff(const UiRenderState& state, const char* currentTime, bool timeValid) {
    const bool network = state.settingsWebHandoff == 0;
    drawSettingsBackground();
    if (network) {
        drawNetworkQrHandoff(!isAP);
        drawPageHeader("Network", currentTime, timeValid);
        return;
    }
    drawListCard(kListOuterInset, 66, false, true);
    text(network ? "Configure Wi-Fi on your phone." : "Configure location and time on your phone.",
         24, 82, uiFont(&fonts::FreeSans9pt7b), kWhite, 260);
    const String address = isAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    text(String("Open ") + address, 24, 112, uiFont(&fonts::FreeSans9pt7b), kBlueFocus, 260);
    text("Passwords and API keys stay off this screen.", 24, 142,
         uiFont(&fonts::Font0), kTextMuted, 260);
    footerButton(0, 320, "Back");
    drawPageHeader("Weather & Time", currentTime, timeValid);
}

void renderSettingsConfirmation(const UiRenderState& state, const char* currentTime, bool timeValid) {
    renderSettings(state, currentTime, timeValid);
    const bool factoryReset = state.settingsConfirmAction == 2;
    canvas().fillRoundRect(27, 54, 266, 132, 12, kWhite);
    text(factoryReset ? "Factory reset radio?" : "Restart radio?", 48, 76,
         uiFont(&fonts::FreeSans9pt7b), kNavy, 226);
    text(factoryReset ? "This removes settings and stations." : "Playback will stop briefly.",
         48, 110, uiFont(&fonts::Font0), kSurfaceRaised, 226);
    text(factoryReset ? "Touch calibration is kept." : "Your settings are kept.",
         48, 130, uiFont(&fonts::Font0), kSurfaceRaised, 226);
    canvas().fillRoundRect(45, 142, 104, 36, 6, kSlate);
    canvas().fillRoundRect(171, 142, 104, 36, 6, factoryReset ? kRed : kBlue);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString("Cancel", 97, 160, uiFont(&fonts::Font0));
    canvas().drawString(factoryReset ? "Reset" : "Restart", 223, 160, uiFont(&fonts::Font0));
}

void renderConfirm(const UiRenderState& state, const char* currentTime, bool timeValid) {
    renderListening({}, currentTime, timeValid);
    canvas().fillRoundRect(27, 56, 266, 128, 12, kWhite);
    text("Enter standby?", 54, 80, uiFont(&fonts::FreeSansBold12pt7b), kNavy, 210);
    text("Audio stops until K0 or alarm wake.", 54, 113, uiFont(&fonts::Font0), kSurfaceRaised, 210);
    canvas().fillRoundRect(45, 132, 104, 40, 6, kSlate);
    canvas().fillRoundRect(171, 132, 104, 40, 6, kRed);
    if (state.confirmAcceptFocused) {
        canvas().drawRoundRect(169, 130, 108, 44, 7, kNavy);
    } else {
        canvas().drawRoundRect(43, 130, 108, 44, 7, kNavy);
    }
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString("Cancel", 97, 150, uiFont(&fonts::Font0));
    canvas().drawString("Standby", 223, 150, uiFont(&fonts::Font0));
}

void drawHomeTile(int16_t x, uint16_t color, const char* top, const char* bottom, uint8_t tile) {
    if (uiFrameReady) {
        switch (tile) {
        case 0: drawHomeAsset(ui_home_tile_radio, x, kHomeTileY); break;
        case 1: drawHomeAsset(ui_home_tile_shows, x, kHomeTileY); break;
        case 2: drawHomeAsset(ui_home_tile_favorites, x, kHomeTileY); break;
        case 3: drawHomeAsset(ui_home_tile_settings, x, kHomeTileY); break;
        }
    } else {
        canvas().fillRoundRect(x, kHomeTileY, kHomeTileWidth, kHomeTileHeight, 8, color);
    }
    const uint8_t icon = tile < 4 ? tile : 0;
    const int16_t iconY = kHomeTileIconVisibleTop - kHomeTileIconTransparentTop[icon];
    drawHomeTileIcon(x + (kHomeTileWidth - kHomeTileIconSize) / 2, iconY, icon);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kHomeText);
    canvas().drawString(top, x + kHomeTileWidth / 2, kHomeTileY + (bottom == nullptr ? 52 : 46), homeLabelFont());
    if (bottom != nullptr) canvas().drawString(bottom, x + kHomeTileWidth / 2, kHomeTileY + 58, homeLabelFont());
    serviceUiAudio();
}

void renderHome(const UiRenderState& state, const char* currentTime, bool timeValid) {
    if (!canvas().drawPng(ui_home_background, sizeof(ui_home_background), 0, 0)) {
        canvas().fillScreen(kNavy);
    }
    serviceUiAudio();
    String station;
    if (state.homeStationPreview >= 0 && state.homeStationPreview < STATION_COUNT) {
        station = stations[state.homeStationPreview].name;
    } else if (podcastMode) {
        station = podcastShowTft;
    } else if (currentStationIdx >= 0 && currentStationIdx < STATION_COUNT) {
        station = stations[currentStationIdx].name;
    }
    drawHomeHeader(timeValid, station);
    float temperature = 0.0F;
    int condition = 0;
    bool hasWeather = false;
    portENTER_CRITICAL(&weatherStateMux);
    temperature = tempC;
    condition = weatherID;
    hasWeather = weatherDataValid && millis() - weatherLastSuccessAt <= WEATHER_STALE_AFTER_MS;
    portEXIT_CRITICAL(&weatherStateMux);

    // Home deliberately leaves the supplied sunset visible.  It is the primary
    // composition layer; only dense pages receive opaque reading surfaces.
    // Visibility is a committed setting shared with the web UI, not a browser-
    // only preference.  Hidden weather leaves the photograph untouched.
    if (showWeatherOnHome && hasWeather) {
        drawHomeWeatherIcon(9, 68, true, condition);
        char temperatureText[12];
        const float displayedTemperature = useCelsius ? temperature : temperature * 9.0F / 5.0F + 32.0F;
        snprintf(temperatureText, sizeof(temperatureText), "%d*", static_cast<int>(roundf(displayedTemperature)));
        drawHomeTemperatureAtlas(temperatureText);
        text(homeCityLabel(owmCity), kHomeWeatherTextLeft, kHomeWeatherCityTop,
             homeCaptionFont(), kWhite, 106);
        text(homeWeatherDescription(condition), kHomeWeatherTextLeft,
             kHomeWeatherConditionTop, homeCaptionFont(), kWhite, 106);
    }

    // Manual-update mode needs an on-device prompt as well as the browser's
    // Install button; otherwise a release can wait forever unnoticed.
    if (firmwareUpdater.snapshot().awaitingConfirmation) {
        canvas().fillRoundRect(96, 124, 216, 18, 5, kAmber);
        canvas().setTextDatum(MC_DATUM);
        canvas().setTextColor(kNavy);
        canvas().drawString("UPDATE AVAILABLE", 204, 133, uiFont(&fonts::Font0));
    }

    // One renderer owns the Home clock in both PSRAM and direct-TFT modes.
    // A user-selected 12-hour value remains supported because the atlas is
    // laid out from the actual digits, not a fixed 24-hour string width.
    drawHomeClockAtlas(timeValid ? currentTime : "--:--");
    // A strict 4-column grid: identical tiles, wider breathing gaps and one baseline.
    drawHomeTile(kHomeTileX[0], kHomeBlue, "Live Radio", nullptr, 0);
    drawHomeTile(kHomeTileX[1], kHomeGreen, "Recorded", "Shows", 1);
    drawHomeTile(kHomeTileX[2], kHomePurple, "Favorites", nullptr, 2);
    drawHomeTile(kHomeTileX[3], kHomeSlate, "Settings", nullptr, 3);
    const uint8_t focus = state.homeFocus < 4 ? state.homeFocus : 0;
    if (uiFrameReady) {
        drawHomeAsset(ui_home_focus, kHomeTileX[focus] - 2, kHomeTileY - 2);
    } else {
        canvas().drawRoundRect(kHomeTileX[focus] - 2, kHomeTileY - 2, kHomeTileWidth + 4, kHomeTileHeight + 4, 9, kBlueFocus);
    }
}

void renderUnavailable(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawPageHeader("Not available", currentTime, timeValid);
    canvas().fillRoundRect(20, 76, 280, 88, 10, kSurface);
    const char* destination = "This destination is coming later.";
    switch (state.unavailableDestination) {
    case 1: destination = "Recorded Shows are coming later."; break;
    case 2: destination = "Favorites are coming later."; break;
    case 3: destination = "Settings are coming later."; break;
    case 4: destination = "Station options are coming later."; break;
    default: break;
    }
    text(destination, 40, 100, uiFont(&fonts::FreeSans9pt7b), kWhite, 230);
    text("Use Back to return home.", 40, 132, uiFont(&fonts::Font0), kTextMuted, 230);
}

void renderP2Fixture(const UiRenderState& state, const char* currentTime, bool timeValid) {
    switch (UI_P2_FIXTURE) {
    case 1:
        renderHome(state, currentTime, timeValid);
        break;
    case 2:
        renderStations(state, currentTime, timeValid);
        break;
    case 3:
        renderListening(state, currentTime, timeValid);
        break;
    case 11: {
        UiRenderState overlayState = state;
        overlayState.volumeOverlay = true;
        renderListening(overlayState, currentTime, timeValid);
        break;
    }
    case 16:
        renderConfirm(state, currentTime, timeValid);
        break;
    default:
        renderHome(state, currentTime, timeValid);
        break;
    }
}

}  // namespace

bool homeStationTitleRefreshDue(unsigned long now) {
    return homeStationTitleMarquee.overflows &&
        static_cast<long>(now - homeStationTitleMarquee.nextRefreshAt) >= 0;
}

void renderHomeStationTitleTick() {
    if (!homeStationTitleMarquee.overflows) return;
    drawHomeStationTitle(millis());
    presentCanvas(kHomeStationTitleTop, kHomeStationTitleHeight);
}

UiTarget uiHitTest(const UiRenderState& state, int16_t x, int16_t y) {
    if (state.page == UiPage::Settings &&
        contains(x, y, kListRailLeft, kStationListTop, kListRailWidth, kListRailHeight)) {
        return y < kStationListTop + kListRailHeight / 2 ? UiTarget::SettingsPrevious : UiTarget::SettingsNext;
    }
    if ((state.page == UiPage::Stations || state.page == UiPage::RecordedShows ||
         state.page == UiPage::ShowEpisodes) &&
        contains(x, y, kListRailLeft, kStationListTop, kListRailWidth, kListRailHeight)) {
        return y < kStationListTop + kListRailHeight / 2 ? UiTarget::ListPrevious : UiTarget::ListNext;
    }
    if (state.page == UiPage::Home) {
        if (contains(x, y, kHomeTileX[0], kHomeTileY, kHomeTileWidth, kHomeTileHeight)) return UiTarget::HomeLiveRadio;
        if (contains(x, y, kHomeTileX[1], kHomeTileY, kHomeTileWidth, kHomeTileHeight)) return UiTarget::HomeRecordedShows;
        if (contains(x, y, kHomeTileX[2], kHomeTileY, kHomeTileWidth, kHomeTileHeight)) return UiTarget::HomeFavorites;
        if (contains(x, y, kHomeTileX[3], kHomeTileY, kHomeTileWidth, kHomeTileHeight)) return UiTarget::HomeSettings;
    } else if (state.page == UiPage::Listening) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::PlayerBack;
        if (contains(x, y, 0, 44, 110, 96) || contains(x, y, 264, 44, 56, 96)) return UiTarget::PlayerOptions;
        if (contains(x, y, 52, 140, 48, 48)) return UiTarget::PlayerPrevious;
        if (contains(x, y, 136, 140, 48, 48)) return UiTarget::PlayerStopOrPlay;
        if (contains(x, y, 220, 140, 48, 48)) return UiTarget::PlayerNext;
        if (contains(x, y, 0, 188, 48, 52)) return UiTarget::ListeningMute;
        if (contains(x, y, 48, 188, 214, 52)) return UiTarget::ListeningVolume;
    } else if (state.page == UiPage::Stations) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::ListBack;
        const int row = listRowAt(y);
        if (row >= 0 && x >= kListOuterInset + 204) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::ListRowFavorite0) +
                row);
        }
        if (row >= 0) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::ListRow0) +
                row);
        }
    } else if (state.page == UiPage::StationOptions) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::OptionsBack;
        if (contains(x, y, 0, 44, 320, 48)) return UiTarget::OptionsFavorite;
        if (contains(x, y, 0, 92, 320, 48)) return UiTarget::OptionsInfo;
        if (contains(x, y, 0, 140, 320, 48)) return UiTarget::OptionsBack;
    } else if (state.page == UiPage::StationInfo) {
        if (contains(x, y, 0, 188, 320, 52) ||
            contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::InfoBack;
    } else if (state.page == UiPage::Favorites) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::FavoritesBack;
        if (contains(x, y, 8, 48, 148, 34)) return UiTarget::FavoritesStationsTab;
        if (contains(x, y, 164, 48, 148, 34)) return UiTarget::FavoritesShowsTab;
        if (contains(x, y, kListRailLeft, kFavoriteListTop, kListRailWidth,
                     kFavoriteListBottom - kFavoriteListTop)) {
            return y < kFavoriteListTop + (kFavoriteListBottom - kFavoriteListTop) / 2
                ? UiTarget::FavoritesPrevious : UiTarget::FavoritesNext;
        }
        if (!state.favoriteShowsTab && y >= kFavoriteListTop && y < kFavoriteListBottom && x >= 212) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::FavoritesRowFavorite0) +
                (y - kFavoriteListTop) / kFavoriteListRowHeight);
        }
        if (y >= kFavoriteListTop && y < kFavoriteListBottom) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::FavoritesRow0) +
                (y - kFavoriteListTop) / kFavoriteListRowHeight);
        }
    } else if (state.page == UiPage::RecordedShows) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::ShowsBack;
        const int row = listRowAt(y);
        if (row >= 0 && x >= kListOuterInset + 204) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::ShowRowFavorite0) + row);
        }
        if (row >= 0) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::ShowRow0) +
                row);
        }
    } else if (state.page == UiPage::ShowEpisodes) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::EpisodesBack;
        const int row = listRowAt(y);
        if (row >= 0) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::EpisodeRow0) +
                row);
        }
    } else if (state.page == UiPage::PodcastPlayer) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::PodcastBack;
        // The wider target makes the 8px track comfortable to tap without
        // stealing the header/back affordance.
        if (contains(x, y, 146, 118, 158, 40)) return UiTarget::PodcastProgress;
        if (contains(x, y, 58, 170, 48, 48)) return UiTarget::PodcastSeekBack;
        if (contains(x, y, 136, 170, 48, 48)) return UiTarget::PodcastPause;
        if (contains(x, y, 214, 170, 48, 48)) return UiTarget::PodcastSeekForward;
    } else if (state.page == UiPage::StandbyConfirm) {
        if (contains(x, y, 38, 124, 110, 44)) return UiTarget::ConfirmCancel;
        if (contains(x, y, 172, 124, 110, 44)) return UiTarget::ConfirmStandby;
    } else if (state.page == UiPage::Settings) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::SettingsBack;
        const int row = listRowAt(y);
        if (row >= 0 && x < kListRailLeft) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::SettingsRow0) + row);
        }
    } else if (state.page == UiPage::SettingsAudio) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::SettingsBack;
        if (x >= kToneMinusX && x < kToneMinusX + kToneButtonWidth) {
            if (y >= 44 && y < 90) return UiTarget::ToneBassDecrease;
            if (y >= 92 && y < 138) return UiTarget::ToneMidDecrease;
            if (y >= 140 && y < 186) return UiTarget::ToneTrebleDecrease;
        }
        if (x >= kTonePlusX && x < kTonePlusX + kToneButtonWidth) {
            if (y >= 44 && y < 90) return UiTarget::ToneBassIncrease;
            if (y >= 92 && y < 138) return UiTarget::ToneMidIncrease;
            if (y >= 140 && y < 186) return UiTarget::ToneTrebleIncrease;
        }
        if (contains(x, y, 16, 190, 136, 40)) return UiTarget::ToneCancel;
        if (contains(x, y, 168, 190, 136, 40)) return UiTarget::ToneSave;
    } else if (state.page == UiPage::SettingsDisplay) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::SettingsBack;
        if (contains(x, y, 218, 64, 44, 52)) return UiTarget::DimDecrease;
        if (contains(x, y, 268, 64, 44, 52)) return UiTarget::DimIncrease;
        if (contains(x, y, 16, 190, 136, 40)) return UiTarget::DimCancel;
        if (contains(x, y, 168, 190, 136, 40)) return UiTarget::DimSave;
    } else if (state.page == UiPage::SettingsDevice) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::SettingsBack;
        const int row = listRowAt(y);
        if (row == 0) return UiTarget::DeviceFirmware;
        if (row == 1) return UiTarget::DeviceCalibration;
        if (row == 2) return UiTarget::DeviceRestart;
        if (row == 3) return UiTarget::DeviceFactoryReset;
    } else if (state.page == UiPage::SettingsFirmware) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) return UiTarget::SettingsBack;
        if (contains(x, y, 8, 52, 304, 48)) return UiTarget::FirmwareCheckNow;
        if (contains(x, y, 8, 108, 304, 48)) return UiTarget::FirmwareToggleAutoInstall;
        if (contains(x, y, 8, 164, 304, 48) &&
            firmwareUpdater.snapshot().awaitingConfirmation) return UiTarget::FirmwareInstallNow;
    } else if (state.page == UiPage::SettingsWebHandoff) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight) ||
            contains(x, y, 0, 188, 320, 52)) return UiTarget::SettingsBack;
    } else if (state.page == UiPage::SettingsConfirm) {
        if (contains(x, y, 0, 0, kHeaderBackHitWidth, kHeaderBackHitHeight)) {
            return UiTarget::SettingsBack;
        }
        if (contains(x, y, 45, 142, 104, 36)) return UiTarget::SettingsConfirmCancel;
        if (contains(x, y, 171, 142, 104, 36)) return UiTarget::SettingsConfirmAccept;
    } else if (state.page == UiPage::Unavailable) {
        if (contains(x, y, 0, 0, 320, 240)) return UiTarget::ListBack;
    }
    return UiTarget::None;
}

void renderRadioUi(const UiRenderState& state, const char* currentTime, bool timeValid) {
    initCanvas();
#if UI_P2_FIXTURE
    renderP2Fixture(state, currentTime, timeValid);
#else
    switch (state.page) {
    case UiPage::Home:
        renderHome(state, currentTime, timeValid);
        break;
    case UiPage::Listening:
        renderListening(state, currentTime, timeValid);
        break;
    case UiPage::Stations:
        renderStations(state, currentTime, timeValid);
        break;
    case UiPage::StationOptions:
        renderStationOptions(state, currentTime, timeValid);
        break;
    case UiPage::StationInfo:
        renderStationInfo(state, currentTime, timeValid);
        break;
    case UiPage::Favorites:
        renderFavorites(state, currentTime, timeValid);
        break;
    case UiPage::RecordedShows:
        renderRecordedShows(state, currentTime, timeValid);
        break;
    case UiPage::ShowEpisodes:
        renderShowEpisodes(state, currentTime, timeValid);
        break;
    case UiPage::PodcastPlayer:
        renderPodcastPlayer(state, currentTime, timeValid);
        break;
    case UiPage::StandbyConfirm:
        renderConfirm(state, currentTime, timeValid);
        break;
    case UiPage::Settings:
        renderSettings(state, currentTime, timeValid);
        break;
    case UiPage::SettingsAudio:
        renderToneSettings(state, currentTime, timeValid);
        break;
    case UiPage::SettingsDisplay:
        renderDisplaySettings(state, currentTime, timeValid);
        break;
    case UiPage::SettingsDevice:
        renderDeviceSettings(state, currentTime, timeValid);
        break;
    case UiPage::SettingsFirmware:
        renderFirmwareSettings(state, currentTime, timeValid);
        break;
    case UiPage::SettingsWebHandoff:
        renderSettingsWebHandoff(state, currentTime, timeValid);
        break;
    case UiPage::SettingsConfirm:
        renderSettingsConfirmation(state, currentTime, timeValid);
        break;
    case UiPage::Unavailable:
        renderUnavailable(state, currentTime, timeValid);
        break;
    }
#endif
    // Encoder feedback remains visible without changing focus, even when the
    // user is browsing a list or a podcast screen. The OTA overlay below stays
    // on top while firmware is being written.
    if (state.volumeOverlay) drawVolumeOverlay();
    if (firmwareUpdateOverlayActive) {
        canvas().fillScreen(kNavy);
        canvas().setTextDatum(MC_DATUM);
        canvas().setTextColor(kWhite);
        canvas().drawString("Firmware update", 160, 82, uiFont(&fonts::FreeSansBold12pt7b));
        canvas().setTextColor(kTextMuted);
        canvas().drawString("Keep power connected", 160, 112, uiFont(&fonts::FreeSans9pt7b));
        canvas().drawRoundRect(40, 142, 240, 16, 8, kTextMuted);
        const int width = static_cast<int>(236UL * firmwareUpdatePercent / 100UL);
        if (width > 0) canvas().fillRoundRect(42, 144, width, 12, 6, kBlue);
        char percent[8];
        snprintf(percent, sizeof(percent), "%u%%", firmwareUpdatePercent);
        canvas().setTextColor(kWhite);
        canvas().drawString(percent, 160, 181, uiFont(&fonts::Font0));
    }
    presentCanvas(0, 240);
}

void showConfigurationQrScreen() {
    initCanvas();
    drawConfigurationBootScreen(WiFi.localIP().toString());
    presentCanvas(0, 240);
}

void setFirmwareUpdateProgress(bool active, uint8_t percent) {
    firmwareUpdateOverlayActive = active;
    firmwareUpdatePercent = percent > 100 ? 100 : percent;
    forceRedraw = true;
}
