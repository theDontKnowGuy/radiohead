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
| Encoder A | 39 | 4 |
| Encoder B | 40 | 7 |
| Encoder switch / SW | 38 | 13 |
| TFT backlight control | 3 | 1 |
| Separate K0 power/wake button | 6 | 6 |

Power down before rewiring. Move T_CLK from GPIO4 to GPIO12 alongside TFT SCK,
and T_DIN from GPIO7 to GPIO11 alongside TFT SDI. Keep touch T_DO on GPIO5;
the TFT bus now also configures GPIO5 as MISO so both devices use the same bus
pin mapping. TFT SDO stays unconnected.
The subsequent encoder/backlight remap on 2026-10-13 moves encoder A/B/SW
from GPIO39/40/38 to GPIO4/7/13 and backlight control from GPIO3 to GPIO1.
Do not leave the old separate-bus touch connections on GPIO4/7: those pins
now belong to the encoder. Retain power, ground, TFT CS/DC/reset and touch CS.
Display CS (GPIO8) and touch CS (GPIO14) must stay separate. Touch IRQ is unused.

Both LovyanGFX configurations enable bus sharing so display transactions yield
for touch reads. Touch acquisition remains at 1 MHz and saved calibration is
retained. Touch recovery reinitializes touch without freeing the shared display
bus or its DMA resources, preserving TFT calibration/mapping and runtime state.

GPIO35-37 remain reserved for octal PSRAM on the N16R8 board. The remap avoids
JTAG GPIO39/40, possible RGB LED GPIO38 and strapping GPIO3. Backlight GPIO1
supports the existing RTC hold. The current light-sleep path still wakes from
the encoder switch (now GPIO13) or K0 (GPIO6).

Shared-bus verification before the encoder/backlight remap:
`pio run -e esp32s3`, `python3 tools/check_touch_diagnostics.py`
and `git diff --check` passed. The firmware binary is 4,508,768 bytes, with
2,044,832 bytes (31%) free in the smallest app partition. Static DIRAM usage
is 260,491 bytes (76.22%), leaving 81,269 bytes. The host diagnostics check covers
recovery bookkeeping, release consumption and endpoint validation, not physical
shared-bus operation. This change has not been flashed. Hardware acceptance remains pending: check startup,
touch coordinates across the screen, touch recovery, encoder volume/mute,
boot-held Wi-Fi setup and encoder/K0 sleep/wake with audio playing and the USB
serial monitor closed.

## Encoder/backlight remap verification, 2026-10-13

`pio run -e esp32s3`, `python3 tools/check_encoder_power.py`,
`python3 tools/check_touch_input.py` and `git diff --check` passed with
A/B/SW on GPIO4/7/13 and backlight PWM on GPIO1. Firmware size remains
4,508,768 bytes with 2,044,832 bytes free in the app slot; static DIRAM
remains 260,491 bytes (76.22%) with 81,269 bytes free.
`python3 tools/build_firmware.py --tft-color-test` also passed with its
backlight remapped to GPIO1; diagnostic size is 422,912 bytes.

No flash or physical rewiring was performed. After rewiring, verify encoder
direction, volume, mute, boot-held Wi-Fi setup, the 3-second sleep hold and
encoder/K0 wake; also verify backlight brightness, automatic dimming and sleep
shutdown, touch operation and audio continuity with the USB serial monitor
closed. GPIO1 remapping does not establish that the TFT LED terminal is an
appropriate GPIO control input; use the backlight interface described in README.

## Rotation failure after the remap

The user reports that GPIO13 pushbutton control works after flashing through
PlatformIO, but rotation does not, with GPIO4/7 wiring confirmed. A read-only
check at the user-supplied IP on 2026-10-13 reports build `Oct 13 2026 17:46:32`,
image size 4,508,768 bytes and an existing `Ctrl` task with 3,236 stack bytes
free. These observations do not verify electrical A/B edges.

The diagnostic image adds `GET /api/device/encoder-debug` (no caching or
Serial output). Read it before and after slowly turning several detents:

- `pinA`, `pinB`, `pinSwitch`: compiled GPIO assignments, expected 4/7/13.
- `taskCreated`, `samples`: creation result and cumulative production polls;
  samples should advance between requests while awake.
- `rawA`, `rawB`, `rawSwitch`: current HIGH=1 / LOW=0 electrical levels.
- `sampledAB`: last task sample, A in bit 1 and B in bit 0. Current raw levels
  can differ because they are read later by the HTTP handler.
- `aEdges`, `bEdges`: cumulative channel changes. Both should increase while
  rotating. No changes, or just one changing channel, directs investigation
  toward wiring, rotary common/GND, pull-ups, contacts or the signal path.
- `positiveTicks`, `negativeTicks`: cumulative decoded quarter steps. Full
  detents still use the existing four-step accumulation; contact bounce can
  contribute to both counts without producing a detent.
- `invalidTransitions`: both A/B bits changed between task samples, so direction
  was not decoded. Repeated increases can indicate missed edges or signal quality
  problems; the counter alone does not establish their cause.

Counters saturate at UINT32_MAX and reset on boot. Reading diagnostics does
not consume encoder ticks, change GPIO modes or alter volume. Task and counter
updates share the existing encoder lock. If samples and both channel edges
advance with decoded ticks but volume stays unchanged, inspect the subsequent
detent/UI/media path rather than changing pin assignments again.
