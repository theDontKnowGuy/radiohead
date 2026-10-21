#include "audio_suite.h"
#include "audio_controls.h"
#include "audio_write.h"
#if AUDIO_ONLY_RECORDING_SEQUENCE
#include "audio_sequence.h"
#endif
#include "app_audio_pins.h"
#include <cinttypes>
#include <cstdio>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/i2s_std.h"
#if AUDIO_ONLY_UART_EVIDENCE
#include "driver/uart.h"
#elif AUDIO_ONLY_EVIDENCE
#include "driver/usb_serial_jtag.h"
#endif
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const int16_t musicStart[] asm("_binary_music_pcm_start");
extern const int16_t musicEnd[] asm("_binary_music_pcm_end");
namespace {
using namespace audio_test;
constexpr int kDmaBlocks = 8;
struct Counters {
    uint64_t generated=0, accepted=0, partial=0, errors=0, deadlines=0, gaps=0, clips=0;
    int peak=0, id=1, trimDb=0, error=0;
    bool stopped=true, muted=false, fault=false, holding=false;
    uint32_t epoch=0;
};
portMUX_TYPE sharedMux=portMUX_INITIALIZER_UNLOCKED;
Request requested;
Counters published;
// Only the output task owns the I2S handle, synth, buffers and working counters.
Counters counts;
i2s_chan_handle_t tx=nullptr;
bool enabled=false;
alignas(4) int16_t samples16[kBlock*2];
alignas(4) int32_t samples32[kBlock*2];
StaticTask_t audioTaskState;
StackType_t audioStack[6144/sizeof(StackType_t)];
int64_t lastWrite=0;

Request snapshot() {
    portENTER_CRITICAL(&sharedMux); const Request r=requested; portEXIT_CRITICAL(&sharedMux); return r;
}
void publish() {
    portENTER_CRITICAL(&sharedMux); published=counts; portEXIT_CRITICAL(&sharedMux);
}
void fail(esp_err_t error) { ++counts.errors; counts.error=error; counts.fault=true; }
void stop() {
    if (tx && enabled) {
        const esp_err_t result=i2s_channel_disable(tx); // stops BCLK and WS together
        if (result!=ESP_OK) fail(result);
        else enabled=false;
    }
    if (tx && !enabled) {
        const esp_err_t removed=i2s_del_channel(tx);
        if (removed!=ESP_OK) fail(removed);
        else tx=nullptr; // never lose an owned handle on a driver cleanup error
    }
    counts.stopped=!enabled;
    if (counts.stopped) lastWrite=0;
}
bool start(const Case& c) {
    configASSERT(!tx && !enabled);
    i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    channel.dma_desc_num=kDmaBlocks; channel.dma_frame_num=kBlock;
    channel.auto_clear_after_cb=true; // driver emits zeros on unfilled DMA, not old audio
    esp_err_t result=i2s_new_channel(&channel,&tx,nullptr);
    if (result!=ESP_OK) { tx=nullptr; fail(result); return false; }
    const auto width=c.bits==16 ? I2S_DATA_BIT_WIDTH_16BIT : I2S_DATA_BIT_WIDTH_32BIT;
    i2s_std_config_t config{};
    config.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(c.rate));
    config.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(width,I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk=I2S_GPIO_UNUSED;
    config.gpio_cfg.bclk=static_cast<gpio_num_t>(I2S_BCK);
    config.gpio_cfg.ws=static_cast<gpio_num_t>(I2S_LRC);
    config.gpio_cfg.dout=static_cast<gpio_num_t>(I2S_DIN);
    config.gpio_cfg.din=I2S_GPIO_UNUSED;
    result=i2s_channel_init_std_mode(tx,&config);
    if (result!=ESP_OK) { fail(result); stop(); return false; }
    // Fill every descriptor with exact zeros while READY, before enabling the clocks.
    for (int i=0;i<kDmaBlocks;++i) {
        size_t loaded=0;
        const size_t bytes=kBlock*2*(c.bits/8);
        result=i2s_channel_preload_data(tx,samples32,bytes,&loaded);
        if (result!=ESP_OK || loaded!=bytes) {
            fail(result==ESP_OK ? ESP_FAIL : result); stop();return false;
        }
        counts.generated+=kBlock; counts.accepted+=kBlock;
    }
    result=i2s_channel_enable(tx);
    if (result!=ESP_OK) { fail(result); stop();return false; }
    enabled=true; counts.stopped=false; lastWrite=0; return true;
}
bool writeBlock(const Case& c) {
    const size_t frameBytes=2*(c.bits/8), bytes=kBlock*frameBytes;
    const uint8_t* data=reinterpret_cast<const uint8_t*>(c.bits==16 ? static_cast<void*>(samples16) : static_cast<void*>(samples32));
    esp_err_t latestError=ESP_OK;
    const bool ok=writeAll(data,bytes,frameBytes,
        [&latestError,frameBytes](const uint8_t* ptr,size_t remaining) {
            size_t written=0;
            const esp_err_t result=i2s_channel_write(tx,ptr,remaining,&written,30);
            if (result!=ESP_OK) { ++counts.errors;counts.error=result;latestError=result; }
            if (!validProgress(remaining,written,frameBytes)) { latestError=ESP_ERR_INVALID_SIZE;fail(latestError); }
            return WriteResult{written,result,result==ESP_ERR_TIMEOUT};
        }, [frameBytes](size_t written,bool partial) {
            counts.accepted+=written/frameBytes;
            if (partial) ++counts.partial;
        });
    if (!ok) {
        if (latestError==ESP_OK) fail(ESP_ERR_TIMEOUT);
        else { counts.error=latestError;counts.fault=true; }
        return false;
    }
    const int64_t now=esp_timer_get_time();
    if (lastWrite && now-lastWrite > kDmaBlocks*kBlock*1000000LL/c.rate) ++counts.gaps;
    lastWrite=now;
    return true;
}
void pack(int16_t l, int16_t r, size_t i) {
    samples16[i*2]=l; samples16[i*2+1]=r;
    samples32[i*2]=pack32(l); samples32[i*2+1]=pack32(r);
    counts.peak=std::max({counts.peak,std::abs(static_cast<int>(l)),std::abs(static_cast<int>(r))});
}
bool zeros(const Case& c, int blocks) {
    for (auto& x:samples16) x=0;
    for (auto& x:samples32) x=0;
    for (int i=0;i<blocks;++i) {
        counts.generated+=kBlock;
        if (!writeBlock(c)) return false;
    }
    return true;
}
void outputTask(void*) {
#if AUDIO_ONLY_RECORDING_SEQUENCE
    const Case format=kCases[0]; // unchanged Philips stereo 44.1 kHz / 16-bit
    RecordingSequence sequence{musicStart,
        (reinterpret_cast<uintptr_t>(musicEnd)-reinterpret_cast<uintptr_t>(musicStart))/4};
    if (start(format)) {
        // start() preloads 8 * 128 zero frames. They are part of the opening
        // five seconds, rather than an unexported delay before the reference.
        sequence.frame=kDmaBlocks*kBlock;
        for (;;) {
            for (size_t i=0;i<kBlock;++i) {
                int16_t left,right;
                sequence.next(left,right);
                pack(left,right,i);
            }
            counts.generated+=kBlock;
            if (!writeBlock(format)) break;
        }
    }
    stop(); // any driver failure latches silent; never replay a partial sequence
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
#else
    Synth synth;
    Case currentCase=kCases[0];
    Ramp envelope;
    Request active{}; active.epoch=0;
    uint32_t cue=0;
    bool changing=false, cueDone=false;
    for (;;) {
        const Request wanted=snapshot();
        if (wanted.epoch!=active.epoch) {
            if (tx && !counts.fault) {
                if (!changing) { envelope.set(0,currentCase.rate*30/1000); changing=true; }
                if (!envelope.left) { zeros(currentCase,kDmaBlocks+2); stop(); }
            }
            if (!tx || counts.fault) {
                if (tx) stop();
                if (tx) { publish();vTaskDelay(pdMS_TO_TICKS(5));continue; }
                active=wanted; counts.id=active.id; counts.epoch=active.epoch; counts.fault=false; counts.holding=false;
                currentCase=selectedCase(active);
                const Case& c=currentCase;
                // Stopped case still has a brief ID cue, then no clocks indefinitely.
                synth.configure(c,musicStart,(reinterpret_cast<uintptr_t>(musicEnd)-reinterpret_cast<uintptr_t>(musicStart))/4);
                for (auto& x:samples32) x=0;
                changing=false; cue=0; cueDone=false; envelope=Ramp{};
                if (!start(c)) { publish(); vTaskDelay(pdMS_TO_TICKS(5)); continue; }
            }
        }
        const Case& c=currentCase;
        const int cueId=active.cueId ? active.cueId : active.id;
        counts.trimDb=wanted.trimDb; counts.muted=wanted.muted;
        if (counts.fault || !tx) { publish(); vTaskDelay(pdMS_TO_TICKS(2)); continue; }
        if (wanted.muted) { cue=cueFrames(cueId,c.rate); cueDone=true; }
        const float targetLevel=wanted.muted ? 0 : dbGain(wanted.trimDb);
        if (!changing) envelope.set(targetLevel, c.rate*(wanted.muted ? 5 : 30)/1000);
        const int64_t begin=esp_timer_get_time();
        synth.beginBlock();
        for (size_t i=0;i<kBlock;++i) {
            int16_t l=0,r=0;
            if (!cueDone) {
                l=r=cueSample(cue,cueId,c.rate);
                // Honor mute during the ID too. Format changes occur only at zero.
                if (changing) l=r=static_cast<int16_t>(l*envelope.tick());
                if (++cue>=cueFrames(cueId,c.rate)) {
                    cueDone=true;envelope=Ramp{};
                    // Start gain ramp on the first PCM frame, independent of cue length.
                    if (!changing) envelope.set(targetLevel,c.rate*30/1000);
                }
            } else synth.next(l,r,envelope.tick());
            pack(l,r,i);
        }
        counts.clips+=synth.clips; synth.clips=0;
        counts.generated+=kBlock;
        if (esp_timer_get_time()-begin > kBlock*1000000LL/c.rate) ++counts.deadlines;
        if (!writeBlock(c)) { stop(); publish(); continue; }
        if (cueDone && c.wave==Wave::Stopped && !changing) { zeros(c,kDmaBlocks+2); stop(); }
        counts.holding=cueDone && !changing && !counts.fault;
        publish();
    }
#endif
}
void outputPin(gpio_num_t pin, int level) {
    gpio_set_level(pin,level); gpio_set_direction(pin,GPIO_MODE_OUTPUT);
}
#if AUDIO_ONLY_EVIDENCE
bool evidenceHealthy=true;
void evidence() {
    static int64_t last=0;
    if (esp_timer_get_time()-last<1000000) return;
    last=esp_timer_get_time();
    portENTER_CRITICAL(&sharedMux);
    const Counters c=published;
    const Request r=requested;
    portEXIT_CRITICAL(&sharedMux);
    // Only this control task changes requests. A matching stopped epoch means
    // the output task cannot start a pending new case while status is sent.
    if (!c.stopped || r.epoch!=c.epoch || !evidenceHealthy) return;
    static char line[512];
    const int n=snprintf(line,sizeof(line),"audio-only id=%d trim=%d mute=%d stopped=%d fault=%d generated=%" PRIu64
        " accepted=%" PRIu64 " partial=%" PRIu64 " errors=%" PRIu64 " last_error=%d generation_deadlines=%" PRIu64
        " inferred_gaps=%" PRIu64 " peak=%d clipped=%" PRIu64 " driver_underruns=unavailable\r\n",
        c.id,c.trimDb,c.muted,c.stopped,c.fault,c.generated,c.accepted,c.partial,c.errors,c.error,c.deadlines,c.gaps,c.peak,c.clips);
    const size_t length=std::min(n,static_cast<int>(sizeof(line)-1));
#if AUDIO_ONLY_UART_EVIDENCE
    // No TX ring queue or host-dependent blocking. Bounded FIFO chunks are
    // drained before this task can request another case; clocks remain stopped.
    const int64_t deadline=esp_timer_get_time()+200000;
    evidenceHealthy=writeAll(reinterpret_cast<const uint8_t*>(line),length,1,
        [deadline](const uint8_t* data,size_t remaining) {
            if (esp_timer_get_time()>=deadline) return WriteResult{0,ESP_ERR_TIMEOUT,false};
            const int sent=uart_tx_chars(UART_NUM_0,reinterpret_cast<const char*>(data),std::min(remaining,size_t{64}));
            if (sent<0) return WriteResult{0,ESP_FAIL,false};
            const esp_err_t drained=uart_wait_tx_done(UART_NUM_0,pdMS_TO_TICKS(20));
            return WriteResult{static_cast<size_t>(sent),drained,false};
        }, [](size_t,bool) {});
    // A UART failure latches the control task in this stopped case. It must
    // never advance to audio while a failed status transfer could still drain.
#else
    // Zero timeout; missing native-USB readers may drop this bounded snapshot.
    usb_serial_jtag_write_bytes(line,length,0);
#endif
}
#endif
} // namespace

