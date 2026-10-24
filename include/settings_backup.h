#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// Opaque NVS settings snapshot, not a firmware/partition image. Buffers contain
// credentials and must never be logged. All calls run on the Arduino loop.
namespace SettingsBackup {
constexpr size_t MaxBytes = 32768;
struct Buffer {
    std::unique_ptr<uint8_t[]> data;
    size_t size = 0;
    ~Buffer();
    bool allocate();
    void clear();
};
enum class RestoreResult { Ok, Invalid, SnapshotFailed, RolledBack, RollbackFailed };
bool capture(Buffer& output);
bool validate(const uint8_t* data, size_t size);
// Replaces all application namespaces, including keys absent from the file.
// Caller must reboot immediately on success, without servicing runtime saves.
RestoreResult restore(const uint8_t* data, size_t size);
}
