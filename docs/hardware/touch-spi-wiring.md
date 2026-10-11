# Separate TFT and touch SPI wiring

The firmware uses SPI2 for the ILI9341 TFT and SPI3 for the XPT2046 touch
controller. Pin constants live in `include/app_state.h`; bus configuration lives
in `src/app_state.cpp`.

| Connection | Previous GPIO | New GPIO |
| --- | --- | --- |
| TFT SCK | 12 | 12 |
| TFT SDI / MOSI | 11 | 11 |
| TFT SDO / MISO | Not connected | Not connected |
| Touch T_CLK | 12 | 4 |
| Touch T_DIN | 11 | 7 |
| Touch T_DO | 13 | 5 |
| Touch T_CS | 14 | 14 |
| Encoder A | 4 | 39 |
| Encoder B | 5 | 40 |
| Encoder switch / SW | 7 | 38 |
| Separate K0 power/wake button | 6 | 6 |

Power down before rewiring. Disconnect the TFT/touch clock and MOSI joins
and move touch T_DO from GPIO13 to GPIO5. TFT SDO stays unconnected; GPIO13
remains configured as TFT MISO but has no wire. If your wiring also joins TFT
SDO to touch T_DO, disconnect that join. Retain the existing power, ground,
TFT CS/DC/reset/backlight and touch CS connections. Touch IRQ remains unused.
The touch SPI clock remains 1 MHz; saved calibration is retained.

GPIO35-37 are unavailable on the configured N16R8 board because they serve
octal PSRAM. GPIO39/40 are usable as encoder inputs, but cannot also be used
for external pin-based JTAG. USB debugging uses different pins. Some DevKitC
revisions connect their RGB LED input to GPIO38; the firmware reserves that
GPIO as an encoder button input and must not drive the LED.

K0 remains on GPIO6 because the existing ext0 deep-sleep wake path requires
an RTC GPIO (GPIO0-21 on ESP32-S3). The encoder switch is a separate input;
moving it to GPIO38 does not make it a deep-sleep wake source.

References: [DevKitC-1 board pin restrictions](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html),
[ESP32-S3 wake sources](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/sleep_modes.html#external-wakeup-ext0).

Verification: `pio run -e esp32s3` and `git diff --check` passed. The image is
4,496,080 bytes with 31% of the smallest app partition free; static DIRAM use
is 260,391 bytes (76.19%). The normal radio image was restored on 2026-10-11;
esptool verified written hashes and reset the board. Functional hardware
verification remains pending.

Hardware acceptance remains pending: check startup, touch coordinates across
the screen, encoder direction/volume, short click, long press, boot-held Wi-Fi
setup and K0 sleep/wake with the new wiring and audio playing.
