#include "sequence.h"
#include "pcm_tables.h"

void tone_fill(uint32_t first_frame, int16_t *stereo, size_t frames, int left)
{
    const unsigned slot = left ? 0u : 1u;
    for (size_t i = 0; i < frames; ++i) {
        uint32_t frame = first_frame + (uint32_t)i;
        int16_t sample = 0;
        if (frame >= TONE_INITIAL_SECONDS * TONE_RATE &&
            frame < TONE_END_SECONDS * TONE_RATE) {
            frame -= TONE_INITIAL_SECONDS * TONE_RATE;
            const unsigned stage = frame / ((TONE_SECONDS + TONE_GAP_SECONDS) * TONE_RATE);
            const unsigned offset = frame % ((TONE_SECONDS + TONE_GAP_SECONDS) * TONE_RATE);
            if (stage < 6u && offset < TONE_SECONDS * TONE_RATE) {
                const struct tone_pcm *p = &tone_pcm[stage];
                if (offset < PCM_FADE_FRAMES) {
                    sample = p->fade_in[offset];
                } else if (offset >= TONE_SECONDS * TONE_RATE - PCM_FADE_FRAMES) {
                    sample = p->fade_out[offset - (TONE_SECONDS * TONE_RATE - PCM_FADE_FRAMES)];
                } else {
                    sample = p->period[offset % PCM_PERIOD_FRAMES];
                }
            }
        }
        stereo[2u * i] = 0;
        stereo[2u * i + 1u] = 0;
        stereo[2u * i + slot] = sample;
    }
}
