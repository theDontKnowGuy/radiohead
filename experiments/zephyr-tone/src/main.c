#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include "sequence.h"

#define BLOCK_BYTES (TONE_BLOCK_FRAMES * 2u * sizeof(int16_t))
#define PREFILL 6u
K_MEM_SLAB_DEFINE_STATIC(tx_slab, BLOCK_BYTES, 10, 4);
BUILD_ASSERT(BLOCK_BYTES <= 4092);
BUILD_ASSERT(sizeof(int16_t) == 2);

/* RAM only, inspectable with a debugger. No playback-time output or retries.
 * A failed run must not restart tones or silently conceal a DMA/queue error. */
volatile int tone_fault;
volatile uint32_t tone_accepted_blocks;

int main(void)
{
    const struct device *i2s = DEVICE_DT_GET(DT_NODELABEL(i2s0));
    struct i2s_config cfg = {
        .word_size = 16,
        .channels = 2,
        .format = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_DATA_ORDER_MSB,
        .options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
        .frame_clk_freq = TONE_RATE,
        .mem_slab = &tx_slab,
        .block_size = BLOCK_BYTES,
        .timeout = 500,
    };
    int error = -ENODEV;
    uint32_t frame = 0;
    bool running = false;
    bool configured = false;

    if (!device_is_ready(i2s)) {
        goto failed;
    }
    error = i2s_configure(i2s, I2S_DIR_TX, &cfg);
    if (error) {
        goto failed;
    }
    configured = true;
    for (;;) {
        void *block;
        error = k_mem_slab_alloc(&tx_slab, &block, K_MSEC(500));
        if (error) {
            goto failed;
        }
        tone_fill(frame, block, TONE_BLOCK_FRAMES, IS_ENABLED(CONFIG_TONE_LEFT));
        error = i2s_write(i2s, block, BLOCK_BYTES);
        if (error) {
            k_mem_slab_free(&tx_slab, block); /* ownership only transfers on success */
            goto failed;
        }
        ++tone_accepted_blocks;
        /* Saturate after finite sequence; keep buffered zero PCM and clocks forever. */
        if (frame < TONE_REFERENCE_SECONDS * TONE_RATE) {
            frame += TONE_BLOCK_FRAMES;
        }
        if (!running && tone_accepted_blocks == PREFILL) {
            error = i2s_trigger(i2s, I2S_DIR_TX, I2S_TRIGGER_START);
            if (error) {
                goto failed;
            }
            running = true;
        }
    }
failed:
    tone_fault = error;
    if (configured) {
        (void)i2s_trigger(i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
    }
    k_sleep(K_FOREVER);
    return 0;
}
