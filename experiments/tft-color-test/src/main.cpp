#if !defined(RADIOHEAD_TFT_COLOR_TEST) || RADIOHEAD_TFT_COLOR_TEST != 1
#error "Enable RADIOHEAD_TFT_COLOR_TEST explicitly to build the screen diagnostic"
#endif

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <driver/rtc_io.h>

namespace {
constexpr int kBacklight = 1;
constexpr int kTouchCs = 14;

class TestDisplay final : public lgfx::LGFX_Device {
    lgfx::Bus_SPI bus_;
    lgfx::Panel_ILI9341 panel_;

public:
    TestDisplay() {
        auto bus = bus_.config();
        bus.spi_host = SPI2_HOST;
        bus.spi_mode = 0;
        bus.freq_write = 8000000;
        bus.pin_sclk = 12;
        bus.pin_mosi = 11;
        bus.pin_miso = -1;  // TFT SDO is unconnected.
        bus.pin_dc = 9;
        bus.dma_channel = 0;
        bus_.config(bus);
        panel_.setBus(&bus_);

        auto panel = panel_.config();
        panel.pin_cs = 8;
        panel.pin_rst = 10;
        panel.panel_width = 240;
        panel.panel_height = 320;
        panel.bus_shared = false;
        panel_.config(panel);
        setPanel(&panel_);  // No touch controller or second SPI bus.
    }
};

TestDisplay display;
constexpr uint16_t kColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
}  // namespace

void setup() {
    // Deselect touch even if it remains physically connected.
    pinMode(kTouchCs, OUTPUT);
    digitalWrite(kTouchCs, HIGH);
    // Release the backlight hold if the radio previously entered deep sleep.
    rtc_gpio_hold_dis(static_cast<gpio_num_t>(kBacklight));
    rtc_gpio_deinit(static_cast<gpio_num_t>(kBacklight));
    pinMode(kBacklight, OUTPUT);
    digitalWrite(kBacklight, HIGH);
    display.init();
    display.setRotation(1);
}

void loop() {
    static size_t nextColor = 0;
    display.fillScreen(kColors[nextColor]);
    nextColor = (nextColor + 1) % (sizeof(kColors) / sizeof(kColors[0]));
    delay(1000);
}
