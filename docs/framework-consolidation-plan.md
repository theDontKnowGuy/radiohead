# Framework and build consolidation plan

Date: 2026-10-09

Status: stages 1–3 implemented and compiler/host verification for stage 4 passed.
Baseline device measurements and consolidation hardware acceptance remain pending.
See [the verification record](framework-consolidation-validation.md).

## Decision

Use one authoritative ESP-IDF build for the complete Radiohead firmware. Retain
Arduino-ESP32 as a component for the existing application and libraries that
depend on it. Removing Arduino is an optional later migration, justified by a
specific, measured benefit rather than a requirement of this plan.

This is the recommendation for the existing product. For a new implementation
with Radiohead's audio, networking and task requirements, pure ESP-IDF would be
a reasonable starting choice. Rewriting this working application solely to
reach that architecture would introduce regression risk without an established
performance or reliability benefit.

Arduino-ESP32 is built on ESP-IDF; they share the underlying runtime and drivers.
Radio and Spotify do not inherently require different frameworks. The current
combination reflects the dependencies chosen for each implementation. Espressif
officially supports [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html),
including retaining Arduino `setup()` and `loop()` entry points.

## Pre-consolidation implementation (baseline `abcf67b`)

- `platformio.ini` declared an Arduino build and attached the now-retired
  `tools/build_spotify_default.py` as a post-build script.
- PlatformIO builds the Arduino application first. The script then invokes
  [build-integrated.sh](../experiments/spotify-native/build-integrated.sh) to
  build the entire application again through ESP-IDF, including Spotify, and
  copies that integrated image to `.pio/build/esp32s3/firmware.bin`.
- The integrated [application component](../src/CMakeLists.txt) already depends
  on Arduino, the radio audio library, LovyanGFX, ArduinoJson and Spotify.
  The Spotify adapter itself also uses some Arduino interfaces.
- The native build currently obtains Arduino from PlatformIO's packages and
  LovyanGFX/ArduinoJson from `.pio/libdeps/esp32s3`. UI assets are prepared by a
  PlatformIO pre-build script. These dependencies must be prepared independently
  before the preliminary Arduino compilation can be removed.
- The default integrated path uses ESP-IDF 5.5.5 and Arduino 3.3.11, with pinned
  cspot/Bell candidate revisions and local patches. Preserve these versions
  during consolidation; a framework upgrade is a separate change.
- The uploader uses matching native bootloader, partition and initial OTA
  selection artifacts. Its current write set preserves NVS, LittleFS and app1.

The duplicated application build and configuration are the immediate maintenance
problem. Keeping Arduino as an explicit component is an acceptable end state.

## Target architecture and constraints

ESP-IDF/CMake owns the component graph, firmware configuration and final build
artifacts. One entry command prepares dependencies and UI assets, builds the
combined application, and exports matching upload/OTA artifacts. PlatformIO may
remain a convenience entry point if it delegates to this build without compiling
a second application. Select and document the exact entry point during
implementation.

Keep current feature-module ownership and the Spotify adapter boundary. `media`
continues to coordinate exclusive audio output ownership. Consolidation must
preserve routes, Preferences namespaces/keys and types, pairing, volume/tone
mapping, startup ordering, device pins, partition layout and OTA behavior.
Do not combine it with UI redesign, feature additions or vendor edits. Retired
features remain retired under the
[configuration and controls specification](ui/configuration-and-controls-plan.md).

Preserve the effective integrated configuration, including PSRAM, TLS,
exceptions, USB behavior and task/stack settings. Inspect generated configuration
as well as defaults: a cached build may contain settings not apparent in
`sdkconfig.defaults`. Neither lower memory consumption nor better sound is
assumed from a build-system change.

## Implementation stages

### 1. Record a reproducible baseline

- Inspect and preserve existing worktree changes; use an isolated checkout or
  build directory for comparisons. Identify the exact source state being tested.
- Record toolchain, framework and dependency revisions, applied candidate patches,
  asset-generation inputs, effective configuration and partition/flash manifests.
- Run the existing `pio run -e esp32s3` build and relevant host checks. Record the
  final integrated image hash/size, native RAM/flash report and OTA slot headroom.
  The preliminary Arduino size report does not describe the shipped image.
- Capture device behavior and resource measurements for the same source state.
  Existing Spotify acceptance gaps remain separate from consolidation results.

