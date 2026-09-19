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

## Build

```sh
pio run -e esp32s3
```

## XPT2046 touch test wiring

The current touch test assumes the ILI9341 module has an XPT2046 resistive-touch
controller with pins labelled `T_CLK`, `T_CS`, `T_DIN`, and `T_DO`. Connect it to the
ESP32-S3 as follows:

| Touch pin | ESP32-S3 GPIO | Notes |
| --- | ---: | --- |
| `T_CLK` | 12 | Shared with TFT SCLK |
| `T_DIN` | 11 | Shared with TFT MOSI |
| `T_DO` | 13 | Touch MISO |
| `T_CS` | 14 | Touch chip select |
| `T_IRQ` | Not connected | Polling is used for the first test |
| `VCC` | 3.3 V | Do not connect directly to 5 V |
| `GND` | GND | Common ground |

On the first boot, the firmware starts a four-point calibration. Tap and release each
corner marker. The raw coordinates are printed at 115200 baud and saved in NVS, then
loaded automatically on later boots. Hold the encoder switch while booting to run the
calibration again after replacing or rotating the screen.

During normal operation, touching the screen prints raw coordinates, mapped screen
coordinates, and pressure, and draws a cyan/white marker at the mapped location. This
provides a quick test of both the controller connection and the saved calibration.
