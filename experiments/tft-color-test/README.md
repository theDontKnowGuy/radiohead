# TFT color test

Opt-in ESP32-S3 N16R8 / ILI9341 test: the entire screen cycles red, green,
blue, white and black, one color per second. The backlight stays on. Black is
an intentional one-second stage. Touch, encoder, Wi-Fi, audio, sprites and saved
settings are unused. The test does not read or write Preferences or LittleFS.

| TFT connection | GPIO |
| --- | ---: |
| SCK | 12 |
| SDI / MOSI | 11 |
| CS | 8 |
| DC | 9 |
| RESET | 10 |
| LED / backlight control | 1 |
| SDO / MISO | Unconnected |

Keep the existing TFT supply and common ground. Touch CS on GPIO14 is held
high; the test never initializes SPI3. SPI2 writes at 8 MHz with DMA disabled,
using the same pinned LovyanGFX 1.2.21 library as the radio.

From the repository root:

```sh
python3 tools/build_firmware.py --tft-color-test
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/tft-color-test --port /dev/cu.usbmodem11201
```

The build flag sets CMake `RADIOHEAD_TFT_COLOR_TEST=ON`; only this diagnostic
source supplies Arduino `setup()` and `loop()`. The full radio application
sources are excluded. Without the flag, the build explicitly sets OFF and
builds the normal radio. Default test build/output directories are
`.pio/idf-tft-color-test` and `.pio/build/tft-color-test`, so radio artifacts
stay available for recovery. Both images use the pinned ESP-IDF/Arduino build
and the same partition table. The standalone PlatformIO project in this
directory remains an alternative and explicitly defines the same test macro.

Use the connected radio's port if it differs. Upload replaces the running radio
application with this test; the partition layout matches the radio. To restore
the radio's existing build:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py --port /dev/cu.usbmodem11201
```

If the colors appear, basic TFT communication works with this simpler setup;
the result does not yet verify full-firmware SPI3 initialization, touch, DMA or
normal display speed. If it stays blank, check TFT SCK/SDI/CS/DC/RESET, power and
ground with power off, and check that the old TFT/touch joins were removed.
A lit backlight alone does not establish TFT communication. Confirm the test
upload succeeded and the board restarted before interpreting either result.

Code review of the radio remap found unchanged TFT GPIOs and SPI2 host, with
touch moved to separate GPIOs on SPI3. The TFT's `bus_shared` flag also changed
to false. No definite cause of the blank screen was established from source
inspection; hardware verification remains pending.

Verification: the native flag-enabled test, normal `pio run -e esp32s3`,
standalone test build and `git diff --check` passed. CMake caches show the flag
ON for the diagnostic and OFF for the radio. Compile commands confirm the
diagnostic uses only `experiments/tft-color-test/src/main.cpp` as application
source, while the normal build retains all radio application sources. The
native test partition binary matches the radio's byte-for-byte.

The native diagnostic image is 422,912 bytes with 94% app-slot space free and
27.13% static DIRAM use. The normal image remains 4,496,080 bytes with 31%
app-slot space free and 76.19% static DIRAM use. The original standalone color
test was uploaded to `/dev/cu.usbmodem11201` on 2026-10-11 with verified hashes.
The flag-enabled native diagnostic has been build-verified but not uploaded;
device behavior still requires visual/audio observation.

After retaining the test behind the flag, the normal radio image was restored
on the same port on 2026-10-11. esptool verified all written hashes and reset
the board; normal playback and display behavior still need user observation.
