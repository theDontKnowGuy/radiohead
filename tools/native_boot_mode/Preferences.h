#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace fakeNvs {
inline std::map<std::string, std::variant<uint8_t, std::string>> values;
inline bool failOpen = false;
inline bool failWrite = false;
inline int writes = 0;
}

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false) {
        namespace_ = name;
        readOnly_ = readOnly;
        return !fakeNvs::failOpen;
    }
    void end() {}
    uint8_t getUChar(const char* key, uint8_t fallback) {
        const auto it = fakeNvs::values.find(namespace_ + "/" + key);
        if (it == fakeNvs::values.end()) return fallback;
        const auto* value = std::get_if<uint8_t>(&it->second);
        return value ? *value : fallback;
    }
    size_t putUChar(const char* key, uint8_t value) {
        if (readOnly_ || fakeNvs::failWrite) return 0;
        fakeNvs::values[namespace_ + "/" + key] = value;
        ++fakeNvs::writes;
        return sizeof(value);
    }
private:
    std::string namespace_;
    bool readOnly_ = false;
};
