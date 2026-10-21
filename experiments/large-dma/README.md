# Local I2S DMA queue comparison

2026-10-21. User reports more right-speaker crackles with Radiohead than the
Internet-Radio reference on the **same hardware and stream**. This separate
4.0.0 image tests only the final local I2S DMA queue depth. It is an experiment,
not a confirmed audio fix.

| Setting | Baseline | Test |
| --- | ---: | ---: |
| DMA descriptors | 6 | 28 |
| Frames per descriptor | 256 | 256 |
| Maximum buffered time at 44.1 kHz | 34.83 ms | 162.54 ms |
| Stereo 32-bit sample payload | 12,288 B | 57,344 B |

Only the descriptor count changes. The 28-descriptor setting closely matches
the reference's 161.09 ms without changing Radiohead's per-buffer frame count.
Additional internal DMA sample payload is **45,056 B (44 KiB)**, plus driver
metadata overhead. The allocation happens at runtime; a static RAM report
cannot establish that the allocation or late Spotify-to-local restoration fits.

`src/media.cpp::mediaConfigureOutput()` assigns the public
`audio.settings.DMA_DESC_NUM` before `setPinout()`. The library stays unchanged.
Sample rate selection, slot width, pins, decoded-PCM buffers, scheduling,
volume/EQ processing, persistence, display, network and Spotify code are retained.
The test does not automatically fall back to six descriptors, which would
invalidate a listening comparison. Spotify's own output queue is not changed;
returning to local output recreates the larger local queue.

## Build

```sh
python3 tools/build_firmware.py --large-dma
```

This enables `RADIOHEAD_LARGE_DMA_TEST` only in `.pio/idf-large-dma-test/` and
exports matching artifacts to `.pio/build/large-dma-test/`. The manifest records
`diagnostic: large-dma-test`, audio library version and the DMA parameters.
Other diagnostic modes cannot be combined with it, and normal build/export
directories are rejected for this profile. Normal builds explicitly set the
flag OFF and continue using six descriptors.

`pio run -e esp32s3` builds the normal image. It does not select this experiment.

## Install and compare

Close serial monitors. From the project folder, select the test export:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/large-dma-test --port YOUR_PORT
```

The preceding 4.0.0 baseline has been preserved outside PlatformIO's export
cleanup at `.pio/dma-comparison/baseline-400/`. Restore it with:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/dma-comparison/baseline-400 --port YOUR_PORT
```

Both use the same partition layout and preserve saved settings/artwork. After
installing the test, read `/api/spotify/resources` in the browser and check
`localDmaDescriptors: 28` and `localDmaFrames: 256`. The updated normal build
reports 6 and 256. The preserved pre-test baseline lacks these new fields.
The baseline and test can have the same image size, so size alone cannot
identify this experiment. The diagnostics also expose internal/DMA free,
minimum and largest-block sizes plus allocation-failure counts; record those
before and after any Spotify return test. These are not underrun counters. Record the stream and volume/EQ settings;
keep them identical between passes. Compare baseline/test/baseline with the same
passage and power/wiring. Record right-speaker crackle separately from sandy
sound, including whether it occurs during steady playback or volume changes.
Keep the USB serial monitor closed while listening.

Spotify remains enabled. First evaluate the requested local-radio comparison;
Spotify playback and return to local radio need separate on-device checks with
this larger allocation. No underrun counter has proved DMA starvation, and a
cleaner listening result would implicate buffering without identifying every
cause of the noise.

## Verification

- Normal `pio run -e esp32s3` and the separate `--large-dma` build pass.
- Normal/test images: 4,514,800 / 4,514,816 B. Test app-slot headroom:
  2,038,784 B. Both report 260,523 B static DIRAM (76.23%), with 81,237 B
  static headroom; the extra DMA payload is allocated at runtime.
- Normal/test SDK configurations and partition bytes match exactly. The compile
  graphs retain Spotify and select the large-DMA define only in the test.
- Host checks exercise the actual output-configuration function in both modes:
  the depth is applied before I2S initialization; frame size, pins, boot mute,
  volume-step count and EQ remain unchanged.
- Actual diagnostics serialization with maximum 32-bit counters produces valid
  JSON in 763 of 1,100 bytes, including the new configured-DMA fields. This
  readback runs only on a diagnostics HTTP request, outside audio servicing.
- Conflicting profiles and attempts to overwrite normal directories are rejected.
  Export hashes, factory consistency, partition layout and flash-helper dry run
  pass. `git diff --check` passes; no vendored library files changed.
- Fresh framework compilation reports existing ESP_SR partition and cspot
  deprecation warnings. The final incremental application builds add no warnings.

No image has been flashed by the agent. Runtime allocation, audible effect and
source transitions remain unverified.
