/* Host-only exporter of the exact buffer-filling code compiled into firmware. */
#include <stdio.h>
#include <stdlib.h>
#include "sequence.h"
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    const unsigned chunk = (unsigned)strtoul(argv[1], NULL, 10);
    const int left = atoi(argv[2]);
    if (!chunk || chunk > 2048 || (left != 0 && left != 1)) return 2;
    int16_t buffer[2048 * 2];
    for (uint32_t n = 0; n < TONE_REFERENCE_SECONDS * TONE_RATE;) {
        unsigned count = chunk;
        if (n + count > TONE_REFERENCE_SECONDS * TONE_RATE)
            count = TONE_REFERENCE_SECONDS * TONE_RATE - n;
        tone_fill(n, buffer, count, left);
        for (unsigned i = 0; i < count * 2; ++i) {
            const uint16_t value = (uint16_t)buffer[i];
            if (putchar(value & 255) == EOF || putchar(value >> 8) == EOF) return 1;
        }
        n += count;
    }
    /* Also exercise the post-sequence saturation point used by main.c. */
    tone_fill(TONE_REFERENCE_SECONDS * TONE_RATE, buffer, 2048, left);
    for (unsigned i = 0; i < 4096; ++i) if (buffer[i]) return 3;
    return ferror(stdout) ? 1 : 0;
}
