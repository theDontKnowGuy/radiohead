# Build and flash

You need macOS, Python 3, PlatformIO, Git, and `sips`, `xxd`, Swift for UI assets.
Run from the project folder; the first build downloads dependencies.

```sh
python3 -m pip install Pillow
pio run -e esp32s3
pio run -e esp32s3 -t upload --upload-port PORT
```

Replace `PORT` with the radio's serial port. To flash an existing build:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py --port PORT
```

Add `--dry-run` to inspect the upload without opening the port. The uploader preserves
NVS settings, LittleFS and app1. OTA uses `.pio/build/esp32s3/firmware.bin`.

`python3 tools/build_firmware.py` is the direct build command. PlatformIO delegates
to this single ESP-IDF 5.5.5 / Arduino 3.3.11 build, including Spotify.
Use `pio run -e esp32s3 -t clean` after changing configuration defaults or component paths.
Keep matching boot/partition/app artifacts together; [build details](framework-consolidation-validation.md).
