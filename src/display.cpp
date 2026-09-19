#include "display.h"
#include <esp_heap_caps.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <cmath>
#include <time.h>

#include "app_state.h"
#include "display_fonts.h"
#include "media.h"
#include "ui_controller.h"
#include "ui_text.h"
#include "ui_background_asset.h"
#include "ui_home_assets.h"

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
portMUX_TYPE weatherStateMux = portMUX_INITIALIZER_UNLOCKED;
String weatherCitySnapshot;
String weatherKeySnapshot;

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
        tempC = newTemperature;
        weatherID = newWeatherId;
        weatherDataValid = true;
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

    const uint32_t level = audio.getVUlevel();
    needlePosition +=
        (map(constrain(level, 0, 70000), 0, 70000, 0, 100) - needlePosition) * 0.15F;
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
        (millis() - lastWeatherUpdate > 900000 || lastWeatherUpdate == 0)) {
        if (WiFi.status() == WL_CONNECTED && !isAP && !owmKey.isEmpty()) {
            weatherCitySnapshot = owmCity;
            weatherKeySnapshot = owmKey;
            weatherFetchInProgress = true;
            if (xTaskCreatePinnedToCore(
                    fetchWeatherTask,
                    "Weather",
                    8192,
                    nullptr,
                    1,
                    nullptr,
                    0) != pdPASS) {
                weatherFetchInProgress = false;
            } else {
                lastWeatherUpdate = millis();
            }
        }
    }
}

