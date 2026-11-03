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
#include <time.h>

namespace {
RTC_NOINIT_ATTR RadioLog::Store retained;
std::atomic<uint32_t> signals{0}, pcmBlocks{0}, allocationFailures{0}, failedBytes{0};
std::atomic<uint32_t> disconnects{0}, disconnectReason{0};
bool ready = false, filesystem = false, recovered = false, archiveOk = true;
uint32_t sequence = 0, sampleIndex = 0, eventIndex = 0;
uint32_t phaseAtUs = 0, loopAtUs = 0, gapMax = 0, audioMax = 0, webMax = 0;
uint32_t phaseMax = 0, slowPhase = 0, sampleAt = 0, eventAt = 0, pollAt = 0;
uint32_t previousPcm = 0, pcmAt = 0, inputMin = UINT32_MAX;
uint32_t previousDisconnects = 0, previousFailures = 0, pending = 0;
int previousState = -1, previousStation = -2, previousWifi = -1;
bool stalled = false, wasPlaying = false;

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

void radioDiagnosticsSignal(uint32_t events) { signals.fetch_or(events, std::memory_order_relaxed); }
void radioDiagnosticsPcmBlock() {
    // Only the local PCM task writes this counter; publish with one 32-bit store.
    static uint32_t blocks = 0;
    pcmBlocks.store(++blocks, std::memory_order_relaxed);
}
void radioDiagnosticsAllocationFailure(size_t bytes) {
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
    const auto* description = esp_app_get_description();
    for (size_t i = 0; i < 8; ++i) snprintf(header.build + i * 2, 3, "%02x", description->app_elf_sha256[i]);
    RadioLog::commit(retained.header, header);
    ready = true;
    retained.phaseInverse = ~uint32_t(RadioPhase::Boot);
    phaseAtUs = micros();
    // Validation builds replace this hook with a forwarding hook so both
    // consumers keep their counters; no allocation or logging in either hook.
    heap_caps_register_failed_alloc_callback(allocationFailed);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        disconnectReason.store(info.wifi_sta_disconnected.reason, std::memory_order_relaxed);
        disconnects.fetch_add(1, std::memory_order_relaxed);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void radioDiagnosticsPhase(RadioPhase phase) {
    if (!ready) return;
    const uint32_t now = micros();
    const uint32_t elapsed = now - phaseAtUs;
    const uint32_t old = retained.phase;
    if (old != uint32_t(RadioPhase::Boot)) {
        if (elapsed > phaseMax) { phaseMax = elapsed; slowPhase = old; }
        if (old == uint32_t(RadioPhase::Audio)) audioMax = std::max(audioMax, elapsed);
        if (old == uint32_t(RadioPhase::Web)) webMax = std::max(webMax, elapsed);
    }
    if (phase == RadioPhase::Audio) {
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
    if (!ready) return;
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
    if (!sampleDue && !eventDue) return;

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
    if (eventDue) { save(record, true); eventAt = now; pending = 0; }
    if (sampleDue) { save(record, false); sampleAt = now; }
    gapMax = audioMax = webMax = phaseMax = 0;
    inputMin = UINT32_MAX;
}

String radioDiagnosticsJson(const String& bank, size_t offset) {
    if (!ready || offset >= RadioLog::Count || (bank != "live" && bank != "previous" && bank != "crash")) return String();
    RadioLog::Header header{};
    RadioLog::Record page[RadioLog::PageSize]{};
    uint32_t phase = 0, phaseMs = 0, endingReset = 0;
    bool available = true;
    const size_t count = std::min(RadioLog::PageSize, RadioLog::Count - offset);
    if (bank == "live") {
        header = retained.header;
        phase = retained.phase; phaseMs = retained.phaseMs;
        std::memcpy(page, retained.records + offset, count * sizeof(page[0]));
    } else {
        available = filesystem && RadioLog::readArchive(bank == "crash", offset, header, phase, phaseMs, endingReset, page);
    }
    if (!available) { header = {}; phase = phaseMs = endingReset = 0; }
    String json;
    if (!json.reserve(8000)) return String();
    char text[1000];
    snprintf(text, sizeof(text),
        "{\"schema\":1,\"bank\":\"%s\",\"available\":%s,\"rtcRecovered\":%s,\"archiveOk\":%s,\"filesystemReady\":%s,"
        "\"boot\":%u,\"build\":\"%.16s\",\"bootResetReason\":%u,\"endingResetReason\":%u,"
        "\"lastPhase\":%u,\"lastPhaseMs\":%u,\"offset\":%u,\"nextOffset\":%d,\"records\":[",
        bank.c_str(), available ? "true" : "false", recovered ? "true" : "false", archiveOk ? "true" : "false", filesystem ? "true" : "false",
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
    json += "]}";
    return json;
}
