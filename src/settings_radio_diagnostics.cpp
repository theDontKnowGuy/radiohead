#include "settings_radio_diagnostics.h"
#include <LittleFS.h>
#include <Preferences.h>

namespace RadioLog {
namespace {
const char* path(bool crash) { return crash ? "/radio-crash.bin" : "/radio-previous.bin"; }
constexpr const char* Temporary = "/radio-log.tmp";
}

bool loadDetailedEnabled() {
    Preferences storage;
    if (!storage.begin("radio", true)) return true;
    const bool enabled = storage.getBool("radioLog", true);
    storage.end();
    return enabled;
}

bool saveDetailedEnabled(bool enabled) {
    Preferences storage;
    if (!storage.begin("radio", false)) return false;
    const bool saved = storage.putBool("radioLog", enabled) == sizeof(bool);
    storage.end();
    return saved;
}

bool archive(const Store& store, uint32_t endingReset, bool crash) {
    if (!validHeader(store.header)) return false;
    File file = LittleFS.open(Temporary, FILE_WRITE);
    if (!file) return false;
    const bool written = file.write(reinterpret_cast<const uint8_t*>(&store), sizeof(store)) == sizeof(store) &&
        file.write(reinterpret_cast<const uint8_t*>(&endingReset), sizeof(endingReset)) == sizeof(endingReset);
    file.close();
    // Verify the entire temporary image before atomically replacing the old
    // archive. Never delete the old file first or format diagnostic storage.
    file = LittleFS.open(Temporary, FILE_READ);
    bool verified = written && file && file.size() == sizeof(store) + sizeof(endingReset);
    uint8_t bytes[128];
    const auto* source = reinterpret_cast<const uint8_t*>(&store);
    for (size_t at = 0; verified && at < sizeof(store); at += sizeof(bytes)) {
        const size_t count = std::min(sizeof(bytes), sizeof(store) - at);
        verified = file.read(bytes, count) == count && std::memcmp(bytes, source + at, count) == 0;
    }
    uint32_t savedReset = 0;
    if (verified) verified = file.read(reinterpret_cast<uint8_t*>(&savedReset), sizeof(savedReset)) == sizeof(savedReset) && savedReset == endingReset;
    file.close();
    if (verified && LittleFS.rename(Temporary, path(crash))) return true;
    LittleFS.remove(Temporary);
    return false;
}

bool readArchive(bool crash, size_t offset, Header& header, uint32_t& phase,
                 uint32_t& phaseMs, uint32_t& endingReset, Record* page, Network* network) {
    if (offset >= Count || !page) return false;
    File file = LittleFS.open(path(crash), FILE_READ);
    if (network) *network = {};
    const bool legacy = file && file.size() == LegacyStoreSize + sizeof(endingReset);
    if (!file || (!legacy && file.size() != sizeof(Store) + sizeof(endingReset)) ||
        file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        !validHeader(header)) return false;
    uint32_t breadcrumb[3];
    if (file.read(reinterpret_cast<uint8_t*>(breadcrumb), sizeof(breadcrumb)) != sizeof(breadcrumb)) return false;
    phase = breadcrumb[0] == ~breadcrumb[2] ? breadcrumb[0] : UINT32_MAX;
    phaseMs = breadcrumb[1];
    const size_t bytes = std::min(PageSize, Count - offset) * sizeof(Record);
    if (!file.seek(offsetof(Store, records) + offset * sizeof(Record)) ||
        file.read(reinterpret_cast<uint8_t*>(page), bytes) != bytes ||
        !file.seek(legacy ? LegacyStoreSize : sizeof(Store)) ||
        file.read(reinterpret_cast<uint8_t*>(&endingReset), sizeof(endingReset)) != sizeof(endingReset)) return false;
    if (network && !legacy) {
        if (!file.seek(offsetof(Store, network)) ||
            file.read(reinterpret_cast<uint8_t*>(network), sizeof(*network)) != sizeof(*network) ||
            network->version != 1 || !valid(*network)) *network = {};
    }
    return true;
}
} // namespace RadioLog
