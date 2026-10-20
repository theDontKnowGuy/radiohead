# Three-version audio comparison

Date: 2026-10-20. Branch: `codex/audio-storage-diagnostics`.
Main baseline: `8d75e4b` (`change amp legs`).

All source, tools and investigation records accumulated during this debugging
session are included on this branch. Build products remain ignored under `.pio`.
The branch's normal firmware still saves settings; storage suppression is opt-in.
Nothing in this comparison automatically flashes the radio.

## The three exports

| Version | Export directory | `/api/player` identification |
| --- | --- | --- |
| A: main at `8d75e4b` | `.pio/build/comparison-main` | `storageQuietTest` field absent |
| B: this branch, normal | `.pio/build/comparison-branch` | `storageQuietTest: false` |
| C: this branch, storage disabled | `.pio/build/comparison-storage-quiet` | `storageQuietTest: true`; test banner visible |

Each directory contains its matching bootloader, partition table, OTA-selection
data, application and validated `artifacts.json`. `comparison-source.json`
alongside them records the source commit, mode and application SHA-256.
All three use the same partition layout and preserve NVS/artwork on upload.

The normal branch includes the volume-range correction:
`audio.setVolumeSteps(volCurve[21])` (55 steps). Main retains the library's
21-step default, so the application's curve clamps UI indexes 13–21 to maximum.
**Begin at a low setting on main and match listening loudness, not just the
displayed index.** This is a real difference between A and B. B and C use the
same corrected gain mapping.

C also disables LittleFS artwork access, OTA/updater activity and Wi-Fi/PHY NVS,
and caches saved Spotify credentials at boot. Volume/tone changes work in RAM
but are not saved. Normal program-code/constant reads from flash continue.
See [the storage diagnostic](../../experiments/storage-quiet/README.md).

## Install one version at a time

Run from this repository with serial monitors closed. The radio was enumerated
as an Espressif USB JTAG/serial device at `/dev/cu.usbmodem1201`; adjust the port
if it changes.

A — main:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/comparison-main --port /dev/cu.usbmodem1201
```

B — branch with normal storage:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/comparison-branch --port /dev/cu.usbmodem1201
```

C — branch with storage disabled:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/comparison-storage-quiet --port /dev/cu.usbmodem1201
```

Use USB for every swap, including restoring from C, whose OTA is disabled.
Verify the API mode after each upload rather than inferring it from a successful
build or a browser notification. Refresh the browser after changing versions.

Keep the working amplifier/speaker, wiring, supply and source unchanged. EQ zero.
Report the volume-change crackle separately from the steady sandy sound. Wait
three seconds between isolated volume changes and ten seconds before judging
steady playback. A and B save settings; C reloads whichever settings were last
persisted, so check volume/EQ after each restart.

| Version | Source / displayed volume / matched loudness | Crackle on change | Crackle about 1 s later | Steady sandy sound |
| --- | --- | --- | --- | --- |
| A: main | | | | |
| B: branch | | | | |
| C: branch storage disabled | | | | |

No cause has yet been established for the reported disappearance of the crackle:
the live readback after that report showed `storageQuietTest: false`. This
three-version comparison is intended to replace that unverified attribution.

## Rebuild

While on this branch:

```sh
python3 tools/build_firmware.py --output-dir .pio/build/comparison-branch
python3 tools/build_firmware.py --storage-quiet --output-dir .pio/build/comparison-storage-quiet
```

A was built in the detached worktree `.pio/comparison-main-source`, at the exact
main baseline, with its own build/generated-assets directories and shared
installed dependencies. Main itself was not modified. The comparison-source
records describe the prepared snapshots; regenerate them if later source
changes are rebuilt into these folders.
