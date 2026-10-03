# Firmware architecture

Read this reference when changing ownership, startup, the main loop, hardware mappings,
or behavior spanning multiple modules.

## Build target

- PlatformIO environment: `esp32s3`
- Framework: Arduino on ESP32-S3
- Board declaration: `esp32-s3-devkitc1-n16r8`
- Flash configuration: 16 MB QSPI
- PSRAM configuration: 8 MB Octal/OPI
- Main build command: `pio run -e esp32s3`

`platformio.ini` is authoritative when these notes and configuration disagree.

## Module ownership

| Module | Owns |
| --- | --- |
| `src/main.cpp` | Minimal Arduino entry points forwarding to the application coordinator |
| `src/application.cpp` | Startup sequence and cooperative service ordering, including clock and update-status sampling |
| `src/wifi_network.cpp` | Wi-Fi join, setup AP fallback, NTP startup and asynchronous roaming |
| `src/ui_runtime.cpp` | Input-to-controller bridge and execution of queued commands through owning modules |
| `src/ui_controller.cpp` | Semantic UI state, navigation and command production without hardware side effects |
| `src/app_state.cpp` | Shared device objects, constants, data models, catalog data, and cross-module state definitions |
| `src/media.cpp` | I2S setup, saved playback startup, station/M3U/podcast playback, media state and Spotify output handoff |
| `src/settings.cpp` | Preferences load/save and persisted-value normalization |
| `src/display.cpp` | TFT/backlight initialization, native rendering/hit testing, redraw scheduling, weather fetch/status and Wi-Fi indicator |
| `src/device_control.cpp` | Encoder task, input snapshots, calibration, dimming, physical power button, sleep/wake and factory reset |
| `src/web_server.cpp` | HTML pages, request handlers, validation, M3U/firmware uploads, route registration |
| `src/firmware_updater.cpp` | Background release checks and firmware installation |
| `src/boot_screen.cpp` | Boot artwork, audio and bounded hold animation |
| `src/display_fonts.cpp`, `src/ui_text.cpp` | Native typography and text preparation |
| `src/validation_diagnostics.cpp` | Opt-in runtime timing and diagnostic evidence |
| `include/*.h` | Public interfaces and shared types required across translation units |

The refactor review and verification record is in `docs/firmware-architecture.md`.

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
- Podcast playback suppresses live stream-title replacement; text preparation preserves
  readable Hebrew and mixed text in the native renderer.
- Deep sleep saves settings and uses the external K0 wake source. Alarms and timer
  wake are retired by the configuration and controls specification.

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

