# Shared TFT and touch SPI wiring

As of 2026-10-13, the firmware uses SPI2 for both the ILI9341 TFT and XPT2046
touch controller, at the user's request to restore the clock and MOSI joins.
Pin constants live in `include/app_state.h`; bus configuration lives in
`src/app_state.cpp`.

| Connection | Previous separate-bus GPIO | Current shared-bus GPIO |
| --- | --- | --- |
| TFT SCK | 12 | 12, joined to T_CLK |
| TFT SDI / MOSI | 11 | 11, joined to T_DIN |
| TFT SDO / MISO | Not connected | Not connected |
| Touch T_CLK | 4 | 12 |
| Touch T_DIN | 7 | 11 |
| Touch T_DO | 5 | 5 |
| Touch T_CS | 14 | 14 |
| Encoder A | 39 | 39 |
| Encoder B | 40 | 40 |
| Encoder switch / SW | 38 | 38 |
| Separate K0 power/wake button | 6 | 6 |

Power down before rewiring. Move T_CLK from GPIO4 to GPIO12 alongside TFT SCK,
and T_DIN from GPIO7 to GPIO11 alongside TFT SDI. Keep touch T_DO on GPIO5;
the TFT bus now also configures GPIO5 as MISO so both devices use the same bus
pin mapping. TFT SDO stays unconnected. GPIO4, GPIO7 and GPIO13 are unused.
Retain power, ground, TFT CS/DC/reset/backlight and touch CS connections.
Display CS (GPIO8) and touch CS (GPIO14) must stay separate. Touch IRQ is unused.

Both LovyanGFX configurations enable bus sharing so display transactions yield
for touch reads. Touch acquisition remains at 1 MHz and saved calibration is
retained. Touch recovery reinitializes touch without freeing the shared display
bus or its DMA resources, preserving TFT calibration/mapping and runtime state.

GPIO35-37 remain reserved for octal PSRAM on the N16R8 board. Encoder GPIO39/40
cannot also be used for external pin-based JTAG. GPIO38 is reserved for the
encoder switch; do not drive the onboard RGB LED that some DevKitC revisions
connect there. The current light-sleep path wakes from the encoder switch or K0.

Verification: `pio run -e esp32s3`, `python3 tools/check_touch_diagnostics.py`
and `git diff --check` passed. The firmware binary is 4,508,768 bytes, with
2,044,832 bytes (31%) free in the smallest app partition. Static DIRAM usage
is 260,491 bytes (76.22%), leaving 81,269 bytes. The host diagnostics check covers
recovery bookkeeping, release consumption and endpoint validation, not physical
shared-bus operation. This change has not been flashed. Hardware acceptance remains pending: check startup,
touch coordinates across the screen, touch recovery, encoder volume/mute,
boot-held Wi-Fi setup and encoder/K0 sleep/wake with audio playing and the USB
serial monitor closed.
