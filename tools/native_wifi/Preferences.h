#pragma once
#include <cassert>
#include <cstddef>
#include <cstring>

inline bool savedRoaming = true, roamingKeyExists = false;
inline bool failRoamingOpen = false, failRoamingWrite = false;
inline unsigned roamingWrites = 0;
struct Preferences {
    bool begin(const char* name, bool) {
        assert(std::strcmp(name, "radio") == 0);
        return !failRoamingOpen;
    }
    bool getBool(const char* key, bool fallback) {
        assert(std::strcmp(key, "wifiRoam") == 0);
        return roamingKeyExists ? savedRoaming : fallback;
    }
    size_t putBool(const char* key, bool value) {
        assert(std::strcmp(key, "wifiRoam") == 0);
        if (failRoamingWrite) return 0;
        ++roamingWrites;
        savedRoaming = value;
        roamingKeyExists = true;
        return sizeof(bool);
    }
    void end() {}
};
