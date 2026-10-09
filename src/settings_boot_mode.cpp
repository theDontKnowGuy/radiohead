#include "settings.h"

#include <Preferences.h>

#include "app_state.h"

namespace {
constexpr const char* kBootModeKey = "bootMode";
}

void loadBootModeSettings() {
    BootMode loaded = DEFAULT_BOOT_MODE;
    Preferences storage;
    if (storage.begin("radio", true)) {
        loaded = static_cast<BootMode>(storage.getUChar(
            kBootModeKey, static_cast<uint8_t>(DEFAULT_BOOT_MODE)));
        storage.end();
    }
    bootMode = isSupportedBootMode(loaded) ? loaded : DEFAULT_BOOT_MODE;
    configuredBootMode = bootMode;
}

bool saveBootMode(BootMode mode) {
    if (!isSupportedBootMode(mode)) return false;
    Preferences storage;
    if (!storage.begin("radio", false)) return false;
    const bool saved = storage.putUChar(kBootModeKey, static_cast<uint8_t>(mode)) == sizeof(uint8_t);
    storage.end();
    if (saved) configuredBootMode = mode;
    return saved;
}