extern "C" void app_main() {
    gpio_deep_sleep_hold_dis();
    rtc_gpio_hold_dis(GPIO_NUM_1); gpio_hold_dis(GPIO_NUM_1); rtc_gpio_deinit(GPIO_NUM_1);
    outputPin(GPIO_NUM_1,0); outputPin(GPIO_NUM_8,1); outputPin(GPIO_NUM_14,1);
    // No SPI, TFT reset, touch, LEDC, NVS, filesystem or radio initialization.
#if !AUDIO_ONLY_RECORDING_SEQUENCE && !AUDIO_ONLY_HANDSFREE && !AUDIO_ONLY_MUSIC_FORMATS && !AUDIO_ONLY_MUSIC_CHANNELS && AUDIO_ONLY_FIXED_CASE == 0
    gpio_config_t input{};
    input.pin_bit_mask=(1ULL<<4)|(1ULL<<7)|(1ULL<<13);
    input.mode=GPIO_MODE_INPUT; input.pull_up_en=GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&input));
#endif
#if AUDIO_ONLY_MUSIC_CHANNELS
    requested=MusicChannels{AUDIO_ONLY_RADIO_INDEX}.state;
#elif AUDIO_ONLY_MUSIC_FORMATS
    requested=MusicFormats{AUDIO_ONLY_RADIO_INDEX}.state;
