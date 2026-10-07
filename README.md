# Radiohead ESP32 Internet Radio

PlatformIO firmware for an ESP32-S3 internet radio with a TFT display, web controls,
station playlists, podcasts, weather, alarms, audio settings, and OTA updates.

## Application layout

- `src/main.cpp` coordinates startup and the main device loop.
- `src/app_state.cpp` owns shared hardware objects and application state.
- `src/media.cpp` handles stations, M3U playlists, podcasts, and audio metadata.
- `src/settings.cpp` persists user configuration with `Preferences`.
- `src/display.cpp` owns colors, weather rendering, Wi-Fi status, spectrum, and VU drawing.
- `src/device_control.cpp` handles the encoder task, sleep, and factory reset.
- `src/web_server.cpp` owns web pages, request validation, routes, uploads, and OTA handling.
- `include/` contains the public interface for each module.

The bundled `lib/ESP32-audioI2S-master` directory is third-party code and remains
separate from the application modules.

## Stereo speakers

The firmware already sends stereo audio on the I2S bus. Two mono MAX98357A
amplifiers share the same BCLK, LRC/WS, and DIN signals; their `SD_MODE` wiring
selects left or right audio.

![MAX98357A stereo wiring diagram](docs/hardware/max98357a-stereo-wiring.svg)

The diagram shows the additional 1 MΩ pull-up used to select the right channel on
the common 5 V Adafruit-style board. See the
[MAX98357A stereo wiring notes](docs/hardware/max98357a-stereo.md) before connecting
the second amplifier or using a different breakout. The speaker outputs are
bridge-tied and must not be connected together or to ground.

## Build

```sh
pio run -e esp32s3
```

## TFT display and touch wiring

The firmware uses a **240 × 320 ILI9341 SPI TFT with an XPT2046 resistive-touch
controller**, displayed in **320 × 240 landscape** orientation. The connections below
match the pin constants in [include/app_state.h](include/app_state.h) and the
LovyanGFX configuration in [src/app_state.cpp](src/app_state.cpp). Numbers refer to
ESP32-S3 **GPIO numbers**, not physical header positions. The table records the
owner's working wiring in the actual TFT header order.

Disconnect power before wiring. This device's TFT `VCC` is connected to **5 V DC**;
its signal connections use **3.3 V logic**. Display, touch controller, ESP32-S3, and
audio hardware share a common ground. The pin order matches the
[LCDWIKI MSP2807 interface](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807),
which specifies 3.3–5 V power, 3.3 V logic, and optional display `SDO`. The exact
module model has not been confirmed, so verify the supply rating before using this
wiring with a replacement panel. A 5 V module supply does not mean 5 V GPIO signals.
ESP32-S3 electrical limits are documented in the
[Espressif datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf).

### Complete header wiring, in pin order

| Header position | TFT module pin | ESP32-S3 connection | Function |
| ---: | --- | --- | --- |
| 1 | `VCC` | 5 V DC supply | Module power; owner's working connection |
| 2 | `GND` | `GND` | Common ground |
| 3 | `CS` | GPIO 8 | Display chip select (`TFT_CS`) |
| 4 | `RESET` | GPIO 10 | Display reset (`TFT_RST`) |
| 5 | `DC` | GPIO 9 | Data/command select (`TFT_DC`) |
| 6 | `SDI(MOSI)` | GPIO 11 | SPI data from ESP32-S3 (`TFT_MOSI`) |
| 7 | `SCK` | GPIO 12 | SPI clock (`TFT_SCLK`) |
| 8 | `LED` | GPIO 3 | Backlight PWM (`TFT_BLK`); owner's working connection |
| 9 | `SDO(MISO)` | **Not connected** | Display readback; not needed by the current application |
| 10 | `T_CLK` | GPIO 12 | Touch clock, shared with display `SCK` |
| 11 | `T_CS` | GPIO 14 | Separate touch chip select (`TOUCH_CS`) |
| 12 | `T_DIN` | GPIO 11 | Touch data in, shared with display `SDI(MOSI)` |
| 13 | `T_DO` | GPIO 13 | Touch data out to ESP32-S3 (`TFT_MISO`) |
| 14 | `T_IRQ` | **Not connected** | Touch interrupt; firmware uses polling |

