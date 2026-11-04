#include "settings.h"

#include <Preferences.h>

bool loadWifiRoamingEnabled() {
    Preferences storage;
    if (!storage.begin("radio", true)) return true;
    const bool enabled = storage.getBool("wifiRoam", true);
    storage.end();
    return enabled;
}

bool saveWifiRoamingEnabled(bool enabled) {
    Preferences storage;
    if (!storage.begin("radio", false)) return false;
    const bool saved = storage.putBool("wifiRoam", enabled) == sizeof(bool);
    storage.end();
    return saved;
}
