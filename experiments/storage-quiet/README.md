# Radio playback with storage disabled

2026-10-20. Built and host-checked; not flashed by the agent. **Current live radio
reports `storageQuietTest: false`. Diagnostic installation is not confirmed.**
User reports vanished volume-change static and remaining sandy music, but those
observations cannot yet be attributed to this diagnostic; see the correction below.

This is the normal radio with a temporary storage diagnostic enabled. It tests
whether persistent-storage activity contributes to web-volume crackles or the
continuous sandy sound. It does not select a root cause or change the gain
ramps, volume mapping, decoder, EQ, I2S setup, audio priorities or pin assignments.
The pre-existing volume-range correction remains included.

There is no SD-card access in this application. The relevant data storage is
internal flash NVS/Preferences and LittleFS. This test does not eliminate normal
memory-mapped instruction/constant reads from program flash or embedded assets.

## Install and restore

From the repository root, close serial monitors and flash the already built
image using the settings-preserving helper:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/storage-quiet \
  --port /dev/cu.usbmodem1201
```

That port was enumerated as an Espressif USB JTAG/serial unit on 2026-10-20.
Adjust it if the radio moves to a different USB port. The other enumerated
USB modem port was an LG monitor, not the radio. No device port was opened by
the agent.

The original pre-experiment radio export is preserved separately. Restore it
over USB with:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/storage-quiet-baseline \
  --port /dev/cu.usbmodem1201
```

Neither command erases NVS or the artwork partition. OTA is intentionally
disabled in the diagnostic, so restore over USB. Do not substitute a factory
image write or an erase-all operation. The known rapidly heating amplifier is
not a suitable listening-test load.

## Listening comparison

1. Open the radio's usual web page after boot. It should show:
   **Audio test: settings are temporary; storage and updates are disabled.**
   `/api/player` also returns `storageQuietTest: true`.
2. Play the same internet-radio source, EQ zero. Start at a comfortable level.
   Make isolated volume steps around 15/16/17, both up and down; leave at least
   three seconds between changes.
3. Record separately: crackle at the change, crackle roughly one second later,
   and continuous sandy/unstable sound with the control untouched at 16.
4. If useful, repeat on Spotify at matched listening loudness. Do not change
   wiring, supply or capacitors during the comparison.

Changes apply to RAM for the session. Restart reloads the original stored
volume/tone/station values. Configure credentials/stations in the normal image,
not in this diagnostic.

If the crackle disappears but the steady dirt remains, storage may contribute
to the transient without explaining the other symptom. If both remain, this
test weakens persistent-data storage as a necessary cause. An improvement alone
does not identify one particular flash write: filesystem artwork and the updater
worker are disabled too. A narrower follow-up would isolate those differences.

## Implementation boundary

- Default build remains normal; `--storage-quiet` selects separate IDF and
  export directories and records `diagnostic: storage-quiet` in the manifest.
- Saved settings, boot mode, Wi-Fi credentials, touch calibration and Spotify
  app credentials/pairing load during setup. After the boot-read barrier,
  Spotify and touch status use cached values; settings reloads are blocked.
- Settings saves, including the one-second deferred volume save, are no-ops.
  Dedicated durable-save APIs and factory reset reject writes. Saved Wi-Fi
  network activation can still update the active network in RAM.
- LittleFS is never mounted, formatted, repaired, read or written. Saved
  station artwork falls back to the normal missing-artwork display.
- Wi-Fi uses RAM storage; SDK Wi-Fi NVS and PHY calibration storage are disabled.
  PHY initialization data comes from the firmware, not a data partition.
- Automatic updater startup and flash installation are disabled. Browser OTA
  upload callbacks do nothing; the completion route returns HTTP 503.
- Existing Spotify pairing loads at boot. New pairing can work in RAM but is
  not retained over a restart. Credential contents are never printed.
- Audio output code, library files, buffering and clocks are unchanged.
  Screen/network playback remain active, apart from saved artwork and updater.

## Build and verification

```sh
python3 tools/build_firmware.py --storage-quiet
pio run -e esp32s3
python3 tools/check_spotify_credentials.py
python3 tools/check_boot_modes.py
node tools/check_spotify_web.cjs
python3 tools/check_firmware_artifacts.py
git diff --check
```

Both firmware builds and the listed checks passed. The credential host test
uses the actual production loader with a fake store: it tests saved and absent
credentials, removes the store after boot, forces future opens/reads to fail,
and verifies repeated runtime loads use the boot snapshot with no further
opens or writes. The normal persistence tests also pass.

The diagnostic export passed hashes/layout/factory consistency checks; its
partition table is byte-identical to the original baseline. Flash-helper dry
runs write only bootloader, partition table, OTA-selection data and application,
not stored settings/artwork. Diagnostic application/Spotify object libraries
have no undefined calls to Preferences put/clear/remove, LittleFS filesystem
methods, Update begin/write/end, or NVS set/commit. This supplements the source
audit; it is not a runtime bus trace.

| Image | Application size | App slot free | Static DIRAM used / free |
| --- | ---: | ---: | ---: |
| Storage diagnostic | 4,444,128 | 2,109,472 | 260,491 / 81,269 |
| Rebuilt normal radio | 4,514,720 | 2,038,880 | 260,523 / 81,237 |

Diagnostic application SHA-256:
`7b93c598125b3797da8e00851a0397952a677d38f852d267a7ac0562d3725d6a`.

The fresh diagnostic build emits upstream ESP_SR partition and cspot
shared-pointer deprecation warnings; no application compile error occurred.
No vendor files were modified. No flash, powered listening test or improvement
was performed or directly verified by the agent.

## First listening result — 2026-10-20

**Attribution correction:** the following was initially interpreted as a
storage-quiet result without reading back the running mode. That interpretation
is not established. The subsequent live check below found the test disabled.

User reports: **"static noise when change volume is now gone."** This is user
listening evidence, not a scope measurement or a device-identity readback.
It supports storage-related activity contributing to the transient. The
one-second deferred settings save is a leading candidate, but this build also
removes artwork storage access and the updater; the result alone does not prove
which operation caused it. Do not restore runtime persistence merely to complete
this test. User confirms continuous sandy/capped/unstable music is **still
present** after ten seconds at volume 16 with EQ zero. Treat the transient and
steady distortion as separate diagnostic branches. Persistent-data activity is
not necessary for the latter in the earlier verified audio-only diagnostic;
this particular storage-quiet comparison is not verified.

## Running-mode correction — 2026-10-20

User notices volume/tone settings still save and asks what changed. A read-only
GET to the previously used radio address `http://192.168.11.127/api/player`
returns `storageQuietTest: false`, volume 14, bass/mid/treble 0 and source radio.
This proves the storage-quiet mode is not active on that radio at this check.
It does not establish which image ran at the exact earlier listening moment.

The normal export `.pio/build/esp32s3` deliberately keeps persistence enabled.
The separate `.pio/build/storage-quiet` export enables this experiment. A normal
`pio run -e esp32s3` or default upload does not select the test. The agent built
both and did not flash either. The vanished crackle remains a valid user
observation, but attributing it to suppressed flash activity was premature.
Verify `storageQuietTest: true` after installing the explicit diagnostic before
using subsequent observations as a controlled storage comparison.
