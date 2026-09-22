#include "firmware_updater.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <cctype>

#include "FirmwareVersion.h"
#include "UpdateRootCAs.h"

namespace {

constexpr char MANIFEST_URL[] =
    "https://github.com/theDontKnowGuy/radiohead/releases/latest/download/manifest.json";
constexpr uint32_t FIRST_CHECK_DELAY_MS = 2UL * 60UL * 1000UL;
constexpr uint32_t CHECK_INTERVAL_MS = 60UL * 60UL * 1000UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
constexpr uint32_t DOWNLOAD_STALL_TIMEOUT_MS = 20000;
constexpr int MANIFEST_MAX_BYTES = 8192;
constexpr size_t DOWNLOAD_CHUNK_BYTES = 2048;
constexpr uint32_t TASK_STACK_BYTES = 12288;
// ESP32-audioI2S owns a priority-2 decoder task on core 0.  Release checks
// perform TLS and JSON work, so keeping this equal-priority maintenance task
// on the Arduino loop core keeps that work from starving IDLE0 while audio is
// decoding.  The loop task is also priority 1, which lets normal FreeRTOS
// time-slicing continue to service the radio and web server.
constexpr BaseType_t MAINTENANCE_TASK_CORE = ARDUINO_RUNNING_CORE;
constexpr UBaseType_t MAINTENANCE_TASK_PRIORITY = 1;

bool isHexDigest(const String& text) {
    if (text.length() != 32) return false;
    for (size_t index = 0; index < text.length(); ++index) {
        if (!isxdigit(static_cast<unsigned char>(text[index]))) return false;
    }
    return true;
}

bool isApprovedGitHubUrl(const String& url) {
    return url.startsWith("https://github.com/") ||
        url.startsWith("https://objects.githubusercontent.com/");
}

}  // namespace

FirmwareUpdater firmwareUpdater;

void FirmwareUpdater::begin(bool shouldAutoInstall) {
    if (taskHandle != nullptr) return;
    autoInstall.store(shouldAutoInstall);
    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == nullptr) {
        setStatus(Status::Failed, "could not create update state lock");
        return;
    }
    if (xTaskCreatePinnedToCore(taskEntry, "firmware-update", TASK_STACK_BYTES,
                                this, MAINTENANCE_TASK_PRIORITY, &taskHandle,
                                MAINTENANCE_TASK_CORE) != pdPASS) {
        taskHandle = nullptr;
        setStatus(Status::Failed, "could not start update worker");
        return;
    }
    Serial.printf("[update] running %s (%s)\n", FIRMWARE_VERSION, FIRMWARE_BUILD);
}

void FirmwareUpdater::setAutoInstall(bool enabled) {
    autoInstall.store(enabled);
    // A release may have been held in ask-first mode when the owner changes
    // policy. Do not strand it: make the switch take effect immediately.
    if (enabled && taskHandle != nullptr && releaseAvailable.load() && !busy.exchange(true)) {
        installRequested.store(true);
        xTaskNotifyGive(taskHandle);
    }
}

void FirmwareUpdater::requestCheckNow() {
    if (taskHandle == nullptr || releaseAvailable.load() || busy.exchange(true)) return;
    setStatus(Status::Checking, "Checking GitHub releases");
    xTaskNotifyGive(taskHandle);
}

bool FirmwareUpdater::requestInstallNow() {
    if (taskHandle == nullptr || !releaseAvailable.load() || busy.exchange(true)) return false;
    installRequested.store(true);
    xTaskNotifyGive(taskHandle);
    return true;
}

void FirmwareUpdater::taskEntry(void* context) {
    static_cast<FirmwareUpdater*>(context)->taskLoop();
}

void FirmwareUpdater::taskLoop() {
    uint32_t waitMs = FIRST_CHECK_DELAY_MS;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
        waitMs = CHECK_INTERVAL_MS;

        if (installRequested.exchange(false) && releaseAvailable.load()) {
            installPendingRelease();
            continue;
        }
        if (releaseAvailable.load()) continue;
        checkForUpdate();
    }
}

