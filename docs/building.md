# Build and flash firmware

Run commands from the repository root. Replace `PORT` with the connected radio's
serial port.

```sh
python3 tools/build_firmware.py
# Equivalent PlatformIO convenience target:
pio run -e esp32s3
```

The complete firmware is compiled once through ESP-IDF 5.5.5 / CMake, with
Arduino-ESP32 3.3.11 as a component and Spotify included. PlatformIO installs
the pinned toolchain and libraries; it does not compile a second application.
The entry command also prepares the pinned cspot/Bell candidate and generates
UI assets. Asset generation currently requires macOS `sips`, `xxd`, and Swift.
Python 3 with Pillow, PlatformIO and Git must be installed. Install Pillow into
the Python environment used to run the build (`python3 -m pip install Pillow`);
asset preparation uses it to decode small alpha icons into flash data so player
redraws need no DMA allocation. macOS AppKit/Swift rasterizes the source SVGs.
The first build needs network
access for dependencies. Framework updates remain a separate change.

The OTA application stays at `.pio/build/esp32s3/firmware.bin`. Matching factory,
ELF, bootloader, partition and initial OTA images plus `artifacts.json` are
exported from the same native build. Flash an existing build with:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py --port PORT
```

`pio run -e esp32s3 -t upload` builds then uses the same validated uploader.
It writes the bootloader, partition table, initial OTA selection and app0,
preserving NVS, LittleFS and app1. `--dry-run` prints that command without
opening the serial port. `pio run -e esp32s3 -t clean` removes the default native
build and exported artifacts. Use a fresh `--build-dir` after changing component
paths or configuration defaults; `--output-dir` can isolate comparison artifacts.
See the [consolidation verification record](framework-consolidation-validation.md)
for build evidence, baseline recovery artifacts and pending physical-device checks.