#elif AUDIO_ONLY_FIXED_CASE > 0
    requested.id=AUDIO_ONLY_FIXED_CASE; requested.trimDb=AUDIO_ONLY_TRIM;
#endif
#if AUDIO_ONLY_UART_EVIDENCE
    // Explicit diagnostic-only UART0 mapping for the board's USB-UART bridge.
    // No hardware rewiring; standard ESP32-S3 UART0 TX=43, RX=44, no RTS/CTS.
    uart_config_t uart{};
    uart.baud_rate=115200;uart.data_bits=UART_DATA_8_BITS;
    uart.parity=UART_PARITY_DISABLE;uart.stop_bits=UART_STOP_BITS_1;
    uart.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;uart.source_clk=UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,256,0,0,nullptr,0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0,&uart));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0,43,44,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_disable_rx_intr(UART_NUM_0)); // no command parser or RX work
#elif AUDIO_ONLY_EVIDENCE
    usb_serial_jtag_driver_config_t usb{512,128};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
#endif
#if !AUDIO_ONLY_RECORDING_SEQUENCE
    vTaskDelay(pdMS_TO_TICKS(2000)); // quiet boot before the first low-level ID
#endif
    const TaskHandle_t task=xTaskCreateStaticPinnedToCore(outputTask,"audio-only",sizeof(audioStack),nullptr,
        20,audioStack,&audioTaskState,1);
    configASSERT(task);
    if (!task) return;
