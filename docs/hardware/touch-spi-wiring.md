# Shared SPI and encoder remap

Current [display wiring](tft-wiring.md) uses shared SPI2; touch runs at 1 MHz.
Constants: `include/app_state.h`; setup: `src/app_state.cpp`.

| Signal | Before → current GPIO |
| --- | --- |
| Touch clock / MOSI | 4→12 (join TFT SCK) / 7→11 (join TFT SDI) |
| Touch MISO / CS | 5 / 14, unchanged |
| TFT CS / DC / RESET | 8 / 9 / 10, unchanged |
| Encoder A / B / SW | 39→4 / 40→7 / 38→13 |
| Backlight / optional K0 | 3→1 / 6 unchanged |

Power off before moving wires; remove old touch 4/7 connections. TFT SDO and
T_IRQ stay disconnected. GPIO35–37 are PSRAM; audio uses 39–41. Check the
[backlight interface](tft-wiring.md#complete-header-wiring-in-pin-order) before connecting LED.
Both devices enable bus sharing. Touch recovery retains calibration and never
frees the TFT bus/DMA. Encoder GPIO13 and optional K0/GPIO6 wake light sleep.

## Rotation not working?

2026-10-13: owner reported pushbutton working after a PlatformIO flash, rotation
not working despite confirmed 4/7 wiring. Device had a `Ctrl` task, 3,236 B stack
free, build `Oct 13 2026 17:46:32`, image 4,508,768 B. This does not prove A/B edges.

Read `GET /api/device/encoder-debug` before/after slow turns:

- `pinA/pinB/pinSwitch`: expect 4/7/13; `taskCreated/samples`: task exists/polls advance.
- `rawA/rawB/rawSwitch`: 1=HIGH, 0=LOW. `sampledAB`: A bit 1, B bit 0 at task sample.
- `aEdges/bEdges`: both should rise. A missing channel points toward contacts,
  ground/pull-ups or wiring; decoded ticks with no volume change point downstream.
- `positiveTicks/negativeTicks`: quarter-steps; four accumulate per detent.
  `invalidTransitions`: both channels changed between polls; cause remains uncertain.

Reads do not alter input/volume. Locked counters saturate at UINT32_MAX and reset on boot.
Build, touch/encoder host checks and TFT diagnostic build passed for the remap.
Still check direction, mute, 3 s sleep/wake, setup hold, dimming/touch and audio on
hardware with serial closed. Earlier build-only claims do not establish full acceptance.
