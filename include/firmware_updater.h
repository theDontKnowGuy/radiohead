#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Pull-based GitHub release updater. The check and transfer run on a low
// priority worker so the Arduino loop continues to service audio and HTTP.
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
        bool awaitingConfirmation = false;
        bool autoInstall = true;
        bool busy = false;
    };

    void begin(bool autoInstall);
    void setAutoInstall(bool enabled);
    void requestCheckNow();
    bool requestInstallNow();
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool isBusy() const { return busy.load(); }
    [[nodiscard]] static const char* statusName(Status status);

private:
    struct Release {
        String version;
        String notes;
        String url;
        String md5;
        size_t size = 0;
    };
    struct Version {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t patch = 0;
        bool valid = false;
    };

    std::atomic<bool> busy{false};
    std::atomic<bool> autoInstall{true};
    std::atomic<bool> releaseAvailable{false};
    std::atomic<bool> installRequested{false};
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t stateMutex = nullptr;
    Release pendingRelease;
    Status currentStatus = Status::Idle;
    String statusMessage;

    static void taskEntry(void* context);
    void taskLoop();
    void checkForUpdate();
    void installPendingRelease();
    void setStatus(Status status, const String& message);
    [[nodiscard]] Release pendingReleaseCopy() const;
    [[nodiscard]] static Version parseVersion(const String& text);
    [[nodiscard]] static bool isNewer(const Version& candidate, const Version& current);
};

extern FirmwareUpdater firmwareUpdater;
