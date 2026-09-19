#include "display.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <cmath>

#include "app_state.h"

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

void updateWeatherUI() {
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
