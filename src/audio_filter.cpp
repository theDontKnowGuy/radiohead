#include "audio_notch.h"
#include "audio_tone.h"
#include "media.h"

namespace {
uint32_t appliedTone = UINT32_MAX;
float toneMakeup = 1.0f;
AudioToneLimiter toneLimiter;
}

void mediaInitializeAudioFilters() {
    // Referenced by media startup so this object is extracted from libsrc.a.
    // Otherwise the vendor's weak no-op hooks can satisfy the static linker.
    appliedTone = UINT32_MAX;
    toneMakeup = 1.0f;
    toneLimiter.reset();
}

// Keep this translation unit independent of Audio.h: its weak declaration
// would also make the application hook weak, leaving link selection ambiguous.
// Pinned Audio.h hook: interleaved stereo int32 PCM words before tone/volume.
// The library uses the source sample rate (no output resampling configured).
void audio_process_raw_samples(int32_t* samples, int16_t words) {
    // The pre/post hooks run on the same PCM task. Change coefficients here so
    // EQ and compensation agree for the entire block, including TFT previews.
    const uint32_t requested = radioToneRequested.load(std::memory_order_relaxed);
    if (requested != appliedTone) {
        const int bass = radioToneBand(requested, 0);
        const int mid = radioToneBand(requested, 1);
        const int treble = radioToneBand(requested, 2);
        mediaApplyLocalTone(bass, mid, treble);
        toneMakeup = radioToneMakeup(bass, mid, treble);
        appliedTone = requested;
        if (toneMakeup == 1.0f) toneLimiter.reset();
    }
    static AudioNotch notch;
    notch.configure(radioNotchConfiguration.load(std::memory_order_relaxed), mediaLocalSampleRate());
    if (!notch.active() || !samples || words <= 0) return;
    for (int index = 0; index < words; ++index) {
        const float output = notch.process(samples[index] / 2147483648.0f, index & 1U);
        samples[index] = output >= 1.0f ? INT32_MAX : output <= -1.0f ? INT32_MIN :
            static_cast<int32_t>(output * 2147483648.0f);
    }
}

// Pinned library hook after its EQ and volume. Restoring gain here uses the
// headroom available at the chosen volume, rather than clipping before volume.
void audio_process_i2s(int32_t* samples, int16_t words, bool*) {
    if (toneMakeup == 1.0f || !samples || words <= 0) return;
    toneLimiter.configure(mediaLocalSampleRate());
    for (int index = 0; index + 1 < words; index += 2) {
        float left = samples[index] / 2147483648.0f * toneMakeup;
        float right = samples[index + 1] / 2147483648.0f * toneMakeup;
        toneLimiter.process(left, right);
        samples[index] = radioPcm32(left);
        samples[index + 1] = radioPcm32(right);
    }
}

