# TFT display and touch wiring

The firmware uses a **240 × 320 ILI9341 SPI TFT with an XPT2046 resistive-touch
controller**, displayed in **320 × 240 landscape** orientation. The connections below
match the pin constants in [include/app_state.h](../../include/app_state.h) and the
LovyanGFX configuration in [src/app_state.cpp](../../src/app_state.cpp). Numbers refer to
ESP32-S3 **GPIO numbers**, not physical header positions. The table records the
shared-bus wiring in the actual TFT header order; hardware verification
of this change remains pending.

The **two joined pairs** are:

| Pins joined on the TFT module | ESP32-S3 connection |
| --- | --- |
| `SCK` + `T_CLK` | GPIO **12** |
| `SDI(MOSI)` + `T_DIN` | GPIO **11** |

Disconnect power before wiring. This device's TFT `VCC` is connected to **5 V DC**;
its signal connections use **3.3 V logic**. Display, touch controller, ESP32-S3, and
audio hardware share a common ground. The pin order matches the
[LCDWIKI MSP2807 interface](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807),
which specifies 3.3–5 V power, 3.3 V logic, and optional display `SDO`. The exact
module model has not been confirmed, so verify the supply rating before using this
wiring with a replacement panel. A 5 V module supply does not mean 5 V GPIO signals.
ESP32-S3 electrical limits are documented in the
[Espressif datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf).

## Complete header wiring, in pin order

Read the labels printed on the TFT module: this numbering starts at `VCC` and
ends at `T_IRQ`. Viewing the header from the opposite side reverses its apparent
left-to-right order; match labels rather than counting from a viewing direction.

| Header position | TFT module pin | ESP32-S3 connection | Function |
| ---: | --- | --- | --- |
| 1 | `VCC` | 5 V DC supply | Module power; owner's working connection |
| 2 | `GND` | `GND` | Common ground |
| 3 | `CS` | GPIO 8 | Display chip select (`TFT_CS`) |
| 4 | `RESET` | GPIO 10 | Display reset (`TFT_RST`) |
| 5 | `DC` | GPIO 9 | Data/command select (`TFT_DC`) |
| 6 | `SDI(MOSI)` | GPIO 11, joined to `T_DIN` | Shared SPI2 data from ESP32-S3 (`TFT_MOSI`) |
| 7 | `SCK` | GPIO 12, joined to `T_CLK` | Shared SPI2 clock (`TFT_SCLK`) |
| 8 | `LED` | GPIO 1 | Backlight PWM (`TFT_BLK`); remapped from GPIO 3 |
| 9 | `SDO(MISO)` | **Not connected** | Display readback; not needed by the current application |
| 10 | `T_CLK` | GPIO 12, joined to `SCK` | Shared SPI2 touch clock (`TOUCH_SCLK`) |
| 11 | `T_CS` | GPIO 14 | Separate touch chip select (`TOUCH_CS`) |
| 12 | `T_DIN` | GPIO 11, joined to `SDI(MOSI)` | Shared SPI2 touch data in (`TOUCH_MOSI`) |
| 13 | `T_DO` | GPIO 5 | Touch data out to ESP32-S3 (`TOUCH_MISO`) |
| 14 | `T_IRQ` | **Not connected** | Touch interrupt; firmware uses polling |

The shared bus uses GPIO 5 for MISO, connected only to touch `T_DO`.
GPIO 13 is the encoder switch. Display `SDO(MISO)` remains disconnected.
Current display rendering does not read pixels or registers back from the physical TFT; pixel
reads in the renderer use its in-memory canvas. Keep `T_DO` connected for touch
to work.

