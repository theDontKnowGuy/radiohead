#include "boot_screen.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include <FS.h>
#include <FSImpl.h>

#include "app_state.h"
#include "media.h"
#include "BootAudio.h"
#include "BootScreenNew.h"
#include "BootLogo.h"

namespace BootScreen {
namespace {

constexpr int16_t kBarX = 80;
constexpr int16_t kBarWidth = 160;
constexpr int16_t kBarHeight = 8;
constexpr int16_t kBarRadius = kBarHeight / 2;

struct BootStyle {
    const uint8_t* artwork;
    size_t artworkBytes;
    int16_t barY;
    uint32_t barColor;
    uint32_t barTroughColor;
    unsigned long progressMs;
};

// Keep style values independent so the second mode can evolve separately.
const BootStyle kOriginalStyle = {boot_logo_png, boot_logo_png_len, 168, 0x1BB8EE, 0x0C3764, ProgressMs};
const BootStyle kNewArtworkStyle = {boot_screen_new_png, boot_screen_new_png_len, 210, 0xFFFFFF, 0x0C3764, ProgressMs * 3 / 2};

const BootStyle& activeStyle() {
    return bootMode == BootMode::Original ? kOriginalStyle : kNewArtworkStyle;
}
constexpr const char* kBootAudioPath = "/boot.aac";
bool bootAudioStarted = false;
bool bootDecoderReady = false;
bool bootAudioEnded = false;

// Audio::connecttoFS needs a seekable File. Present the compiled AAC bytes as
// a read-only file without copying them into RAM or depending on LittleFS.
class BootAudioFile : public fs::FileImpl {
public:
    size_t write(const uint8_t*, size_t) override { return 0; }
    size_t read(uint8_t* buffer, size_t length) override {
        if (!open_ || buffer == nullptr) return 0;
        const size_t count = std::min(length, size() - position_);
        memcpy(buffer, boot_audio_aac + position_, count);
        position_ += count;
        return count;
    }
    void flush() override {}
    bool seek(uint32_t offset, fs::SeekMode mode) override {
        if (!open_) return false;
        const uint64_t base = mode == fs::SeekCur ? position_ :
            mode == fs::SeekEnd ? size() : 0;
        if (base + offset > size()) return false;
        position_ = static_cast<size_t>(base + offset);
        return true;
    }
    size_t position() const override { return position_; }
    size_t size() const override { return boot_audio_aac_len; }
    bool setBufferSize(size_t) override { return false; }
    void close() override { open_ = false; }
    time_t getLastWrite() override { return 0; }
    const char* path() const override { return kBootAudioPath; }
    const char* name() const override { return "boot.aac"; }
    boolean isDirectory() override { return false; }
    fs::FileImplPtr openNextFile(const char*) override { return {}; }
    boolean seekDir(long) override { return false; }
    String getNextFileName() override { return {}; }
    String getNextFileName(bool*) override { return {}; }
    void rewindDirectory() override {}
    operator bool() override { return open_; }

private:
    size_t position_ = 0;
    bool open_ = true;
};

class BootAudioFilesystem : public fs::FSImpl {
public:
    fs::FileImplPtr open(const char* path, const char* mode, bool create) override {
        if (!exists(path) || mode == nullptr || strcmp(mode, FILE_READ) != 0 || create) {
            return {};
        }
        return std::make_shared<BootAudioFile>();
    }
    bool exists(const char* path) override {
        return path != nullptr && strcmp(path, kBootAudioPath) == 0;
    }
    bool rename(const char*, const char*) override { return false; }
    bool remove(const char*) override { return false; }
    bool mkdir(const char*) override { return false; }
    bool rmdir(const char*) override { return false; }
};

fs::FS& bootAudioFilesystem() {
    static fs::FS filesystem(std::make_shared<BootAudioFilesystem>());
    return filesystem;
}

void drawProgress(int percent) {
    if (percent <= 0) return;
    const BootStyle& style = activeStyle();
    const int16_t width = std::max<int16_t>(
        static_cast<int16_t>((kBarWidth * std::min(percent, 100)) / 100),
        kBarHeight);
    tft.fillRoundRect(
        kBarX, style.barY, width, kBarHeight, kBarRadius, style.barColor);
}

}  // namespace

void draw() {
    const BootStyle& style = activeStyle();
    tft.fillScreen(TFT_BLACK);
    const bool drawn = tft.drawPng(style.artwork, style.artworkBytes, 0, 0, 320, 240);
    if (!drawn) {
        Serial.println("[boot] boot artwork decode failed");
    }
    tft.drawRoundRect(
        kBarX, style.barY, kBarWidth, kBarHeight, kBarRadius,
        style.barTroughColor);
}

void startAudio() {
    bootDecoderReady = false;
    bootAudioEnded = false;
    Audio::audio_info_callback = [](Audio::msg_t message) {
        if (message.e == Audio::evt_info && message.msg != nullptr &&
            strcmp(message.msg, "stream ready") == 0) {
            bootDecoderReady = true;
            Serial.println("[boot] audio decoder ready");
        } else if (message.e == Audio::evt_eof) {
            bootAudioEnded = true;
            Serial.println("[boot] audio file ended");
        }
    };
    bootAudioStarted = audio.connecttoFS(bootAudioFilesystem(), kBootAudioPath);
    if (bootAudioStarted) mediaEnableBootOutput();
    Serial.printf("[boot] sound opened=%d bytes=%lu volume=%u\n",
                  bootAudioStarted, static_cast<unsigned long>(audio.getFileSize()),
                  audio.getVolume());
}

void hold(unsigned long startedAt) {
    const unsigned long progressMs = activeStyle().progressMs;
    int lastPercent = -1;
    bool audioStartAttempted = AudioStartDelayMs == 0;
    unsigned long audioEndedAt = startedAt;
    bool audioFinished = audioStartAttempted && !bootAudioStarted;
    uint32_t maxAudioPosition = 0;
    uint32_t maxAudioSeconds = 0;
    while (true) {
        const unsigned long now = millis();
        const unsigned long elapsed = now - startedAt;
        if (!audioStartAttempted && elapsed >= AudioStartDelayMs) {
            audioStartAttempted = true;
            startAudio();
            if (!bootAudioStarted) {
                audioFinished = true;
                audioEndedAt = now;
            }
        }
        if (bootAudioStarted && audio.isRunning()) audio.loop();
        if (bootAudioStarted) {
            maxAudioPosition = std::max(maxAudioPosition, audio.getAudioFilePosition());
            maxAudioSeconds = std::max(maxAudioSeconds, audio.getAudioCurrentTime());
        }
        if (bootAudioStarted && !audioFinished &&
            (bootAudioEnded || !audio.isRunning())) {
            audioFinished = true;
            audioEndedAt = millis();
        }
        const int percent = static_cast<int>(
            (std::min(elapsed, progressMs) * 100UL) / progressMs);
        if (percent != lastPercent) {
            lastPercent = percent;
            drawProgress(percent);
        }
        if (audioFinished && millis() - audioEndedAt >= AfterAudioMs &&
            (bootMode == BootMode::Original || elapsed >= progressMs)) break;
        if (elapsed >= MaxHoldMs) break;
        delay(2);
    }

    drawProgress(100);
    tft.waitDMA();

    if (bootAudioStarted) {
        Serial.printf("[boot] sound progress read=%lu/%lu seconds=%lu decoder=%d eof=%d running=%d\n",
                      static_cast<unsigned long>(maxAudioPosition),
                      static_cast<unsigned long>(boot_audio_aac_len),
                      static_cast<unsigned long>(maxAudioSeconds),
                      bootDecoderReady, bootAudioEnded, audio.isRunning());
        audio.stopSong();
        bootAudioStarted = false;
    }
}

}  // namespace BootScreen
