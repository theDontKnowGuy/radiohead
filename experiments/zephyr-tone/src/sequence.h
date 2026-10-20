#ifndef TONE_SEQUENCE_H
#define TONE_SEQUENCE_H
#include <stddef.h>
#include <stdint.h>
#define TONE_RATE 44100u
#define TONE_BLOCK_FRAMES 882u /* 20 ms, below ESP32 driver's 4092-byte limit */
#define TONE_SECONDS 5u
#define TONE_GAP_SECONDS 2u
#define TONE_INITIAL_SECONDS 5u
#define TONE_END_SECONDS 45u /* end of final fade; zeros continue indefinitely */
#define TONE_REFERENCE_SECONDS 47u /* include two seconds of final silence */
/* Stateless absolute frame indexing; no phase reset at DMA buffer boundaries. */
void tone_fill(uint32_t first_frame, int16_t *stereo, size_t frames, int left);
#endif