### 2. Make the native build self-contained

- Prepare pinned Arduino, LovyanGFX, ArduinoJson and cspot/Bell dependencies
  without requiring an Arduino application build. Retain local vendor patches.
- Run UI asset generation explicitly from the authoritative entry command.
- Make source lists, required compile definitions, board configuration and
  component paths reproducible without incidental PlatformIO cache state.
- Build in a fresh, isolated directory. Verify effective configuration and
  partition compatibility against the baseline; investigate binary differences
  rather than assuming byte-for-byte identity is required.

### 3. Replace the two-build handoff

- Remove preliminary Arduino application compilation and the replacement-image
  post-build flow once the self-contained build passes.
- Export firmware, factory image and matching boot/partition/OTA artifacts from
  the same build. Preserve the existing firmware output path if callers need it,
  or update all callers coherently.
- Update the uploader and its artifact/layout validation, release/OTA consumers,
  build documentation and firmware skill architecture reference together.
- Preserve partition checks and image-size limits. Compare all flash offsets
  and the uploader's write set; retain NVS, LittleFS and app1 preservation.
- Keep a recoverable baseline image and matching artifacts until device
  acceptance passes. Reverting this stage must not require a settings migration.

### 4. Validate and accept consolidation

- Verify fresh and incremental builds. Editing application code, Spotify
  components or asset inputs must rebuild the relevant shipped artifacts.
- Confirm the ordinary build command always includes Spotify. Check artifact
  consistency and upload command generation before writing the device.
- Run `git diff --check`, relevant existing host checks, and the authoritative
  firmware build. Review final native RAM/flash usage and OTA slot headroom.
- On the device, check boot/setup AP, continuous radio/podcast/Spotify playback,
  repeated source switches, mute/volume/tone, Wi-Fi recovery, web responsiveness,
  TFT/touch/encoder behavior, restart persistence, OTA, and current sleep/wake
  behavior. Check weather and artwork workloads during playback.
- Measure internal heap/largest block, task stack reserve and PCM continuity
  under comparable workloads. Run Spotify listening/resource checks with the
  USB serial monitor closed; never add USB logging to the PCM output task.

Use the existing [Spotify validation handoff](spotify-native-validation.md),
[evidence ledger](spotify-validation/progress.md) and
[S2 output handoff](spotify-validation/s2-output-handoff.md) for measurements
and outstanding acceptance. Consolidation does not establish that S3/S4 have
passed or erase known baseline failures.

### 5. Consider selective Arduino removal, if warranted

After consolidation is accepted, identify a concrete maintenance, resource or
reliability problem before proposing a module migration. Compare its measured
benefit with the porting and verification cost. Replace one dependency at a
time, retaining public behavior and persistence compatibility.

A pure ESP-IDF end state would require migrating Arduino networking, HTTP/OTA,
Preferences and utility interfaces, startup, and Arduino-dependent audio/display
integration. Native NVS access must preserve stored namespaces, keys and value
types. The Spotify adapter's Arduino usage must also be removed. Vendored audio
changes need the explicit authorization required by `AGENTS.md`.

No full Arduino removal or migration of Spotify into an Arduino-only build is
scheduled by this plan.

## Progress and completion evidence

| Stage | Status | Evidence required |
| --- | --- | --- |
| Baseline | Build/source record complete; device measurements pending | Recovery image/configuration saved locally; exact source and sizes in verification record |
| Self-contained native build | Compiler verified | Fresh `.pio/idf-build`, explicit dependency/asset preparation, configuration parity |
| Single build/export/upload flow | Host verified | Native and delegated builds, artifact rejection checks, upload dry run, updated release/OTA callers |
| Consolidation acceptance | Host checks passed; hardware pending | Incremental rebuild evidence complete; physical-device matrix and resource measurements remain |
| Selective Arduino removal | Optional; unscheduled | A specific problem and measured justification |

Record compiler/host verification separately from physical-device evidence. The
build consolidation is complete when one reproducible application build produces
all matching artifacts, required device checks pass, and known baseline defects
and remaining Spotify acceptance gaps are explicitly carried forward.

The implemented entry command is `python3 tools/build_firmware.py`; PlatformIO
is a convenience delegate. This record supplies compiler/host evidence only.
It does not establish hardware acceptance or close outstanding Spotify S3/S4 gaps.
