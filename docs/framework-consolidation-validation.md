# ESP-IDF build consolidation verification

Date: 2026-10-09

The complete application now builds once with ESP-IDF/CMake. Arduino-ESP32 is
an explicit component, retaining the existing `setup()` / `loop()` startup.
Spotify is required by the application component on every ordinary build.
Implementation and host acceptance are complete; physical-device acceptance
and comparable baseline resource measurements remain pending.

## Commands and ownership

```sh
python3 tools/build_firmware.py
pio run -e esp32s3
python3 tools/check_firmware_artifacts.py
~/.platformio/penv/bin/python tools/flash_built_firmware.py --port PORT --dry-run
```

Both build commands use `tools/build_firmware.py`. PlatformIO's pre-script
selects a delegate builder before its platform application builder runs. No
PlatformIO framework builder is selected; pinned ESP-IDF/Arduino/CMake/Ninja
packages and library dependencies are installed explicitly. This also avoids
the pinned platform's ESP-IDF package setup replacing PlatformIO Core 6.2's
SCons runner with its older version while the runner is executing. The
firmware architecture reference, README, release builder, Spotify helper and
uploader now describe or consume this single flow.

`CMakeLists.txt` and component CMake files own sources and required compile
definitions. `sdkconfig.defaults` owns native configuration and `partitions.csv`
owns layout. Each native build directory has its own generated `sdkconfig`;
the old root configuration is no longer an incidental input. Retained compile
definitions include USB mode/CDC, PSRAM, debug level, 16 KiB Arduino loop stack,
`FIRMWARE_BUILD="esp32s3"` and the Spotify feature flag. The Kconfig loop-stack
default remains 8192; the existing compile definition still supplies 16384.

The build prepares dependencies and UI assets before invoking IDF. Source
assets remain unchanged; macOS `sips`, `xxd` and Swift are currently required
for the existing conversion recipe. Framework/component path overrides require
a fresh build directory. Configuration-default changes likewise need a fresh
directory or `pio run -e esp32s3 -t clean` so cached Kconfig choices cannot hide
them. The default native directory is `.pio/idf-build`.

## Baseline and dependency state

The worktree was clean at source commit
`abcf67b4095322717859b36404e6e9fa229e9982`. The pre-consolidation
`pio run -e esp32s3` passed before editing build files.

| Input | Preserved version/revision |
| --- | --- |
| PlatformIO platform | pioarduino 55.03.311 |
| ESP-IDF | 5.5.5 (`framework-espidf` package 3.50505) |
| Arduino-ESP32 | 3.3.11 |
| LovyanGFX | 1.2.21 |
| ArduinoJson | 7.4.3 |
| Native candidate | `9c51b087d488b8f7a516f77582f1fc965c3cb643` |
| cspot | `37b650a526625773a5a0e0a90b57527f914e7641` |
| Bell | `ead27050f63aba369631ec99ad68322685aff5a6` |
| Xtensa compiler | GCC 14.2.0, esp-14.2.0_20260121 |
| CMake / Ninja | 4.0.3 / 1.13.1 |
| PlatformIO Core | 6.2.0 |

The existing patched candidate was compared with a freshly cloned candidate
prepared by the committed `prepare.sh`. Tracked diffs and new sink source files
matched in all three repositories. The managed-component `dependencies.lock`
is unchanged. Patch recipes and firmware asset sources are unchanged.

Recovery evidence is local and ignored under `build/framework-baseline/`:
`radiohead.bin`, matching boot/partition/initial OTA and factory images, ELF/map,
full effective configuration, build log, `baseline.json` and
`dependencies.json` with candidate revisions and patch fingerprints. Keep these
until device acceptance. The old `.pio/spotify-idf-build` is also retained.
Reverting the build changes and flashing the baseline's matching artifacts
requires no settings migration. Avoid full-chip erase and factory-image writes
over an existing configured device; the sparse write set below preserves NVS.

Baseline application SHA-256:
`104f0faba029ec769d759ef211705aa4aec34ea8b776e9a24aa1947f288ea612`.

## Compiler and host evidence

