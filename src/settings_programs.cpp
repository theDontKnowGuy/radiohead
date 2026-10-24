#include "podcast_catalog.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>
#include <memory>
#include <new>

#include "app_state.h"

namespace {
constexpr size_t kCatalogBytes = 16384;
bool catalogReadable = true;

bool textValid(const String& value, size_t limit) {
    if (value.isEmpty() || value.length() > limit) return false;
    for (size_t i = 0; i < value.length(); ++i)
        if (static_cast<uint8_t>(value[i]) < 32 || value[i] == 127) return false;
    return true;
}

bool slugValid(const String& slug) {
    if (slug.isEmpty() || slug.length() > 80) return false;
    for (size_t i = 0; i < slug.length(); ++i) {
        const char c = slug[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

bool sameSource(const PodcastShow& a, const PodcastShow& b) {
    return a.feedUrl == b.feedUrl && a.program == b.program && a.playlist == b.playlist;
}

bool persist(const PodcastShow* shows, uint16_t favorites) {
    if (!catalogReadable) return false; // Never overwrite an unreadable stored catalog.
    if (favorites & ~((1U << PODCAST_SHOW_COUNT) - 1U)) return false;
    for (int slot = 0; slot < PODCAST_SHOW_COUNT; ++slot) {
        const auto& show = shows[slot];
        if (show.favoriteId.isEmpty()) {
            if (!show.webName.isEmpty() || !show.tftName.isEmpty() || !show.program.isEmpty() ||
                !show.playlist.isEmpty() || !show.feedUrl.isEmpty() || (favorites & (1U << slot))) return false;
        } else {
            if (!textValid(show.favoriteId, 80) || !validPodcastShow(show)) return false;
            for (int previous = 0; previous < slot; ++previous)
                if (shows[previous].favoriteId == show.favoriteId || sameSource(shows[previous], show)) return false;
        }
    }
    JsonDocument doc;
    doc["version"] = 1;
    doc["favorites"] = favorites;
    JsonArray rows = doc["shows"].to<JsonArray>();
    for (int slot = 0; slot < PODCAST_SHOW_COUNT; ++slot) {
        const PodcastShow& show = shows[slot];
        JsonObject row = rows.add<JsonObject>();
        row["id"] = show.favoriteId;
        row["name"] = show.webName;
        row["tft"] = show.tftName;
        row["program"] = show.program;
        row["playlist"] = show.playlist;
        row["feed"] = show.feedUrl;
    }
    const size_t expected = measureJson(doc);
    if (doc.overflowed() || expected > kCatalogBytes) return false;
    String encoded;
    if (!encoded.reserve(expected + 1) || serializeJson(doc, encoded) != expected || encoded.length() != expected) return false;
    Preferences storage;
    if (!storage.begin("radio", false)) return false;
    // One NVS blob commits catalog, identity and show favorites together.
    const bool saved = storage.putBytes("programs", encoded.c_str(), encoded.length()) == encoded.length();
    storage.end();
    return saved;
}
}

bool validPodcastSourceUrl(const String& url) {
    if (!textValid(url, PODCAST_SOURCE_BYTES)) return false;
    const size_t start = url.startsWith("https://") ? 8 : url.startsWith("http://") ? 7 : 0;
    if (!start || start >= url.length()) return false;
    // Direct public feed/audio URLs only: no embedded credentials or whitespace.
    for (size_t i = start; i < url.length(); ++i)
        if (url[i] == '@' || url[i] == ' ' || url[i] == '\\' || url[i] == '#') return false;
    return url[start] != '/' && url[start] != '?' && url[start] != ':';
}

bool validPodcastShow(const PodcastShow& show) {
    return textValid(show.webName, PODCAST_NAME_BYTES) &&
        textValid(show.tftName, PODCAST_NAME_BYTES) &&
        (show.feedUrl.isEmpty()
            ? slugValid(show.program) && slugValid(show.playlist)
            : validPodcastSourceUrl(show.feedUrl) && show.program.isEmpty() && show.playlist.isEmpty());
}

bool isSavedPodcastShow(int slot) {
    return slot >= 0 && slot < PODCAST_SHOW_COUNT && !podcastShows[slot].favoriteId.isEmpty();
}

int savedPodcastShowCount() {
    int count = 0;
    for (int slot = 0; slot < PODCAST_SHOW_COUNT; ++slot) if (isSavedPodcastShow(slot)) ++count;
    return count;
}

uint32_t podcastCatalogRevision() {
    uint32_t hash = 2166136261U;
    for (const PodcastShow& show : podcastShows) {
        for (const String* value : {&show.favoriteId, &show.webName, &show.tftName,
                                   &show.program, &show.playlist, &show.feedUrl}) {
            for (size_t i = 0; i < value->length(); ++i) hash = (hash ^ static_cast<uint8_t>((*value)[i])) * 16777619U;
            hash = (hash ^ 0xffU) * 16777619U;
        }
    }
    return ((hash ^ podcastShowFavoriteMask) * 16777619U) & 0x7fffffffU;
}

void loadPodcastCatalog() {
    Preferences storage;
    // Writable open can create the namespace on a fresh installation, allowing
    // an actual open failure to remain a storage error instead of being confused
    // with an absent catalog. No catalog blob is written during loading.
    if (!storage.begin("radio", false)) { catalogReadable = false; return; }
    if (!storage.isKey("programs")) { storage.end(); catalogReadable = true; return; }
    catalogReadable = false;
    const size_t length = storage.getBytesLength("programs");
    std::unique_ptr<char[]> bytes(length && length <= kCatalogBytes ? new (std::nothrow) char[length] : nullptr);
    const bool read = bytes && storage.getBytes("programs", bytes.get(), length) == length;
    storage.end();
    if (!read) return;
    JsonDocument doc;
    if (deserializeJson(doc, bytes.get(), length, DeserializationOption::NestingLimit(4)) ||
        doc["version"] != 1 || !doc["favorites"].is<uint16_t>() ||
        !doc["shows"].is<JsonArray>() || doc["shows"].size() != PODCAST_SHOW_COUNT) return;
    PodcastShow loaded[PODCAST_SHOW_COUNT];
    uint16_t mask = doc["favorites"].as<uint16_t>();
    if (mask & ~((1U << PODCAST_SHOW_COUNT) - 1U)) return;
    for (int slot = 0; slot < PODCAST_SHOW_COUNT; ++slot) {
        JsonObject row = doc["shows"][slot];
        for (const char* key : {"id", "name", "tft", "program", "playlist", "feed"})
            if (!row[key].is<const char*>()) return;
        PodcastShow& show = loaded[slot];
        show = {row["id"].as<String>(), static_cast<uint8_t>(slot), row["name"].as<String>(),
                row["tft"].as<String>(), row["program"].as<String>(), row["playlist"].as<String>(), row["feed"].as<String>()};
        if (show.favoriteId.isEmpty()) {
            if (!show.webName.isEmpty() || !show.tftName.isEmpty() || !show.program.isEmpty() ||
                !show.playlist.isEmpty() || !show.feedUrl.isEmpty() || (mask & (1U << slot))) return;
            continue;
        }
        if (!textValid(show.favoriteId, 80) || !validPodcastShow(show)) return;
        for (int previous = 0; previous < slot; ++previous)
            if (loaded[previous].favoriteId == show.favoriteId || sameSource(loaded[previous], show)) return;
    }
    for (int slot = 0; slot < PODCAST_SHOW_COUNT; ++slot) podcastShows[slot] = loaded[slot];
    podcastShowFavoriteMask = mask;
    catalogReadable = true;
}

bool savePodcastCatalog() { return persist(podcastShows, podcastShowFavoriteMask); }

bool savePodcastShow(int slot, const PodcastShow& draft, bool favorite) {
    if (slot < 0 || slot >= PODCAST_SHOW_COUNT || !validPodcastShow(draft)) return false;
    for (int other = 0; other < PODCAST_SHOW_COUNT; ++other)
        if (other != slot && isSavedPodcastShow(other) && sameSource(podcastShows[other], draft)) return false;
    PodcastShow next[PODCAST_SHOW_COUNT];
    for (int i = 0; i < PODCAST_SHOW_COUNT; ++i) next[i] = podcastShows[i];
    const bool replaced = !isSavedPodcastShow(slot) || !sameSource(next[slot], draft);
    const String previousId = next[slot].favoriteId;
    next[slot] = draft;
    next[slot].favoriteSlot = slot;
    if (replaced) {
        char id[32];
        snprintf(id, sizeof(id), "custom-%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
        next[slot].favoriteId = id;
    } else next[slot].favoriteId = previousId;
    const uint16_t bit = 1U << slot;
    // The editor's explicit favorite choice applies to this new identity.
    const uint16_t mask = favorite ? podcastShowFavoriteMask | bit : podcastShowFavoriteMask & ~bit;
    if (!persist(next, mask)) return false;
    podcastShows[slot] = next[slot];
    podcastShowFavoriteMask = mask;
    return true;
}

bool removePodcastShow(int slot) {
    if (!isSavedPodcastShow(slot)) return false;
    PodcastShow next[PODCAST_SHOW_COUNT];
    for (int i = 0; i < PODCAST_SHOW_COUNT; ++i) next[i] = podcastShows[i];
    next[slot] = {};
    const uint16_t mask = podcastShowFavoriteMask & ~(1U << slot);
    if (!persist(next, mask)) return false;
    podcastShows[slot] = {};
    podcastShowFavoriteMask = mask;
    return true;
}
