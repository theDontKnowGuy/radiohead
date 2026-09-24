#include "validation_diagnostics.h"

#if defined(RADIO_VALIDATION_DIAGNOSTICS) && RADIO_VALIDATION_DIAGNOSTICS

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/task.h>

#include "app_state.h"
#include "media.h"

namespace {

constexpr uint32_t kSamplePeriodMs = 5000;
constexpr size_t kEventCount = 16;
constexpr uint32_t kGapBoundsUs[] = {1000, 2000, 5000, 10000, 20000, 50000, 100000};

struct Event {
    uint32_t atMs;
    const char* name;
};

struct GapStats {
    uint32_t buckets[8] = {};
    uint32_t maxUs = 0;
};

struct HeapSample {
    size_t free;
    size_t minimum;
    size_t largest;
};

Event events[kEventCount] = {};
size_t nextEvent = 0;
uint32_t lastSampleMs = 0;
int64_t lastLoopAtUs = 0;
GapStats loopGaps;
uint32_t maxAudioCallUs = 0;
uint32_t maxWebCallUs = 0;
std::atomic<uint32_t> failedAllocations{0};
std::atomic<uint32_t> lastFailedBytes{0};
std::atomic<uint32_t> lastFailedCaps{0};
uint32_t minimumInputFill = UINT32_MAX;

void failedAllocation(size_t requestedBytes, uint32_t capabilities, const char*) {
    failedAllocations.fetch_add(1, std::memory_order_relaxed);
    lastFailedBytes.store(static_cast<uint32_t>(requestedBytes), std::memory_order_relaxed);
    lastFailedCaps.store(capabilities, std::memory_order_relaxed);
}

void recordGap(GapStats& stats, uint32_t gapUs) {
    size_t bucket = 0;
    while (bucket < 7 && gapUs > kGapBoundsUs[bucket]) ++bucket;
    ++stats.buckets[bucket];
    if (gapUs > stats.maxUs) stats.maxUs = gapUs;
}

HeapSample sampleHeap(uint32_t capabilities) {
    multi_heap_info_t info = {};
    heap_caps_get_info(&info, capabilities);
    return {info.total_free_bytes, info.minimum_free_bytes, info.largest_free_block};
}

uint32_t stackReserve(const char* taskName) {
    const TaskHandle_t task = xTaskGetHandle(taskName);
    return task ? uxTaskGetStackHighWaterMark(task) : 0;
}

void dumpEvents() {
    for (size_t offset = 0; offset < kEventCount; ++offset) {
        const Event& event = events[(nextEvent + offset) % kEventCount];
        if (event.name) Serial.printf("[val-event] ms=%lu name=%s\n",
                                      static_cast<unsigned long>(event.atMs), event.name);
    }
}

}  // namespace

void validationBegin() {
    heap_caps_register_failed_alloc_callback(failedAllocation);
    Serial.printf("[val] boot reset=%d idf=%s heap_integrity=%d\n",
                  static_cast<int>(esp_reset_reason()), esp_get_idf_version(),
                  heap_caps_check_integrity_all(false));
    Serial.println("[val] stack high-water units=bytes; heap capabilities overlap; send ! for event/integrity checkpoint");
    validationEvent("boot");
    lastSampleMs = millis();
}

void validationLoopEnter() {
    const int64_t nowUs = esp_timer_get_time();
    if (lastLoopAtUs != 0) {
        const int64_t gapUs = nowUs - lastLoopAtUs;
        recordGap(loopGaps, static_cast<uint32_t>(gapUs > UINT32_MAX ? UINT32_MAX : gapUs));
    }
    lastLoopAtUs = nowUs;
}

void validationAudioServiced(uint32_t elapsedUs) {
    if (elapsedUs > maxAudioCallUs) maxAudioCallUs = elapsedUs;
}

void validationWebServiced(uint32_t elapsedUs) {
    if (elapsedUs > maxWebCallUs) maxWebCallUs = elapsedUs;
}

void validationEvent(const char* event) {
    events[nextEvent] = {millis(), event};
    nextEvent = (nextEvent + 1) % kEventCount;
    Serial.printf("[val-event] ms=%lu name=%s\n", millis(), event);
}

void validationTick() {
    if (Serial.available() && Serial.read() == '!') {
        Serial.printf("[val] checkpoint heap_integrity=%d\n", heap_caps_check_integrity_all(false));
        dumpEvents();
    }
    const uint32_t now = millis();
    if (now - lastSampleMs < kSamplePeriodMs) return;
    lastSampleMs = now;

    const HeapSample internal = sampleHeap(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const HeapSample dma = sampleHeap(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    const HeapSample psram = sampleHeap(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const uint32_t inputFill = audio.inBufferFilled();
    if (inputFill < minimumInputFill) minimumInputFill = inputFill;
    uint32_t total = 0;
    for (uint32_t count : loopGaps.buckets) total += count;
    uint32_t cumulative = 0;
    size_t p95Bucket = 0;
    for (; p95Bucket < 7; ++p95Bucket) {
        cumulative += loopGaps.buckets[p95Bucket];
        if (cumulative * 20 >= total * 19) break;
    }
    const uint32_t p95UpperUs = p95Bucket < 7 ? kGapBoundsUs[p95Bucket] : UINT32_MAX;
    Serial.printf(
        "[val-sample] ms=%lu wifi=%d source=%d state=%d tasks=%u "
        "int=%u,%u,%u dma=%u,%u,%u psram=%u,%u,%u "
        "stack_loop=%u stack_ctrl=%u stack_audio=%u stack_update=%u "
        "alloc_fail=%lu last_fail=%lu,%lu input=%lu,%lu,%lu "
        "gap_p95_upper_us=%lu gap_max_us=%lu audio_call_max_us=%lu web_call_max_us=%lu\n",
        static_cast<unsigned long>(now), static_cast<int>(WiFi.status()),
        podcastMode ? 2 : (mediaPlaybackState() == PlaybackState::Stopped ? 0 : 1),
        static_cast<int>(mediaPlaybackState()), static_cast<unsigned>(uxTaskGetNumberOfTasks()),
        static_cast<unsigned>(internal.free), static_cast<unsigned>(internal.minimum), static_cast<unsigned>(internal.largest),
        static_cast<unsigned>(dma.free), static_cast<unsigned>(dma.minimum), static_cast<unsigned>(dma.largest),
        static_cast<unsigned>(psram.free), static_cast<unsigned>(psram.minimum), static_cast<unsigned>(psram.largest),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
        static_cast<unsigned>(stackReserve("Ctrl")), static_cast<unsigned>(stackReserve("PeriodicTask")),
        static_cast<unsigned>(stackReserve("firmware-update")),
        static_cast<unsigned long>(failedAllocations.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(lastFailedBytes.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(lastFailedCaps.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(inputFill), static_cast<unsigned long>(minimumInputFill),
        static_cast<unsigned long>(audio.getInBufferSize()),
        static_cast<unsigned long>(p95UpperUs), static_cast<unsigned long>(loopGaps.maxUs),
        static_cast<unsigned long>(maxAudioCallUs), static_cast<unsigned long>(maxWebCallUs));
    loopGaps = {};
    maxAudioCallUs = 0;
    maxWebCallUs = 0;
    minimumInputFill = UINT32_MAX;
}

#endif