Fresh native and PlatformIO delegate builds passed. An additional fresh build
isolated the existing `.pio/libdeps/esp32s3` and `.pio/ui_assets` caches and
removed the default native/export directories. Pinned libraries were installed
again and assets generated without any preliminary Arduino compilation.
`pio ... -t clean` was checked to remove both native and export directories.

The effective generated configuration matches the baseline with zero differing
Kconfig values. After normalizing build-directory paths, all compiler commands
match. ELF symbol sizes and native memory sections match the baseline. The
partition table and initial OTA-selection binaries match byte-for-byte.

| Native metric | Baseline and consolidated build |
| --- | ---: |
| Application binary | 4,496,080 bytes |
| OTA app slot | 6,553,600 bytes |
| OTA slot headroom | 2,057,520 bytes (31%) |
| Flash code | 2,321,114 bytes |
| Flash data | 1,962,684 bytes |
| DIRAM total including resident code | 260,391 bytes |
| DIRAM initialized data / BSS | 91,140 / 64,760 bytes |
| External RAM BSS | 40,224 bytes |

These are linker/build figures, not measurements of runtime free heap or stack
reserve. The preliminary Arduino report is deliberately excluded.

The rebuilt application and bootloader hashes differ from the cached baseline.
App/boot descriptors contain different build timestamps and ELF hashes; a fresh
link also changes symbol placement and relocation bytes. This is not a claim
of binary identity. Equal compiler commands/configuration/symbol sizes and
matching partition artifacts support configuration/footprint parity, while
runtime equivalence still requires device checks.

Incremental verification touched `src/main.cpp` and the Spotify output source
and temporarily added a newline to the CSS input. The corresponding application,
Spotify and web-asset consumers recompiled and the application was relinked and
exported. The CSS was restored and a subsequent build rebuilt/exported its
original content. No product source or asset content changes remain.

Existing checks passed for startup sequencing, Spotify credential storage,
Wi-Fi roaming, touch/input transitions, podcast parsing and persisted boot modes.
Shell/Python syntax checks and `git diff --check` passed. Artifact checks passed
for the actual application/factory set, and rejected an NVS write, corrupted
bootloader, factory/application mismatch, stale application hash and oversized
OTA application. No vendored audio changes or generated images are committed.

## Export and upload contract

`.pio/build/esp32s3/` contains `firmware.bin`, `firmware.factory.bin`,
`firmware.elf`, `partitions.bin`, bootloader, partition table, initial OTA
selection and `artifacts.json`. The latter records hashes of every exported
artifact. Export invalidates its previous manifest before copying; interrupted
exports fail validation. The uploader validates hashes, exact partition entries,
partition MD5, application slot size and factory/application consistency before
opening a port. It no longer depends on the native build directory remaining
present. Releases retain the same OTA image name and firmware build identifier.

| Offset | Sparse uploader artifact |
| --- | --- |
| `0x0` | Bootloader |
| `0x8000` | Partition table |
| `0xe000` | Initial OTA selection |
| `0x10000` | app0 application |

Upload settings remain DIO image-header mode, 80 MHz, 16 MB. QIO is retained in
Kconfig with the same flash auto-detection behavior as the baseline. NVS at
`0x9000`, app1 at `0x650000`, and LittleFS at `0xc90000` are outside the write
set. Upload command generation was checked without opening or resetting a port.
No firmware was uploaded and no release was published during this task.

## Physical-device acceptance still required

Record boot/setup AP, radio/podcast/Spotify continuity, repeated source switches,
mute/volume/tone, Wi-Fi recovery, web responsiveness, TFT/touch/encoder operation,
restart persistence, OTA, weather/artwork workloads and current sleep/wake behavior
for the consolidated image. Compare internal heap/largest block, owned task stack
reserve and PCM continuity with the saved source baseline under the same workloads.
Keep the serial monitor closed for Spotify listening/resource checks; opening the
USB serial port resets this radio. No USB logging was added to its PCM output task.

The existing [Spotify evidence ledger](spotify-validation/progress.md),
[native validation handoff](spotify-native-validation.md) and
[S2 output handoff](spotify-validation/s2-output-handoff.md) remain authoritative
for unresolved acceptance. This build change supplies no new physical-device
measurements and does not close S3/S4 or known baseline failures.
