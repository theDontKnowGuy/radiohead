#include "boot_screen.h"

#include <algorithm>

#include "app_state.h"
#include "BootLogo.h"

namespace BootScreen {
namespace {

constexpr int16_t kBarX = 80;
constexpr int16_t kBarY = 168;
constexpr int16_t kBarWidth = 160;
constexpr int16_t kBarHeight = 8;
constexpr int16_t kBarRadius = kBarHeight / 2;

// Sampled from the blue/cyan treatment in the supplied artwork.
constexpr uint32_t kBarColor = 0x1BB8EE;
constexpr uint32_t kBarTroughColor = 0x0C3764;

void drawProgress(int percent) {
    if (percent <= 0) return;
    const int16_t width = std::max<int16_t>(
        static_cast<int16_t>((kBarWidth * std::min(percent, 100)) / 100),
        kBarHeight);
    tft.fillRoundRect(
        kBarX, kBarY, width, kBarHeight, kBarRadius, kBarColor);
}

}  // namespace

void draw() {
    tft.fillScreen(TFT_BLACK);
    if (!tft.drawPng(boot_logo_png, boot_logo_png_len, 0, 0, 320, 240)) {
        Serial.println("[boot] boot artwork decode failed");
    }
    tft.drawRoundRect(
        kBarX, kBarY, kBarWidth, kBarHeight, kBarRadius,
        kBarTroughColor);
}

void hold(
    unsigned long startedAt,
    bool (*stillWaiting)(),
    unsigned long maxHoldMs) {
    int lastPercent = -1;
    unsigned long elapsed = 0;
    while ((elapsed = millis() - startedAt) < HoldMs) {
        const int percent = static_cast<int>((elapsed * 100UL) / HoldMs);
        if (percent != lastPercent) {
            lastPercent = percent;
            drawProgress(percent);
        }
        delay(10);
    }

    drawProgress(100);
    while (stillWaiting != nullptr && stillWaiting() &&
           millis() - startedAt < maxHoldMs) {
        delay(10);
    }

    tft.fillScreen(TFT_BLACK);
    tft.waitDMA();
}

}  // namespace BootScreen