The firmware drives GPIO 1 with active-high, 5 kHz PWM for brightness, automatic
dimming, and backlight shutdown during sleep. Connect it directly only if the module
provides a 3.3 V compatible backlight control input. If `LED` is a backlight power
terminal, use an appropriate current-limited LED driver with an active-high PWM
input driven by GPIO 1; do not assume a GPIO can supply the backlight current.
Backlight pin circuitry varies by breakout; for example, Adafruit documents a PWM
control input on its
[ILI9341 breakout](https://learn.adafruit.com/adafruit-2-4-color-tft-touchscreen-breakout/pinouts).
Tying the backlight permanently to the supply bypasses firmware dimming and shutdown.

The TFT and touch share SPI2 clock/data lines; touch runs at 1 MHz with its own
chip select. With power disconnected, join SCK/T_CLK to GPIO 12 and
SDI/T_DIN to GPIO 11. Keep touch T_DO on GPIO 5:

```text
ESP32-S3 GPIO 12 ──┬── TFT SCK
                  └── Touch T_CLK
ESP32-S3 GPIO 11 ──┬── TFT SDI (MOSI)
                  └── Touch T_DIN
ESP32-S3 GPIO  8 ───── TFT CS
ESP32-S3 GPIO  9 ───── TFT DC
ESP32-S3 GPIO 10 ───── TFT RESET
ESP32-S3 GPIO  1 ───── TFT LED / backlight control
ESP32-S3 GPIO  5 ───── Touch T_DO
ESP32-S3 GPIO 14 ───── Touch T_CS
TFT SDO (MISO)  ────── Not connected
Touch T_IRQ     ────── Not connected
```

Encoder A/B/switch use GPIO 4/7/13. GPIO 3/38 are no longer used by
the radio's peripheral wiring; GPIO 39/40 are now part of the audio I2S interface.
See [the TFT/touch wiring guide](touch-spi-wiring.md).
The shared-bus firmware passed `pio run -e esp32s3`,
`python3 tools/check_touch_diagnostics.py` and `git diff --check` on 2026-10-13.
The shared-bus change has not been flashed or verified on hardware.

Keep `CS` and `T_CS` separate. Pins on a module's optional microSD connector
(`SD_CS`, `SD_SCK`, `SD_MOSI`, `SD_MISO`) are not used by this firmware and should
be left unconnected. `SDI` and `SDO` in the display table are TFT data pins, not
microSD connections. This wiring requires an SPI display and XPT2046 controller;
raw resistive-touch terminals (`X+`, `X-`, `Y+`, `Y-`) are not substitutes for the
`T_*` pins.

## Display and touch checks

For a lit backlight with no picture, use the optional
[TFT color test](../../experiments/tft-color-test/README.md). It temporarily replaces
the radio application with solid color cycles; the guide covers building,
flashing and restoring the normal image, along with recorded test evidence.

## Touch calibration

On the radio, open **Settings → Device → Touch calibration**, then tap and release
each of the four corner markers. Calibration is saved in NVS and loaded
automatically on later boots. Without saved calibration,
the firmware uses its default touch mapping. Repeat calibration after replacing or
rotating the screen.

For current HTTP diagnostics and recovery, see the
[touch diagnosis guide](../ui/touch-diagnosis.md). Opening the USB serial monitor
resets this radio, so use the browser diagnostics when investigating a failure.

## Low-sensitivity test

Verify the new wiring above before sensitivity testing. There is no established sensitivity
benefit from connecting display `SDO` or touch `T_IRQ`: the former supplies display
readback, and the latter is unused by the current polling implementation. The touch
driver uses the instrumented stock LovyanGFX XPT2046 reader at 1 MHz, including its
pressure gate. The custom coordinate-only sampler remains in the source but is
inactive. Separating the SPI buses does not establish a sensitivity improvement.

1. Wake the screen and complete calibration if taps land in the wrong place.
2. At the same button, compare 10 comfortable light finger-pad taps with 10 gentle
   fingernail taps. Lift fully for at least half a second between taps. Count attempts
   and actual button actions separately. Repeat at the center and near the edges.
3. If light finger-pad taps fail while fingernail taps work, inspect any removable
   shipping film and whether the enclosure presses on the panel. With power off,
   check touch jumper contacts and shorten unnecessarily long SPI/ground wires.
   Change one thing at a time and repeat the same trial. Do not peel the bonded
   resistive touch layer or press harder to count a light tap as successful.
4. For input-event and ADC evidence, use the browser diagnostics described in
   [the touch diagnostic capture procedure](../ui/touch-diagnosis.md). Record
   attempted taps and actual actions alongside the coordinates and pressure
   readings; counters alone do not establish comfortable physical sensitivity.

The owner confirmed the previous shared-bus wiring worked. Firmware now restores
the clock/data joins while keeping `T_DO` on GPIO 5. This shared-bus change and
comfortable finger-pad sensitivity still require physical trials; no new
electrical measurement is implied by this documentation update.
