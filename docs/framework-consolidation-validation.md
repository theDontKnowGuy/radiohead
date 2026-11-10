# Single-build verification

The 2026-10-09 change replaced two application builds with one ESP-IDF/CMake build
using Arduino as a component. Spotify is included. Host checks passed; device
comparison with the baseline remains pending. [Build commands](building.md).

## Reproduce the checks

```sh
python3 tools/build_firmware.py
pio run -e esp32s3
python3 tools/check_firmware_artifacts.py
~/.platformio/penv/bin/python tools/flash_built_firmware.py --port PORT --dry-run
```

CMake owns sources/definitions; `sdkconfig.defaults` and `partitions.csv` own
configuration/layout. Default native directory: `.pio/idf-build`; exports:
`.pio/build/esp32s3/`. Clean or use a fresh build directory after configuration or
component-path changes. The Arduino loop stack retains its 16 KiB definition.

Pinned inputs: pioarduino 55.03.311, ESP-IDF 5.5.5, Arduino 3.3.11,
LovyanGFX 1.2.21, ArduinoJson 7.4.3, GCC 14.2.0 (`esp-14.2.0_20260121`),
CMake 4.0.3, Ninja 1.13.1, PlatformIO 6.2.0.
Candidate `9c51b087d488b8f7a516f77582f1fc965c3cb643`,
cspot `37b650a526625773a5a0e0a90b57527f914e7641`,
Bell `ead27050f63aba369631ec99ad68322685aff5a6`.

## Results and recovery

Fresh native/delegated builds, cache-free dependency/asset preparation, clean,
and incremental application/Spotify/CSS rebuilds passed. Effective Kconfig,
normalized compiler commands, symbol sizes and native memory matched baseline
`abcf67b4095322717859b36404e6e9fa229e9982`; partition/initial-OTA images matched
byte-for-byte. App/boot hashes differ with timestamps/linking; runtime parity is untested.

Baseline app: 4,496,080 B; OTA slot 6,553,600 B; free 2,057,520 B;
DIRAM 260,391 B; external BSS 40,224 B. These are link figures, not free runtime heap.
Recovery set is locally ignored under `build/framework-baseline/` (images, ELF/map,
configuration, logs, dependency/patch records). Baseline app SHA-256:
`104f0faba029ec769d759ef211705aa4aec34ea8b776e9a24aa1947f288ea612`.
Keep it until device comparison; do not erase the whole chip to revert.

Exports include app/factory/ELF, bootloader, partitions, initial OTA and `artifacts.json`.
The uploader checks hashes, partition MD5/layout, app size and factory consistency
before opening the port. Interrupted exports invalidate the manifest.

| Write offset | Image |
| --- | --- |
| `0x0` | Bootloader |
| `0x8000` | Partition table |
| `0xe000` | Initial OTA selection |
| `0x10000` | app0 |

DIO upload header, 80 MHz, 16 MB; Kconfig retains QIO/autodetection.
NVS `0x9000`, app1 `0x650000`, LittleFS `0xc90000` are outside this write set.
Tests rejected NVS writes, corruption, mismatched factory/app, stale hashes and oversized apps.
Startup, credentials, roaming, touch, podcasts, boot modes and syntax/diff checks also passed.

2026-10-29 repair: resolve package paths through PlatformIO rather than assuming
an unsuffixed directory, which selected Arduino 2.0.17 instead of 3.3.11.
Fresh/incremental builds passed: app 4,688,208 B, slot free 1,865,392 B,
DIRAM 261,835 B (79,925 B remaining). No device was flashed for these checks.

Still test on hardware: startup/AP, all audio sources/switches, Wi-Fi/web/display/input,
persistence/OTA and sleep; compare heap/largest block, task stacks and PCM continuity.
Use [Spotify checks](spotify-validation/progress.md) with USB serial closed;
consolidation does not close S3/S4 or known playback failures.
