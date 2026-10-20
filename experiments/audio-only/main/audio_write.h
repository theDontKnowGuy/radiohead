#pragma once
#include "audio_suite.h"
namespace audio_test {
struct WriteResult { size_t bytes; int error; bool retryable; };
// Callback owns driver status accounting. Consume its accepted prefix even on timeout.
// An incomplete stereo frame is a fatal contract violation, never silently rounded.
template<typename Writer, typename Accepted>
bool writeAll(const uint8_t* data, size_t bytes, size_t frameBytes, Writer write, Accepted accepted) {
    size_t offset=0;
    unsigned stalls=0;
    while (offset<bytes) {
        const WriteResult r=write(data+offset,bytes-offset);
        if (!validProgress(bytes-offset,r.bytes,frameBytes)) return false;
        accepted(r.bytes,r.bytes<bytes-offset);
        offset+=r.bytes;
        if (r.error && !r.retryable) return false;
        if (!r.bytes) { if (++stalls>=3) return false; }
        else stalls=0;
    }
    return true;
}
} // namespace audio_test