Despite the constant name `TFT_MISO`, GPIO 13 receives **touch data from `T_DO`**
in this wiring. Display `SDO(MISO)` remains disconnected. Current display rendering
does not read pixels or registers back from the physical TFT; pixel reads in the
renderer use its in-memory canvas. Keep `T_DO` connected for touch to work.

The firmware drives GPIO 3 with active-high, 5 kHz PWM for brightness, automatic
dimming, and backlight shutdown during sleep. Connect it directly only if the module
provides a 3.3 V compatible backlight control input. If `LED` is a backlight power
terminal, use an appropriate current-limited LED driver with an active-high PWM
input driven by GPIO 3; do not assume a GPIO can supply the backlight current.
Backlight pin circuitry varies by breakout; for example, Adafruit documents a PWM
control input on its
[ILI9341 breakout](https://learn.adafruit.com/adafruit-2-4-color-tft-touchscreen-breakout/pinouts).
Tying the backlight permanently to the supply bypasses firmware dimming and shutdown.

Both controllers share the SPI2 clock and outgoing data lines, with separate chip
selects. The return line is connected only to the touch controller:

```text
ESP32-S3 GPIO 12 ──┬── TFT SCK
                  └── Touch T_CLK
ESP32-S3 GPIO 11 ──┬── TFT SDI (MOSI)
                  └── Touch T_DIN
ESP32-S3 GPIO 13 ───── Touch T_DO
ESP32-S3 GPIO  8 ───── TFT CS
ESP32-S3 GPIO 14 ───── Touch T_CS
TFT SDO (MISO)  ────── Not connected
Touch T_IRQ     ────── Not connected
```

Keep `CS` and `T_CS` separate. Pins on a module's optional microSD connector
(`SD_CS`, `SD_SCK`, `SD_MOSI`, `SD_MISO`) are not used by this firmware and should
be left unconnected. `SDI` and `SDO` in the display table are TFT data pins, not
microSD connections. This wiring requires an SPI display and XPT2046 controller;
raw resistive-touch terminals (`X+`, `X-`, `Y+`, `Y-`) are not substitutes for the
`T_*` pins.

### Touch calibration and diagnostics

On the radio, open **Settings → Device → Touch calibration**, then tap and release
each of the four corner markers. The raw coordinates are printed at 115200 baud and
saved in NVS, then loaded automatically on later boots. Without saved calibration,
the firmware uses its default touch mapping. Repeat calibration after replacing or
rotating the screen.

Normal firmware polls touch but leaves diagnostic output disabled.

### Low-sensitivity test

Keep the working wiring above for the baseline. There is no established sensitivity
benefit from connecting display `SDO` or touch `T_IRQ`: the former supplies display
readback, and the latter is unused by the current polling implementation. The touch
driver already uses 250 kHz sampling, same-axis settling, and coordinate validation
without a pressure threshold.

1. Wake the screen and complete calibration if taps land in the wrong place.
2. At the same button, compare 10 comfortable light finger-pad taps with 10 gentle
   fingernail taps. Lift fully for at least half a second between taps. Count attempts
   and actual button actions separately. Repeat at the center and near the edges.
3. If light finger-pad taps fail while fingernail taps work, inspect any removable
   shipping film and whether the enclosure presses on the panel. With power off,
   check touch jumper contacts and shorten unnecessarily long SPI/ground wires.
   Change one thing at a time and repeat the same trial. Do not peel the bonded
   resistive touch layer or press harder to count a light tap as successful.
4. For ADC and input-event evidence, follow
   [the touch diagnostic capture procedure](docs/ui/touch-diagnosis.md). It compares
   idle, fingernail, light-pad, and firm-pad captures using the existing
   `TOUCH_DEBUG_ENABLED=1` reports (`TouchADC` and `Touch polls=`). These counters
   describe coordinates and events, not measured physical pressure.

The owner has confirmed this wiring works. Comfortable finger-pad sensitivity
remains unresolved; no new physical trial or electrical measurement is implied by
this documentation update.
