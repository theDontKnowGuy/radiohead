#pragma once
#include <Arduino.h>
#include <cstring>
#include <map>
#include <vector>

// Bounded in-memory NVS fixture; no real credentials or device storage access.
namespace fakeNvs {
inline std::map<std::string, std::vector<uint8_t>> blobs;
inline std::map<std::string, std::string> strings;
inline bool failOpen = false;
inline bool failWrite = false;
inline bool failRead = false;
inline int writes = 0;
inline void reset() {
    blobs.clear(); strings.clear();
    failOpen = failWrite = failRead = false;
    writes = 0;
}
}

class Preferences {
public:
    bool begin(const char* name, bool = false) {
        return std::string(name) == "spotify" && !fakeNvs::failOpen;
    }
    void end() {}
    bool isKey(const char* key) {
        return fakeNvs::blobs.count(key) || fakeNvs::strings.count(key);
    }
    size_t getBytesLength(const char* key) {
        auto it = fakeNvs::blobs.find(key);
        return it == fakeNvs::blobs.end() ? 0 : it->second.size();
    }
    size_t getBytes(const char* key, void* out, size_t limit) {
        auto it = fakeNvs::blobs.find(key);
        if (fakeNvs::failRead || it == fakeNvs::blobs.end() || it->second.size() > limit) return 0;
        memcpy(out, it->second.data(), it->second.size());
        return it->second.size();
    }
    size_t getString(const char* key, char* out, size_t limit) {
        auto it = fakeNvs::strings.find(key);
        if (fakeNvs::failRead || it == fakeNvs::strings.end() || it->second.size() + 1 > limit) return 0;
        memcpy(out, it->second.c_str(), it->second.size() + 1);
        return it->second.size() + 1;
    }
    size_t putBytes(const char* key, const void* value, size_t length) {
        if (fakeNvs::failWrite) return 0;
        const auto* bytes = static_cast<const uint8_t*>(value);
        fakeNvs::blobs[key] = std::vector<uint8_t>(bytes, bytes + length);
        ++fakeNvs::writes;
        return length;
    }
};