void FirmwareUpdater::checkForUpdate() {
    busy.store(true);
    setStatus(Status::Checking, "Checking GitHub releases");
    if (WiFi.status() != WL_CONNECTED) {
        setStatus(Status::Failed, "not connected to Wi-Fi");
        busy.store(false);
        return;
    }

    WiFiClientSecure client;
    client.setCACert(UpdateRootCAs::GitHubRoots);
    client.setTimeout(HTTP_TIMEOUT_MS / 1000);
    HTTPClient http;
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(client, MANIFEST_URL)) {
        setStatus(Status::Failed, "could not open release manifest");
        busy.store(false);
        return;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK) {
        setStatus(Status::Failed, "manifest request returned HTTP " + String(response));
        http.end();
        busy.store(false);
        return;
    }
    const int length = http.getSize();
    if (length <= 0 || length > MANIFEST_MAX_BYTES) {
        setStatus(Status::Failed, "manifest has an invalid length");
        http.end();
        busy.store(false);
        return;
    }
    const String payload = http.getString();
    http.end();
    if (payload.length() != static_cast<size_t>(length)) {
        setStatus(Status::Failed, "manifest download was truncated");
        busy.store(false);
        return;
    }

    JsonDocument manifest;
    if (const DeserializationError error = deserializeJson(manifest, payload)) {
        setStatus(Status::Failed, String("manifest parse failed: ") + error.c_str());
        busy.store(false);
        return;
    }
    const String offeredVersion = manifest["version"] | "";
    const Version current = parseVersion(FIRMWARE_VERSION);
    const Version offered = parseVersion(offeredVersion);
    if (!offered.valid || !current.valid) {
        setStatus(Status::Failed, "manifest has an invalid version");
        busy.store(false);
        return;
    }
    if (!isNewer(offered, current)) {
        setStatus(Status::UpToDate, "Up to date (" FIRMWARE_VERSION ")");
        busy.store(false);
        return;
    }

    JsonVariantConst build = manifest["builds"][FIRMWARE_BUILD];
    if (!build.is<JsonObjectConst>()) {
        setStatus(Status::Failed, "release has no image for " FIRMWARE_BUILD);
        busy.store(false);
        return;
    }
    Release release;
    release.version = offeredVersion;
    release.notes = manifest["notes"] | "";
    release.url = build["url"] | "";
    release.md5 = build["md5"] | "";
    release.size = build["size"] | 0U;
    if (!isApprovedGitHubUrl(release.url) || !isHexDigest(release.md5) ||
        release.size == 0 || release.size > ESP.getFreeSketchSpace()) {
        setStatus(Status::Failed, "release image metadata was rejected");
        busy.store(false);
        return;
    }
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        pendingRelease = release;
        xSemaphoreGive(stateMutex);
    }
    releaseAvailable.store(true);
    Serial.printf("[update] available %s -> %s (%u bytes)\n", FIRMWARE_VERSION,
                  release.version.c_str(), static_cast<unsigned>(release.size));
    setStatus(Status::UpdateFound, autoInstall.load()
        ? "Version " + release.version + " found; downloading"
        : "Version " + release.version + " is ready to install");
    if (autoInstall.load()) {
        // Keep maintenance exclusive across the hand-off from manifest fetch to
        // flashing; a browser upload must not slip into that small window.
        installPendingRelease();
        return;
    }
    busy.store(false);
}

