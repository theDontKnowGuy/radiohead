# Why there is one firmware build

**Implemented:** ESP-IDF/CMake builds the complete radio once; Arduino-ESP32 remains
as a component. PlatformIO prepares packages and delegates to that build.
Use [the build guide](building.md); [verification details](framework-consolidation-validation.md).

Previously (`abcf67b`), PlatformIO built the Arduino app, then a post-build script
built the whole application again with Spotify through ESP-IDF and replaced
`firmware.bin`. Removing that duplication was the goal. Removing Arduino was not.

## Rules for further build changes

1. Preserve pins, partitions, routes, Preferences types/keys, Spotify pairing,
   startup order, USB behavior, task stacks, volume/tone and OTA compatibility.
2. Prepare pinned dependencies and assets independently; CMake owns sources and
   compile definitions, `sdkconfig.defaults` configuration, `partitions.csv` layout.
   Inspect effective generated configuration as well as defaults.
3. Test a fresh build and incremental rebuilds after application, Spotify and asset
   edits. Export app/ELF/boot/partition/initial-OTA images and their hashes together.
4. Dry-run the validated sparse uploader before flashing. Preserve NVS, LittleFS
   and app1; retain a matching recovery set until physical testing passes.
5. Compare native memory reports, OTA headroom, settings and runtime behavior.
   Framework upgrades and UI changes belong in separate changes.

Recorded host checks establish the single-build flow, configuration/footprint parity,
artifact rejection and incremental rebuilds. Device acceptance remains open:
boot/AP setup, radio/podcast/Spotify, source switches, Wi-Fi recovery, web/TFT/touch/
encoder, tone/mute, persistence, OTA, weather/artwork and sleep/wake.
Measure internal heap/largest block, owned task stacks and PCM continuity under
comparable workloads, with USB serial closed. Never print from the PCM output task.

Selective Arduino removal is optional and unscheduled. Attempt it only for a
measured benefit, one dependency at a time, retaining NVS compatibility. A build-system
change alone does not establish better sound or lower runtime memory use, or close
[Spotify's remaining tests](spotify-validation/progress.md).