#if AUDIO_ONLY_RECORDING_SEQUENCE
    return; // output task owns the entire run; no polling control task is needed
#else
#if AUDIO_ONLY_MUSIC_CHANNELS
    MusicChannels automatic{AUDIO_ONLY_RADIO_INDEX};
#elif AUDIO_ONLY_MUSIC_FORMATS
    MusicFormats automatic{AUDIO_ONLY_RADIO_INDEX};
#elif AUDIO_ONLY_HANDSFREE
    HandsFree automatic;
#elif !AUDIO_ONLY_RECORDING_SEQUENCE && AUDIO_ONLY_FIXED_CASE == 0
    Controls controls;
    controls.ab=(gpio_get_level(GPIO_NUM_4)<<1)|gpio_get_level(GPIO_NUM_7);
#endif
    for (;;) {
#if AUDIO_ONLY_UART_EVIDENCE
        if (!evidenceHealthy) { vTaskDelay(pdMS_TO_TICKS(20));continue; }
#endif
#if AUDIO_ONLY_HANDSFREE || AUDIO_ONLY_MUSIC_FORMATS || AUDIO_ONLY_MUSIC_CHANNELS
        portENTER_CRITICAL(&sharedMux); const Counters current=published; portEXIT_CRITICAL(&sharedMux);
        const Request control=automatic.poll(esp_timer_get_time(),current.id,current.epoch,current.holding,current.fault);
        portENTER_CRITICAL(&sharedMux); requested=control; portEXIT_CRITICAL(&sharedMux);
#elif !AUDIO_ONLY_RECORDING_SEQUENCE && AUDIO_ONLY_FIXED_CASE == 0
        const uint8_t ab=(gpio_get_level(GPIO_NUM_4)<<1)|gpio_get_level(GPIO_NUM_7);
        const Request control=controls.poll(esp_timer_get_time(),!gpio_get_level(GPIO_NUM_13),ab);
        portENTER_CRITICAL(&sharedMux); requested=control; portEXIT_CRITICAL(&sharedMux);
#endif
#if AUDIO_ONLY_EVIDENCE
        evidence();
#endif
        vTaskDelay(pdMS_TO_TICKS(1));
    }
#endif
}