void FirmwareUpdater::installPendingRelease() {
    const Release release = pendingReleaseCopy();
    if (release.url.isEmpty() || release.size == 0) return;
    busy.store(true);
    setStatus(Status::Downloading, "Downloading " + release.version);

    WiFiClientSecure client;
    client.setCACert(UpdateRootCAs::GitHubRoots);
    client.setTimeout(HTTP_TIMEOUT_MS / 1000);
    HTTPClient http;
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(client, release.url)) {
        setStatus(Status::Failed, "could not open firmware download");
        busy.store(false);
        return;
    }
    const int response = http.GET();
    const int reportedSize = http.getSize();
    if (response != HTTP_CODE_OK || reportedSize <= 0 ||
        static_cast<size_t>(reportedSize) != release.size) {
        setStatus(Status::Failed, "firmware download length did not match manifest");
        http.end();
        busy.store(false);
        return;
    }
    if (!Update.begin(release.size, U_FLASH) || !Update.setMD5(release.md5.c_str())) {
        setStatus(Status::Failed, String("cannot start update: ") + Update.errorString());
        if (Update.isRunning()) Update.abort();
        http.end();
        busy.store(false);
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[DOWNLOAD_CHUNK_BYTES];
    size_t written = 0;
    unsigned long lastProgressAt = millis();
    bool failed = false;
    while (written < release.size) {
        if (millis() - lastProgressAt > DOWNLOAD_STALL_TIMEOUT_MS) {
            setStatus(Status::Failed, "firmware download stalled");
            failed = true;
            break;
        }
        const int available = stream->available();
        if (available <= 0) {
            if (!client.connected()) break;
            vTaskDelay(1);
            continue;
        }
        const size_t wanted = std::min(static_cast<size_t>(available),
            std::min(DOWNLOAD_CHUNK_BYTES, release.size - written));
        const size_t read = stream->readBytes(buffer, wanted);
        if (read == 0) continue;
        if (Update.write(buffer, read) != read) {
            setStatus(Status::Failed, String("firmware flash write failed: ") + Update.errorString());
            failed = true;
            break;
        }
        written += read;
        lastProgressAt = millis();
    }
    http.end();
    if (failed || written != release.size || !Update.end()) {
        if (!failed && written != release.size) setStatus(Status::Failed, "firmware download was truncated");
        if (!failed && written == release.size) {
            setStatus(Status::Failed, String("firmware verification failed: ") + Update.errorString());
        }
        if (Update.isRunning()) Update.abort();
        releaseAvailable.store(false);
        busy.store(false);
        return;
    }
    releaseAvailable.store(false);
    setStatus(Status::Installed, "Installed " + release.version + "; restarting");
    Serial.printf("[update] installed %s\n", release.version.c_str());
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

void FirmwareUpdater::setStatus(Status status, const String& message) {
    // A failed image must be rediscovered through a fresh manifest check. This
    // prevents automatic mode from being stranded behind one broken download
    // and lets manual mode use Check now after an error.
    if (status == Status::Failed) releaseAvailable.store(false);
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        currentStatus = status;
        statusMessage = message;
        xSemaphoreGive(stateMutex);
    }
    if (status == Status::Failed) Serial.printf("[update] %s\n", message.c_str());
}

FirmwareUpdater::Snapshot FirmwareUpdater::snapshot() const {
    Snapshot result;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        result.status = currentStatus;
        result.message = statusMessage;
        result.availableVersion = pendingRelease.version;
        result.notes = pendingRelease.notes;
        xSemaphoreGive(stateMutex);
    }
    result.autoInstall = autoInstall.load();
    result.busy = busy.load();
    result.awaitingConfirmation = releaseAvailable.load() && !result.autoInstall && !result.busy;
    return result;
}

FirmwareUpdater::Release FirmwareUpdater::pendingReleaseCopy() const {
    Release result;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        result = pendingRelease;
        xSemaphoreGive(stateMutex);
    }
    return result;
}

const char* FirmwareUpdater::statusName(Status status) {
    switch (status) {
    case Status::Checking: return "checking";
    case Status::UpToDate: return "up-to-date";
    case Status::UpdateFound: return "update-found";
    case Status::Downloading: return "downloading";
    case Status::Installed: return "installed";
    case Status::Failed: return "failed";
    case Status::Idle:
    default: return "idle";
    }
}

FirmwareUpdater::Version FirmwareUpdater::parseVersion(const String& raw) {
    String text = raw;
    text.trim();
    if (text.startsWith("v") || text.startsWith("V")) text.remove(0, 1);
    const int suffix = text.indexOf('-');
    if (suffix >= 0) text.remove(suffix);
    const int firstDot = text.indexOf('.');
    const int secondDot = firstDot < 0 ? -1 : text.indexOf('.', firstDot + 1);
    if (firstDot <= 0 || secondDot <= firstDot + 1 || secondDot == static_cast<int>(text.length() - 1) ||
        text.indexOf('.', secondDot + 1) >= 0) return {};
    const String fields[] = {text.substring(0, firstDot), text.substring(firstDot + 1, secondDot),
                             text.substring(secondDot + 1)};
    Version result;
    uint32_t* values[] = {&result.major, &result.minor, &result.patch};
    for (size_t index = 0; index < 3; ++index) {
        if (fields[index].isEmpty() || fields[index].length() > 9) return {};
        for (size_t character = 0; character < fields[index].length(); ++character) {
            if (!isdigit(static_cast<unsigned char>(fields[index][character]))) return {};
        }
        *values[index] = static_cast<uint32_t>(strtoul(fields[index].c_str(), nullptr, 10));
    }
    result.valid = true;
    return result;
}

bool FirmwareUpdater::isNewer(const Version& candidate, const Version& current) {
    if (candidate.major != current.major) return candidate.major > current.major;
    if (candidate.minor != current.minor) return candidate.minor > current.minor;
    return candidate.patch > current.patch;
}
