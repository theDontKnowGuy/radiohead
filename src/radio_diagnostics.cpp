#include "radio_diagnostics.h"
#include "radio_diagnostics_store.h"
#include "settings_radio_diagnostics.h"
#include "app_state.h"
#include "media.h"
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <time.h>

namespace {
RTC_NOINIT_ATTR RadioLog::Store retained;
std::atomic<uint32_t> signals{0}, pcmBlocks{0}, allocationFailures{0}, failedBytes{0};
std::atomic<uint32_t> disconnects{0}, disconnectReason{0};
std::atomic<bool> detailedEnabled{true};
bool ready = false, filesystem = false, recovered = false, archiveOk = true;
uint32_t sequence = 0, sampleIndex = 0, eventIndex = 0;
uint32_t phaseAtUs = 0, loopAtUs = 0, gapMax = 0, audioMax = 0, webMax = 0;
uint32_t phaseMax = 0, slowPhase = 0, sampleAt = 0, eventAt = 0, pollAt = 0;
uint32_t previousPcm = 0, pcmAt = 0, inputMin = UINT32_MAX;
uint32_t previousDisconnects = 0, previousFailures = 0, pending = 0;
int previousState = -1, previousStation = -2, previousWifi = -1;
bool stalled = false, wasPlaying = false;
RadioLog::Record preceding{};
bool inputArmed = false;
uint8_t apIds[8][6]{};
uint32_t apCount = 0;
// Single producer (Arduino event task), single consumer (loop). Publish whole
// entries, preserving the earliest event if the loop is delayed. No RTC or
// network access from the callback. Overflow is explicit, never silent.
struct NetworkEvent { uint32_t ms, reason, gotIp; };
NetworkEvent networkQueue[32]{};
std::atomic<uint32_t> networkWrite{0}, networkRead{0}, networkDropped{0};
void publishNetworkEvent(uint32_t reason, bool gotIp) {
    const uint32_t write = networkWrite.load(std::memory_order_relaxed);
    if (write - networkRead.load(std::memory_order_acquire) >= 32) {
        networkDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    networkQueue[write % 32] = {uint32_t(millis()), reason, gotIp ? 1U : 0U};
    networkWrite.store(write + 1, std::memory_order_release);
}

void networkTick(const RadioLog::Record& snapshot, bool playing, bool radio) {
    using namespace RadioLog;
    RadioLog::Network n = retained.network;
    bool changed = false;
    const uint32_t now = snapshot.ms;
    auto start = [&](uint32_t at) {
        if (n.flags & Incident) return;
        n.flags = Incident;
        n.firstMs = at;
        const uint32_t age = now - at;
        n.firstUtc = snapshot.utc && age < 86400000 ? snapshot.utc - age / 1000 : 0;
        n.before = preceding;
        n.first = snapshot;
        n.incidentRoam = n.latestRoam;
        changed = true;
    };
    uint32_t read = networkRead.load(std::memory_order_relaxed);
    const uint32_t write = networkWrite.load(std::memory_order_acquire);
    while (read != write) {
        const auto event = networkQueue[read++ % 32];
        networkRead.store(read, std::memory_order_release);
        if (event.gotIp && n.latestRoam.outcome && n.latestRoam.outcome != 2 && n.latestRoam.outcome != 3) {
            n.latestRoam.finishedMs = event.ms;
            n.latestRoam.connectedAp = radioDiagnosticsApId(WiFi.BSSID());
            n.latestRoam.outcome = n.latestRoam.connectedAp &&
                n.latestRoam.connectedAp == n.latestRoam.targetAp ? 2 : 3;
            changed = true;
        }
        if (n.flags & Complete) continue;
        if (!event.gotIp) {
            start(event.ms);
            if (!(n.flags & Disconnected)) {
                n.firstDisconnectMs = event.ms;
                n.firstReason = event.reason;
            }
            n.flags = (n.flags | Disconnected) & ~uint32_t(WifiRecovered | PcmRecovered);
            n.wifiRecoveredMs = n.pcmRecoveredMs = n.outageMs = 0;
            n.lastDisconnectMs = event.ms;
            n.lastReason = event.reason;
            if (n.disconnectCount != UINT32_MAX) ++n.disconnectCount;
            bool counted = false;
            for (auto& bin : n.reasons) {
                if (!bin.count || bin.code == event.reason) {
                    bin.code = event.reason;
                    if (bin.count != UINT32_MAX) ++bin.count;
                    counted = true; break;
                }
            }
            if (!counted && n.reasonOverflow != UINT32_MAX) ++n.reasonOverflow;
            // Read driver configuration, retain ONLY numeric diagnostics. The
            // stack-local configuration also contains credentials: never export it.
            wifi_config_t config{};
            n.configuredPinned = UINT32_MAX;
            n.configuredAp = n.configuredChannel = 0;
            if (esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK) {
                n.configuredPinned = config.sta.bssid_set ? 1 : 0;
                n.configuredAp = config.sta.bssid_set ? radioDiagnosticsApId(config.sta.bssid) : 0;
                n.configuredChannel = config.sta.channel;
            }
            n.autoReconnect = WiFi.getAutoReconnect() ? 1 : 0;
            changed = true;
        } else if (n.flags & Disconnected) {
            n.flags |= WifiRecovered;
            n.wifiRecoveredMs = event.ms;
            n.outageMs = event.ms - n.firstDisconnectMs;
            n.recoveredAp = radioDiagnosticsApId(WiFi.BSSID());
            n.recoveredChannel = WiFi.channel();
            changed = true;
        }
    }
    if (!(n.flags & Complete)) {
        // Do not classify startup, a manual stop or a station change as an empty
        // buffer failure. Connecting after an established stream remains eligible.
        if (!radio || snapshot.station != preceding.station ||
            (snapshot.state != int(PlaybackState::Playing) && snapshot.state != int(PlaybackState::Connecting))) inputArmed = false;
        if (inputArmed && snapshot.input == 0 && !(n.flags & Empty)) {
            start(now); n.flags |= Empty; n.emptyMs = now; changed = true;
        }
        if (playing && snapshot.input > 0) inputArmed = true;
        if ((n.flags & Incident) && (!(n.flags & Disconnected) || (n.flags & WifiRecovered)) &&
            playing && snapshot.input > 0 && snapshot.pcmBlocks != preceding.pcmBlocks) {
            n.flags |= PcmRecovered | Complete;
            n.pcmRecoveredMs = now; changed = true;
        }
        if ((n.flags & WifiRecovered) && !(n.before.flags & 8U) && !(n.flags & Empty)) {
            n.flags |= Complete; changed = true;
        }
        const uint32_t dropped = networkDropped.load(std::memory_order_relaxed);
        if (n.queueDropped != dropped) { n.queueDropped = dropped; changed = true; }
    }
    if (n.latestRoam.outcome == 1 && now - n.latestRoam.startedMs >= 30000) {
        // Diagnostic timeout only. The driver still owns retries; no fallback.
        n.latestRoam.outcome = 4; changed = true;
    }
    if (changed) {
        if (!(retained.network.flags & Complete)) n.lastMs = now;
        commit(retained.network, n);
    }
    preceding = snapshot;
}

void allocationFailed(size_t bytes, uint32_t, const char*) {
    radioDiagnosticsAllocationFailure(bytes);
}

void save(RadioLog::Record record, bool event) {
    record.sequence = ++sequence;
    const size_t slot = event ? RadioLog::Samples + eventIndex++ % RadioLog::Events
                              : sampleIndex++ % RadioLog::Samples;
    RadioLog::commit(retained.records[slot], record);
}
} // namespace

uint32_t radioDiagnosticsApId(const uint8_t* bssid) {
    if (!bssid) return 0;
    const uint8_t empty[6]{};
    if (!memcmp(bssid, empty, sizeof(empty))) return 0;
    for (uint32_t i = 0; i < apCount; ++i) if (!memcmp(apIds[i], bssid, 6)) return i + 1;
    if (apCount == 8) return 0;
    memcpy(apIds[apCount++], bssid, 6);
    return apCount;
}
void radioDiagnosticsRoamStart(const uint8_t* from, const uint8_t* target,
                               int32_t channel, int32_t fromRssi, int32_t targetRssi) {
    if (!ready || !radioDiagnosticsDetailedEnabled()) return;
    auto n = retained.network;
    n.latestRoam = {uint32_t(millis()), radioDiagnosticsApId(from), radioDiagnosticsApId(target),
                    uint32_t(channel), fromRssi, targetRssi, -1, 0, 0, 1};
    RadioLog::commit(retained.network, n);
}
void radioDiagnosticsRoamResult(int32_t status) {
    if (!ready || !radioDiagnosticsDetailedEnabled()) return;
    auto n = retained.network;
    n.latestRoam.beginStatus = status;
    RadioLog::commit(retained.network, n);
}

bool radioDiagnosticsDetailedEnabled() { return detailedEnabled.load(std::memory_order_relaxed); }

bool radioDiagnosticsSetDetailedEnabled(bool enabled) {
    if (!ready) return false;
    if (enabled == radioDiagnosticsDetailedEnabled()) return true;
    if (!RadioLog::saveDetailedEnabled(enabled)) return false;
    detailedEnabled.store(enabled, std::memory_order_relaxed);
    inputArmed = false;
    preceding = {};
    if (retained.network.flags & RadioLog::Incident) {
        auto n = retained.network;
        n.loggingInterrupted = 1;
        RadioLog::commit(retained.network, n);
    }
    signals.exchange(0, std::memory_order_relaxed);
    pending = 0;
    gapMax = audioMax = webMax = phaseMax = 0;
    loopAtUs = 0;
    phaseAtUs = micros();
    sampleAt = eventAt = pollAt = pcmAt = millis();
    inputMin = UINT32_MAX;
    wasPlaying = stalled = false;
    previousState = -1;
    previousStation = -2;
    previousWifi = -1;
    previousDisconnects = disconnects.load(std::memory_order_relaxed);
    previousFailures = allocationFailures.load(std::memory_order_relaxed);
    // 0 means legacy detailed mode, 1 enabled, 2 disabled. This uses an existing
    // reserved byte so previous firmware archives remain readable.
    auto header = retained.header;
    header.reserved[0] = enabled ? 1 : 2;
    RadioLog::commit(retained.header, header);
    return true;
}

void radioDiagnosticsSignal(uint32_t events) {
    if (radioDiagnosticsDetailedEnabled()) signals.fetch_or(events, std::memory_order_relaxed);
}
void radioDiagnosticsPcmBlock() {
    if (!radioDiagnosticsDetailedEnabled()) return;
    // Only the local PCM task writes this counter; publish with one 32-bit store.
    static uint32_t blocks = 0;
    pcmBlocks.store(++blocks, std::memory_order_relaxed);
}
void radioDiagnosticsAllocationFailure(size_t bytes) {
    if (!radioDiagnosticsDetailedEnabled()) return;
    allocationFailures.fetch_add(1, std::memory_order_relaxed);
    failedBytes.store(bytes, std::memory_order_relaxed);
}

void radioDiagnosticsBegin(bool filesystemReady) {
    filesystem = filesystemReady;
    const auto reason = esp_reset_reason();
    // Power/brownout can leave random or stale RTC bytes. Never present them as
    // evidence of the immediately previous session, even if checksums match.
    recovered = reason != ESP_RST_POWERON && reason != ESP_RST_BROWNOUT &&
        RadioLog::validHeader(retained.header);
    const uint32_t boot = recovered ? retained.header.boot + 1 : 1;
    if (recovered) {
        // An older firmware has no extension; never archive unchecked trailing
        // RTC bytes as network evidence after an upgrade or a torn commit.
        if (retained.network.version != 1 || !RadioLog::valid(retained.network)) retained.network = {};
        const bool crash = reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
            reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT;
        // Keep the most recent crash independently of subsequent normal boots.
        archiveOk = filesystem && RadioLog::archive(retained, reason, false);
        if (crash) archiveOk = filesystem && RadioLog::archive(retained, reason, true) && archiveOk;
    }
    std::memset(&retained, 0, sizeof(retained));
    RadioLog::Header header{};
    header.magic = RadioLog::Magic;
    header.boot = boot;
    header.resetReason = reason;
    detailedEnabled.store(RadioLog::loadDetailedEnabled(), std::memory_order_relaxed);
    header.reserved[0] = radioDiagnosticsDetailedEnabled() ? 1 : 2;
    const auto* description = esp_app_get_description();
    for (size_t i = 0; i < 8; ++i) snprintf(header.build + i * 2, 3, "%02x", description->app_elf_sha256[i]);
    RadioLog::commit(retained.header, header);
    RadioLog::Network network{};
    network.version = 1;
    RadioLog::commit(retained.network, network);
    preceding = {};
    inputArmed = false;
    apCount = 0;
    ready = true;
    retained.phaseInverse = ~uint32_t(RadioPhase::Boot);
    phaseAtUs = micros();
    // Validation builds replace this hook with a forwarding hook so both
    // consumers keep their counters; no allocation or logging in either hook.
    heap_caps_register_failed_alloc_callback(allocationFailed);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        if (!radioDiagnosticsDetailedEnabled()) return;
        disconnectReason.store(info.wifi_sta_disconnected.reason, std::memory_order_relaxed);
        disconnects.fetch_add(1, std::memory_order_relaxed);
        publishNetworkEvent(info.wifi_sta_disconnected.reason, false);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
        if (radioDiagnosticsDetailedEnabled()) publishNetworkEvent(0, true);
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void radioDiagnosticsPhase(RadioPhase phase) {
    if (!ready) return;
    const uint32_t now = micros();
    const uint32_t elapsed = now - phaseAtUs;
    const uint32_t old = retained.phase;
    const bool detailed = radioDiagnosticsDetailedEnabled();
    if (detailed && old != uint32_t(RadioPhase::Boot)) {
        if (elapsed > phaseMax) { phaseMax = elapsed; slowPhase = old; }
        if (old == uint32_t(RadioPhase::Audio)) audioMax = std::max(audioMax, elapsed);
        if (old == uint32_t(RadioPhase::Web)) webMax = std::max(webMax, elapsed);
    }
    if (detailed && phase == RadioPhase::Audio) {
        if (loopAtUs) gapMax = std::max(gapMax, now - loopAtUs);
        loopAtUs = now;
    }
    retained.phaseInverse = uint32_t(phase); // invalid until committed
    retained.phaseMs = millis();
    retained.phase = uint32_t(phase);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    retained.phaseInverse = ~uint32_t(phase);
    phaseAtUs = now;
}

void radioDiagnosticsTick() {
    if (!ready || !radioDiagnosticsDetailedEnabled()) return;
    const uint32_t now = millis();
    if (now - pollAt < 250) return;
    pollAt = now;
    const bool local = mediaLocalAudioAvailable();
    const bool radio = local && !podcastMode && (mediaRequestedStation() >= 0 || mediaTestActive());
    const int state = radio ? int(mediaPlaybackState()) : -1;
    const int station = radio ? mediaRequestedStation() : -1;
    const int wifi = WiFi.status();
    const bool playing = radio && state == int(PlaybackState::Playing);
    const uint32_t blocks = pcmBlocks.load(std::memory_order_relaxed);
    if (!playing || !wasPlaying || blocks != previousPcm) pcmAt = now;
    previousPcm = blocks;
    wasPlaying = playing;
    const bool noPcm = playing && now - pcmAt >= 1000;
    pending |= signals.exchange(0, std::memory_order_relaxed);
    if (noPcm != stalled) {
        if (playing) pending |= noPcm ? RadioPcmStall : RadioPcmResumed;
        stalled = noPcm;
    }
    if (state != previousState || station != previousStation) pending |= RadioState;
    previousState = state;
    previousStation = station;
    const uint32_t disconnected = disconnects.load(std::memory_order_relaxed);
    if (wifi != previousWifi || disconnected != previousDisconnects) pending |= RadioWifi;
    previousWifi = wifi;
    previousDisconnects = disconnected;
    const uint32_t failures = allocationFailures.load(std::memory_order_relaxed);
    if (failures != previousFailures) pending |= RadioAllocation;
    previousFailures = failures;
    if (gapMax >= 100000) pending |= RadioLoopGap;
    const uint32_t input = local ? audio.inBufferFilled() : 0;
    inputMin = std::min(inputMin, input);
    const bool sampleDue = now - sampleAt >= 5000;
    const bool eventDue = pending && now - eventAt >= 1000;

    RadioLog::Record record{};
    record.ms = now;
    const time_t utc = time(nullptr);
    record.utc = utc >= 1483228800 && utc <= UINT32_MAX ? uint32_t(utc) : 0;
    record.events = pending;
    record.station = station; record.state = state;
    record.wifi = wifi; record.rssi = wifi == WL_CONNECTED ? WiFi.RSSI() : 0;
    record.input = input; record.inputMin = inputMin;
    record.capacity = local ? audio.getInBufferSize() : 0;
    record.pcmBlocks = blocks; record.pcmIdleMs = playing ? now - pcmAt : 0;
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    multi_heap_info_t heap{};
    heap_caps_get_info(&heap, caps);
    record.heapFree = heap.total_free_bytes; record.heapMin = heap.minimum_free_bytes;
    record.heapLargest = heap.largest_free_block;
    record.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    record.allocationFailures = failures; record.failedBytes = failedBytes.load(std::memory_order_relaxed);
    record.loopGapUs = gapMax; record.audioUs = audioMax; record.webUs = webMax;
    record.slowPhase = slowPhase; record.phaseUs = phaseMax;
    record.disconnects = disconnected; record.disconnectReason = disconnectReason.load(std::memory_order_relaxed);
    record.flags = (local && audio.isRunning() ? 1U : 0U) | (radioMuted ? 2U : 0U) |
        (mainVal == 0 ? 4U : 0U) | (radio ? 8U : 0U);
    networkTick(record, playing, radio);
    if (!sampleDue && !eventDue) return;
    if (eventDue) { save(record, true); eventAt = now; pending = 0; }
    if (sampleDue) { save(record, false); sampleAt = now; }
    gapMax = audioMax = webMax = phaseMax = 0;
    inputMin = UINT32_MAX;
}

namespace {
void appendNetworkJson(String& json, const RadioLog::Network& n) {
    if (n.version != 1 || !RadioLog::valid(n)) { json += ",\"network\":null"; return; }
    char text[1600];
    snprintf(text, sizeof(text),
        ",\"network\":{\"version\":1,\"flags\":%u,\"firstMs\":%u,\"firstUtc\":%u,\"lastMs\":%u,"
        "\"firstDisconnectMs\":%u,\"lastDisconnectMs\":%u,\"firstReason\":%u,\"lastReason\":%u,\"disconnectCount\":%u,"
        "\"emptyMs\":%u,\"wifiRecoveredMs\":%u,\"pcmRecoveredMs\":%u,\"outageMs\":%u,"
        "\"reasonOverflow\":%u,\"queueDropped\":%u,\"loggingInterrupted\":%u,"
        "\"configuredPinned\":%u,\"configuredAp\":%u,\"configuredChannel\":%u,\"autoReconnect\":%u,"
        "\"recoveredAp\":%u,\"recoveredChannel\":%u,\"reasons\":[",
        n.flags, n.firstMs, n.firstUtc, n.lastMs, n.firstDisconnectMs, n.lastDisconnectMs,
        n.firstReason, n.lastReason, n.disconnectCount, n.emptyMs, n.wifiRecoveredMs,
        n.pcmRecoveredMs, n.outageMs, n.reasonOverflow, n.queueDropped, n.loggingInterrupted,
        n.configuredPinned, n.configuredAp, n.configuredChannel, n.autoReconnect, n.recoveredAp, n.recoveredChannel);
    json += text;
    bool comma = false;
    for (const auto& bin : n.reasons) {
        if (!bin.count) continue;
        snprintf(text, sizeof(text), "%s{\"reason\":%u,\"count\":%u}", comma ? "," : "", bin.code, bin.count);
        json += text; comma = true;
    }
    json += "]";
    auto snapshot = [&](const char* name, const RadioLog::Record& r) {
        snprintf(text, sizeof(text),
            ",\"%s\":{\"ms\":%u,\"utc\":%u,\"station\":%d,\"state\":%d,\"wifi\":%d,\"rssi\":%d,"
            "\"input\":%u,\"pcmBlocks\":%u,\"heapFree\":%u,\"loopGapUs\":%u,\"events\":%u,\"flags\":%u}",
            name, r.ms, r.utc, r.station, r.state, r.wifi, r.rssi, r.input, r.pcmBlocks,
            r.heapFree, r.loopGapUs, r.events, r.flags);
        json += text;
    };
    snapshot("before", n.before); snapshot("first", n.first);
    auto roam = [&](const char* name, const RadioLog::Roam& r) {
        snprintf(text, sizeof(text),
            ",\"%s\":{\"startedMs\":%u,\"fromAp\":%u,\"targetAp\":%u,\"channel\":%u,"
            "\"fromRssi\":%d,\"targetRssi\":%d,\"beginStatus\":%d,\"finishedMs\":%u,\"connectedAp\":%u,\"outcome\":%u}",
            name, r.startedMs, r.fromAp, r.targetAp, r.channel, r.fromRssi, r.targetRssi,
            r.beginStatus, r.finishedMs, r.connectedAp, r.outcome);
        json += text;
    };
    roam("incidentRoam", n.incidentRoam); roam("latestRoam", n.latestRoam);
    json += "}";
}
} // namespace

String radioDiagnosticsJson(const String& bank, size_t offset) {
    if (!ready || offset >= RadioLog::Count || (bank != "live" && bank != "previous" && bank != "crash")) return String();
    RadioLog::Header header{};
    RadioLog::Record page[RadioLog::PageSize]{};
    RadioLog::Network network{};
    uint32_t phase = 0, phaseMs = 0, endingReset = 0;
    bool available = true;
    const size_t count = std::min(RadioLog::PageSize, RadioLog::Count - offset);
    if (bank == "live") {
        header = retained.header;
        if (offset == 0) network = retained.network;
        phase = retained.phase; phaseMs = retained.phaseMs;
        std::memcpy(page, retained.records + offset, count * sizeof(page[0]));
    } else {
        available = filesystem && RadioLog::readArchive(bank == "crash", offset, header, phase, phaseMs, endingReset, page, offset == 0 ? &network : nullptr);
    }
    if (!available) { header = {}; phase = phaseMs = endingReset = 0; }
    String json;
    if (!json.reserve(offset == 0 ? 12000 : 8000)) return String();
    char text[1000];
    snprintf(text, sizeof(text),
        "{\"schema\":1,\"bank\":\"%s\",\"available\":%s,\"rtcRecovered\":%s,\"archiveOk\":%s,\"filesystemReady\":%s,\"detailedEnabled\":%s,"
        "\"boot\":%u,\"build\":\"%.16s\",\"bootResetReason\":%u,\"endingResetReason\":%u,"
        "\"lastPhase\":%u,\"lastPhaseMs\":%u,\"offset\":%u,\"nextOffset\":%d,\"records\":[",
        bank.c_str(), available ? "true" : "false", recovered ? "true" : "false", archiveOk ? "true" : "false", filesystem ? "true" : "false",
        header.reserved[0] == 2 ? "false" : "true",
        header.boot, header.build, header.resetReason, endingReset, phase, phaseMs, unsigned(offset),
        offset + count < RadioLog::Count ? int(offset + count) : -1);
    json += text;
    bool comma = false;
    for (size_t i = 0; available && i < count; ++i) {
        const auto& r = page[i];
        if (!RadioLog::valid(r)) continue;
        snprintf(text, sizeof(text),
            "%s{\"seq\":%u,\"ms\":%u,\"utc\":%u,\"events\":%u,\"station\":%d,\"state\":%d,\"wifi\":%d,\"rssi\":%d,"
            "\"input\":%u,\"inputMin\":%u,\"capacity\":%u,\"pcmBlocks\":%u,\"pcmIdleMs\":%u,"
            "\"heapFree\":%u,\"heapMin\":%u,\"heapLargest\":%u,\"psramFree\":%u,\"allocationFailures\":%u,\"failedBytes\":%u,"
            "\"loopGapUs\":%u,\"audioUs\":%u,\"webUs\":%u,\"slowPhase\":%u,\"phaseUs\":%u,"
            "\"disconnects\":%u,\"disconnectReason\":%u,\"flags\":%u}",
            comma ? "," : "", r.sequence, r.ms, r.utc, r.events, r.station, r.state, r.wifi, r.rssi,
            r.input, r.inputMin, r.capacity, r.pcmBlocks, r.pcmIdleMs,
            r.heapFree, r.heapMin, r.heapLargest, r.psramFree, r.allocationFailures, r.failedBytes,
            r.loopGapUs, r.audioUs, r.webUs, r.slowPhase, r.phaseUs, r.disconnects, r.disconnectReason, r.flags);
        json += text;
        comma = true;
    }
    json += "]";
    if (offset == 0) appendNetworkJson(json, network);
    json += "}";
    return json;
}
