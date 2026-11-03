#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Fixed-layout records shared by retained RAM, the archive and host tests.
// No pointers, strings from external sources, credentials or settings.
namespace RadioLog {
constexpr uint32_t Magic = 0x524c0001;
constexpr size_t Samples = 32, Events = 24, Count = Samples + Events;
struct Record {
    uint32_t sequence, ms, utc, events;
    int32_t station, state, wifi, rssi;
    uint32_t input, inputMin, capacity, pcmBlocks, pcmIdleMs;
    uint32_t heapFree, heapMin, heapLargest, psramFree;
    uint32_t allocationFailures, failedBytes;
    uint32_t loopGapUs, audioUs, webUs, slowPhase, phaseUs;
    uint32_t disconnects, disconnectReason, flags;
    uint32_t checksum;
};
struct Header {
    uint32_t magic, boot, resetReason;
    char build[17];
    uint8_t reserved[3];
    uint32_t checksum;
};
struct Store {
    Header header;
    // Last entered service survives a hang without needing a completed sample.
    uint32_t phase, phaseMs, phaseInverse;
    Record records[Count];
};
static_assert(sizeof(Store) < 7500, "Leave RTC memory headroom");

inline uint32_t checksum(const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t value = 2166136261U;
    for (size_t i = 0; i < size; ++i) value = (value ^ bytes[i]) * 16777619U;
    return value ? value : 1;
}
template<class T> inline bool valid(const T& value) {
    return value.checksum != 0 && value.checksum == checksum(&value, offsetof(T, checksum));
}
// Invalidate first, commit checksum last. A reset during a write loses at most
// this slot. Readers validate every slot and never trust a retained index.
template<class T> inline void commit(T& destination, T value) {
    value.checksum = checksum(&value, offsetof(T, checksum));
    destination.checksum = 0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::memcpy(&destination, &value, offsetof(T, checksum));
    std::atomic_thread_fence(std::memory_order_seq_cst);
    destination.checksum = value.checksum;
}
inline bool validHeader(const Header& header) {
    return header.magic == Magic && valid(header);
}
inline bool parseOffset(const char* value, size_t& offset) {
    if (!value || !*value) return false;
    size_t result = 0;
    size_t digits = 0;
    for (; *value; ++value) {
        if (++digits > 2) return false;
        if (*value < '0' || *value > '9') return false;
        result = result * 10 + static_cast<unsigned>(*value - '0');
        if (result >= Count) return false;
    }
    offset = result;
    return true;
}
} // namespace RadioLog
