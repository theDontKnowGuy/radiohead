# Firmware architecture

Read this reference when changing ownership, startup, the main loop, hardware mappings,
or behavior spanning multiple modules.

## Build target

- PlatformIO environment: `esp32s3`
- Framework: Arduino on ESP32-S3
- Board declaration: `esp32-s3-devkitc-1`
- Flash configuration: 16 MB QSPI
- PSRAM configuration: 8 MB Octal/OPI
- Main build command: `pio run -e esp32s3`

`platformio.ini` is authoritative when these notes and configuration disagree.

## Module ownership

| Module | Owns |
| --- | --- |
| `src/main.cpp` | Boot sequence, Wi-Fi startup, alarm/encoder/power coordination, top-level loop and display refresh orchestration |
| `src/app_state.cpp` | Shared device objects, constants, data models, catalog data, and cross-module state definitions |
| `src/media.cpp` | Station playback, M3U resolution/import, podcast fetch/playback, stream-title callback |
| `src/settings.cpp` | Preferences load/save and persisted-value normalization |
| `src/display.cpp` | Colors, brightness, weather fetch/rendering, Wi-Fi indicator, spectrum and VU rendering |
| `src/device_control.cpp` | Encoder sampling task, deep sleep, wake configuration, factory reset |
| `src/web_server.cpp` | HTML pages, request handlers, validation, M3U/firmware uploads, route registration |
| `include/*.h` | Public interfaces and shared types required across translation units |

Do not create a catch-all utilities module. Put a helper beside the behavior it supports
unless it has multiple concrete consumers and a stable abstraction.

## Hardware constants

The current mapping is declared in `include/app_state.h`:

- I2S: BCK 15, DIN 16, LRC 17
- Encoder: A 4, B 5, K0 6, switch 7
- TFT backlight: 1
- TFT SPI/panel wiring is configured by `LGFX_Config` in `src/app_state.cpp`

Treat pin changes as product changes. Check boot strapping, wake capability, peripheral
conflicts, and the physical board before modifying them.

## Runtime invariants

- `audio.loop()` must run frequently enough to keep streams fed.
- `server.handleClient()` must remain responsive without starving audio.
- The encoder FreeRTOS task updates `encoderPos`; the Arduino loop consumes it.
- Station indexes must remain within `[0, STATION_COUNT)`.
- Podcast show and episode indexes must be checked before access.
- `mainVal` indexes `volCurve` and must remain within `[0, 21]`.
- AP/setup mode must not attempt normal station playback.
- Podcast playback suppresses live stream-title replacement and displays a TFT-safe show
  name because the selected TFT fonts do not handle Hebrew reliably.
- Deep sleep saves settings and may arm a timer wake for the alarm in addition to the
  external K0 wake source.

## External boundaries

- Preferences data may survive many firmware versions; retain existing keys or add an
  explicit migration.
- Web arguments and uploaded data are untrusted.
- OpenWeather and Omny responses are remote, mutable inputs. Check status, shape, bounds,
  timeouts, and memory usage.
- OTA writes are destructive device operations. Keep success/error handling explicit and
  do not report success before `Update` completes.

## Third-party code

`lib/ESP32-audioI2S-master/` is vendored. Prefer the declared PlatformIO dependency and
application-level adaptation. Patch vendored sources only for a requested, documented
reason and verify that PlatformIO actually builds the intended copy.