void updateWeatherUI() {
    updateWeatherData();

    float displayedTemperature = 0.0F;
    int displayedWeatherId = 0;
    bool hasWeatherData = false;
    portENTER_CRITICAL(&weatherStateMux);
    displayedTemperature = tempC;
    displayedWeatherId = weatherID;
    hasWeatherData = weatherDataValid;
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

    const uint32_t level = audio.getVUlevel();
    for (int i = 0; i < 2; ++i) {
        const int targetHeight = constrain(map(level, 0, 70000, 0, 65), 0, 65);
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
constexpr uint16_t kTextMuted = 0xB596;
constexpr uint16_t kBlue = 0x13DE;
constexpr uint16_t kBlueDark = 0x0B16;
constexpr uint16_t kBlueFocus = 0x8E5F;
constexpr uint16_t kRed = 0xF945;
constexpr uint16_t kGreen = 0x154B;
constexpr uint16_t kPurple = 0x8218;
constexpr uint16_t kSlate = 0x4391;

#ifndef UI_P2_FIXTURE
#define UI_P2_FIXTURE 0
#endif

constexpr int16_t kHomeTileY = 154;
constexpr int16_t kHomeTileHeight = 70;
constexpr int16_t kHomePlayerX = 198;
constexpr int16_t kHomePlayerY = 36;
constexpr int16_t kHomePlayerWidth = 110;
constexpr int16_t kHomePlayerHeight = 70;

const lgfx::IFont* homeLabelFont() {
    return uiFrameReady ? display_fonts::label() : &fonts::Font0;
}

const lgfx::IFont* homeCaptionFont() {
    return uiFrameReady ? display_fonts::caption() : &fonts::Font0;
}

bool contains(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x < left + width && y >= top && y < top + height;
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

void footerButton(int16_t x, int16_t width, const char* label, bool highlighted = false) {
    canvas().fillRoundRect(x + 2, 190, width - 4, 46, 6, highlighted ? kBlue : kSurface);
    canvas().drawRoundRect(x + 2, 190, width - 4, 46, 6, highlighted ? kWhite : kTextMuted);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(label, x + width / 2, 214, uiFont(&fonts::FreeSans9pt7b));
}

void drawBackground() {
    if (!canvas().drawPng(ui_background_png, ui_background_png_len, 0, 0, 320, 240)) {
        canvas().fillScreen(kNavy);
    }
}

void drawHeaderClock(const char* currentTime, bool timeValid) {
    // Reserve separate clock and Wi-Fi slots; restore the whole old text area.
    canvas().fillRect(214, 0, 106, 42, kNavy);
    canvas().setTextDatum(TR_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(timeValid ? currentTime : "--:--", 270, 13, uiFont(&fonts::FreeSans9pt7b));
    canvas().drawArc(294, 20, 5, 8, 210, 330, kWhite);
    canvas().drawArc(294, 20, 10, 13, 210, 330, kWhite);
    canvas().fillCircle(294, 26, 2, kWhite);
}

void drawHeader(const char* currentTime, bool timeValid, const char* title, bool back = true) {
    canvas().fillRect(0, 0, 320, 44, kNavy);
    canvas().fillRect(0, 42, 320, 2, kBlueDark);
    if (back) {
        canvas().drawLine(29, 14, 20, 22, kWhite);
        canvas().drawLine(20, 22, 29, 30, kWhite);
    }
    text(title, back ? 42 : 14, 13, uiFont(&fonts::FreeSans9pt7b), kWhite, 150);
    drawHeaderClock(currentTime, timeValid);
}

void drawHomeHeader(bool timeValid) {
    // Home intentionally has no opaque navigation bar: the top of the sunset
    // photo is part of this screen's composition. Other pages retain drawHeader.
    constexpr int16_t centerY = 22;
    canvas().setTextColor(kWhite);
    canvas().setTextDatum(ML_DATUM);
    canvas().drawString("Internet Radio", 14, centerY, uiFont(&fonts::FreeSans9pt7b));
    char date[16] = "";
    if (timeValid) {
        const time_t now = time(nullptr);
        tm localTime = {};
        if (localtime_r(&now, &localTime) != nullptr) {
            strftime(date, sizeof(date), "%a, %d %b", &localTime);
        }
    }
    canvas().setTextDatum(MR_DATUM);
    canvas().setTextColor(kWhite);
    // The small face's visible letters sit slightly below its line-box center.
    canvas().drawString(date[0] == '\0' ? "" : date, 275, centerY - 1, uiFont(&fonts::Font0));
    if (uiFrameReady) {
        // Visible Wi-Fi pixels span local y=4..16: optical center is y=10.
        canvas().drawPng(ui_home_wifi, sizeof(ui_home_wifi), 282, centerY - 10);
    } else {
        canvas().drawArc(293, centerY, 4, 6, 210, 330, kWhite);
        canvas().drawArc(293, centerY, 8, 10, 210, 330, kWhite);
        canvas().fillCircle(293, centerY + 5, 1, kWhite);
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

void drawHomeTileIconFallback(int16_t x, int16_t y, uint8_t tile) {
    canvas().setTextColor(kWhite);
    if (tile == 0) {
        canvas().drawRoundRect(x + 8, y + 10, 28, 18, 4, kWhite);
        canvas().drawCircle(x + 17, y + 19, 3, kWhite);
        canvas().drawCircle(x + 29, y + 19, 3, kWhite);
        canvas().drawLine(x + 21, y + 7, x + 30, y + 2, kWhite);
        canvas().drawLine(x + 30, y + 2, x + 35, y + 2, kWhite);
    } else if (tile == 1) {
        for (int row = 0; row < 3; ++row) {
            canvas().fillCircle(x + 11, y + 9 + row * 9, 2, kWhite);
            canvas().fillRoundRect(x + 18, y + 7 + row * 9, 22, 4, 2, kWhite);
        }
    } else if (tile == 2) {
        canvas().drawLine(x + 24, y + 32, x + 10, y + 18, kWhite);
        canvas().drawLine(x + 10, y + 18, x + 10, y + 11, kWhite);
        canvas().drawLine(x + 10, y + 11, x + 15, y + 6, kWhite);
        canvas().drawLine(x + 15, y + 6, x + 24, y + 12, kWhite);
        canvas().drawLine(x + 24, y + 12, x + 33, y + 6, kWhite);
        canvas().drawLine(x + 33, y + 6, x + 38, y + 11, kWhite);
        canvas().drawLine(x + 38, y + 11, x + 38, y + 18, kWhite);
        canvas().drawLine(x + 38, y + 18, x + 24, y + 32, kWhite);
    } else {
        canvas().fillCircle(x + 24, y + 17, 11, kWhite);
        canvas().fillCircle(x + 24, y + 17, 5, kSlate);
        for (int i = 0; i < 8; ++i) {
            const float angle = i * 45 * 0.0174533F;
            canvas().fillCircle(x + 24 + cos(angle) * 14, y + 17 + sin(angle) * 14, 3, kWhite);
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

void drawHomeWeatherIcon(int16_t x, int16_t y, bool valid, int id) {
    if (!uiFrameReady) {
        // Use a neutral primitive for unknown/obscured conditions in fallback.
        if (!valid || (id != 800 && id != 801 && id != 802)) {
            canvas().fillRoundRect(x + 14, y + 22, 42, 22, 9, kTextMuted);
        } else {
            drawHomeWeatherIconFallback(x, y, true, id);
        }
        return;
    }
    if (!valid) drawHomeAsset(ui_home_weather_unknown, x, y);
    else if (id == 800) drawHomeAsset(ui_home_weather_clear, x, y);
    else if (id == 801 || id == 802) drawHomeAsset(ui_home_weather_partly, x, y);
    else if (id == 803 || id == 804) drawHomeAsset(ui_home_weather_cloudy, x, y);
    else if (id >= 200 && id <= 232) drawHomeAsset(ui_home_weather_storm, x, y);
    else if ((id >= 300 && id <= 321) || (id >= 500 && id <= 531)) drawHomeAsset(ui_home_weather_rain, x, y);
    else if (id >= 600 && id <= 622) drawHomeAsset(ui_home_weather_snow, x, y);
    else if (id >= 700 && id < 800) drawHomeAsset(ui_home_weather_mist, x, y);
    else drawHomeAsset(ui_home_weather_unknown, x, y);
}

void drawHomeTileIcon(int16_t x, int16_t y, uint8_t tile) {
    if (!uiFrameReady) {
        drawHomeTileIconFallback(x - 6, y, tile);
        return;
    }
    switch (tile) {
    case 0: drawHomeAsset(ui_home_icon_radio, x, y); break;
    case 1: drawHomeAsset(ui_home_icon_shows, x, y); break;
    case 2: drawHomeAsset(ui_home_icon_favorites, x, y); break;
    case 3: drawHomeAsset(ui_home_icon_settings, x, y); break;
    }
}

void drawArtwork(const String& name, int16_t x, int16_t y, int16_t size, uint16_t accent = kBlue) {
    canvas().fillRoundRect(x, y, size, size, 7, kSurfaceRaised);
    canvas().fillRoundRect(x + 4, y + 4, size - 8, size - 8, 5, accent);
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
    const uint16_t color = filled ? 0xFFE0 : kWhite;
    for (int angle = 0; angle < 5; ++angle) {
        const float a = (-90 + angle * 72) * 0.0174533F;
        const float b = (-90 + ((angle + 2) % 5) * 72) * 0.0174533F;
        canvas().drawLine(cx + cos(a) * 10, cy + sin(a) * 10, cx + cos(b) * 10, cy + sin(b) * 10, color);
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

void drawSlider(int16_t y) {
    canvas().fillRoundRect(48, y, 214, 8, 4, kSurfaceRaised);
    canvas().fillRoundRect(48, y, mainVal * 214 / 21, 8, 4, kBlue);
    canvas().fillCircle(48 + mainVal * 214 / 21, y + 4, 8, kWhite);
    text(isStationMuted() ? "Muted" : String(mainVal) + " / 21", 270, y - 4, uiFont(&fonts::Font0), kWhite, 46);
}

void renderListening(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawHeader(currentTime, timeValid, isAP ? "Setup" : "Live Radio");
    if (isAP) {
        canvas().fillRoundRect(12, 55, 296, 120, 8, kSurface);
        text("Connect on your phone", 28, 78, uiFont(&fonts::FreeSans9pt7b), kWhite);
        text("Radio_Setup", 28, 108, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 250);
        text("192.168.4.1", 28, 142, uiFont(&fonts::FreeSans9pt7b), kTextMuted);
    } else {
        const String station = podcastMode ? podcastShowTft : stations[currentStationIdx].name;
        drawArtwork(station, 12, 54, 88);
        canvas().fillRoundRect(110, 54, 198, 103, 8, kSurface);
        canvas().fillRoundRect(120, 63, 38, 17, 5, podcastMode ? kGreen : kRed);
        text(podcastMode ? "SHOW" : "LIVE", 126, 67, uiFont(&fonts::Font0), kWhite);
        text(station, 120, 89, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 164);
        const String detail = songTitle.isEmpty() ? "Live radio" : songTitle;
        text(detail, 120, 123, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 164);
        drawStar(286, 75);
        drawTransport(91, 181, "<", false);
        drawTransport(160, 181, "[]", true);
        drawTransport(229, 181, ">", false);
        drawSlider(218);
        if (alarmActive) {
            char alarmText[18];
            snprintf(alarmText, sizeof(alarmText), "Alarm %02d:%02d", alarmH, alarmM);
            text(alarmText, 120, 142, uiFont(&fonts::Font0), kTextMuted, 160);
        }
    }

    if (state.volumeOverlay) {
        canvas().fillRoundRect(48, 78, 224, 92, 12, kNavy);
        canvas().drawRoundRect(48, 78, 224, 92, 12, kTextMuted);
        text("Volume", 123, 100, uiFont(&fonts::FreeSans9pt7b), kWhite);
        drawSlider(138);
    }
}

void renderStations(const UiRenderState& state, const char* currentTime, bool timeValid) {
    drawBackground();
    drawHeader(currentTime, timeValid, "Live Stations");
    const int count = playableStationCount();
    if (count == 0) {
        canvas().fillRoundRect(12, 60, 296, 105, 8, kSurface);
        text("No stations saved", 28, 86, uiFont(&fonts::FreeSansBold12pt7b), kWhite, 260);
        text("Add one at the phone setup page.", 28, 124, uiFont(&fonts::FreeSans9pt7b), kTextMuted, 260);
    }
    for (int row = 0; row < 3; ++row) {
        const int visibleIndex = state.stationOffset + row;
        const int y = 44 + row * 48;
        const int slot = playableStationSlotAt(visibleIndex);
        if (slot < 0) {
            continue;
        }
        const bool focused = visibleIndex == state.stationFocus;
        canvas().fillRoundRect(8, y + 3, 304, 42, 6, focused ? kBlue : kSurface);
        drawArtwork(stations[slot].name, 14, y + 7, 34, focused ? kBlueDark : kSlate);
        text(stations[slot].name, 58, y + 10, uiFont(&fonts::FreeSans9pt7b), kWhite, 185);
        if (slot == currentStationIdx) {
            canvas().setTextDatum(TR_DATUM);
            canvas().setTextColor(kWhite);
            canvas().drawString("LIVE", 272, y + 16, uiFont(&fonts::Font0));
        }
        drawStar(292, y + 24);
    }
    footerButton(0, 106, "Back");
    footerButton(106, 107, "Previous", state.stationOffset > 0);
    footerButton(213, 107, "Next", state.stationOffset + 3 < count);
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
        canvas().fillRoundRect(x, kHomeTileY, 70, kHomeTileHeight, 8, color);
    }
    drawHomeTileIcon(x + 17, kHomeTileY + 6, tile);
    canvas().setTextDatum(MC_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(top, x + 35, kHomeTileY + (bottom == nullptr ? 53 : 47), homeLabelFont());
    if (bottom != nullptr) canvas().drawString(bottom, x + 35, kHomeTileY + 60, homeLabelFont());
    serviceUiAudio();
}

void renderHome(const UiRenderState& state, const char* currentTime, bool timeValid) {
    if (!canvas().drawPng(ui_home_background, sizeof(ui_home_background), 0, 0)) {
        canvas().fillScreen(kNavy);
    }
    serviceUiAudio();
    const String station = podcastMode ? podcastShowTft : stations[currentStationIdx].name;
    drawHomeHeader(timeValid);
    float temperature = 0.0F;
    int condition = 0;
    bool hasWeather = false;
    portENTER_CRITICAL(&weatherStateMux);
    temperature = tempC;
    condition = weatherID;
    hasWeather = weatherDataValid;
    portEXIT_CRITICAL(&weatherStateMux);

    // Home deliberately leaves the supplied sunset visible.  It is the primary
    // composition layer; only dense pages receive opaque reading surfaces.
    drawHomeWeatherIcon(14, 58, hasWeather, condition);
    if (hasWeather) {
        char temperatureText[12];
        const float displayedTemperature = useCelsius ? temperature : temperature * 9.0F / 5.0F + 32.0F;
        snprintf(temperatureText, sizeof(temperatureText), "%d", static_cast<int>(roundf(displayedTemperature)));
        text(uiFrameReady ? String(temperatureText) + "°" : String(temperatureText),
             90, 65, uiFont(&fonts::FreeSansBold18pt7b), kWhite, 106);
        if (!uiFrameReady) {
            canvas().drawCircle(94 + canvas().textWidth(temperatureText), 71, 2, kWhite);
        }
        text(homeCityLabel(owmCity), 90, 101, homeCaptionFont(), kWhite, 106);
        text(homeWeatherDescription(condition), 90, 118, homeCaptionFont(), kWhite, 106);
    } else {
        text("Weather", 90, 67, uiFont(&fonts::Font0), kWhite, 98);
        text("Unavailable", 90, 91, uiFont(&fonts::FreeSans9pt7b), kWhite, 106);
        text("Configure on phone", 90, 118, uiFont(&fonts::Font0), kTextMuted, 116);
    }

    canvas().setTextDatum(TR_DATUM);
    canvas().setTextColor(kWhite);
    canvas().drawString(timeValid ? currentTime : "--:--", 303, 40, uiFont(&fonts::FreeSansBold24pt7b));
    // Keep a visible return-to-player target after the Home title becomes fixed.
    text(station.isEmpty() ? "Now playing" : station, 212, 84, uiFont(&fonts::Font0), kWhite, 91);
    canvas().fillTriangle(201, 87, 201, 93, 206, 90, kWhite);
    drawHomeTile(8, kBlue, "Live Radio", nullptr, 0);
    drawHomeTile(86, kGreen, "Recorded", "Shows", 1);
    drawHomeTile(164, kPurple, "Favorites", nullptr, 2);
    drawHomeTile(242, kSlate, "Settings", nullptr, 3);
    const int16_t focusX[] = {8, 86, 164, 242};
    const uint8_t focus = state.homeFocus < 4 ? state.homeFocus : 0;
    if (uiFrameReady) {
        drawHomeAsset(ui_home_focus, focusX[focus] - 2, kHomeTileY - 2);
    } else {
        canvas().drawRoundRect(focusX[focus] - 2, kHomeTileY - 2, 74, kHomeTileHeight + 4, 9, kBlueFocus);
    }
}

void renderUnavailable(const char* currentTime, bool timeValid) {
    drawBackground();
    drawHeader(currentTime, timeValid, "Not available");
    canvas().fillRoundRect(20, 76, 280, 88, 10, kSurface);
    text("This destination is coming later.", 40, 100, uiFont(&fonts::FreeSans9pt7b), kWhite, 230);
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

UiTarget uiHitTest(const UiRenderState& state, int16_t x, int16_t y) {
    if (state.page == UiPage::Home) {
        if (contains(x, y, kHomePlayerX, kHomePlayerY, kHomePlayerWidth, kHomePlayerHeight)) return UiTarget::HomeNowPlaying;
        if (contains(x, y, 8, kHomeTileY, 72, kHomeTileHeight)) return UiTarget::HomeLiveRadio;
        if (contains(x, y, 86, kHomeTileY, 72, kHomeTileHeight)) return UiTarget::HomeRecordedShows;
        if (contains(x, y, 164, kHomeTileY, 72, kHomeTileHeight)) return UiTarget::HomeFavorites;
        if (contains(x, y, 242, kHomeTileY, 72, kHomeTileHeight)) return UiTarget::HomeSettings;
    } else if (state.page == UiPage::Listening) {
        if (contains(x, y, 0, 44, 320, 144)) return UiTarget::ListeningStation;
        if (contains(x, y, 0, 188, 106, 52)) return UiTarget::ListeningStation;
        if (contains(x, y, 106, 188, 107, 52)) return UiTarget::ListeningMute;
        if (contains(x, y, 213, 188, 107, 52)) return UiTarget::ListeningMenu;
    } else if (state.page == UiPage::Stations) {
        if (contains(x, y, 0, 188, 106, 52)) return UiTarget::ListBack;
        if (contains(x, y, 106, 188, 107, 52)) return UiTarget::ListPrevious;
        if (contains(x, y, 213, 188, 107, 52)) return UiTarget::ListNext;
        if (y >= 44 && y < 188) {
            return static_cast<UiTarget>(static_cast<int>(UiTarget::ListRow0) + (y - 44) / 48);
        }
    } else if (state.page == UiPage::StandbyConfirm) {
        if (contains(x, y, 38, 124, 110, 44)) return UiTarget::ConfirmCancel;
        if (contains(x, y, 172, 124, 110, 44)) return UiTarget::ConfirmStandby;
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
    case UiPage::StandbyConfirm:
        renderConfirm(state, currentTime, timeValid);
        break;
    case UiPage::Unavailable:
        renderUnavailable(currentTime, timeValid);
        break;
    }
#endif
    presentCanvas(0, 240);
}

void renderRadioUiClock(const char* currentTime, bool timeValid) {
    initCanvas();
    drawHeaderClock(currentTime, timeValid);
    presentCanvas(0, 42);
}
