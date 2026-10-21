# One-run sandy-sound recording test

Prepared 2026-10-20 on `codex/audio-storage-diagnostics`. **Not flashed.**
This extends the existing standalone IDF audio test with one automatic profile.
Production firmware, pins, persisted settings and volume curve are unchanged.

The existing implementation and listening/counter/phone-analysis records were
read first: [run sheet](README.md), [results](../../docs/hardware/audio-only-results.csv),
[validation](../../docs/hardware/audio-only-validation.md), and
[recording analysis](../../docs/hardware/audio-only-iphone-recording-analysis.md).
Music remains sandy with standalone firmware and either active speaker; the
quiet 1 kHz sine was clean. Supply changes, added amplifier decoupling and
16/32-bit slots did not audibly help. This test prepares a reference for a new
recording; it does not establish a cause or repair.

## Build, flash and restore

Run from the repository root. The image is already built at
`.pio/build/audio-only-recording-sequence/firmware.bin`. Reproduce it with:

```sh
python3 tools/build_audio_only.py --recording-sequence
python3 tools/check_audio_only_build.py --recording-sequence
```

Flash **only when ready to record**, with the serial monitor closed:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-recording-sequence \
  --port /dev/cu.wchusbserial5C932096561
```

Restore the existing normal radio image over USB:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/esp32s3 \
  --port /dev/cu.wchusbserial5C932096561
```

That WCH port is present at preparation time; substitute its new name if it
changes. Enumerate without opening a monitor using
`~/.platformio/penv/bin/python -m serial.tools.list_ports -v`.
Both commands use the existing validated uploader and shared partition table.
They write bootloader, partition table, initial OTA selection and app0, preserving
NVS, app1, filesystem and coredump regions. Add `--dry-run` to inspect either
command without contacting the board. There is no OTA/web service in this image.

## Signal and timestamp list

44,100 frames/s, signed 16-bit Philips stereo, BCLK39 / DIN40 / LRCLK41,
no EQ. **Only the digital left slot is driven; right is always exact zero**,
including all transitions. Music copies the existing fixture's left channel;
there is no stereo sum. Physical speaker selection still depends on the modules'
SD wiring. No encoder/web/serial interaction or audible cue pips are used.

Times are relative to the first I2S frame, **after boot**, not power-on or the
start of the phone recording. The five-second opening silence is followed by
the same two-second gap used between every segment.

| Time (s) | Segment | Actual active-channel peak |
| --- | --- | --- |
| 0–5 | Silence | 0 |
| 5–7 | Gap | 0 |
| 7–12 | 250 Hz, low | 519 counts, −36.005386 dBFS |
| 12–14 | Gap | 0 |
| 14–19 | 1 kHz, low | 519 counts, −36.005386 dBFS |
| 19–21 | Gap | 0 |
| 21–26 | 250 Hz + 1 kHz, low | **Combined** 519 counts, −36.005386 dBFS |
| 26–28 | Gap | 0 |
| 28–38 | Reference music, low | 896 counts, −31.262573 dBFS |
| 38–40 | Gap | 0 |
| 40–45 | 250 Hz, +6 dB | 1,036 counts, −30.001539 dBFS |
| 45–47 | Gap | 0 |
| 47–52 | 1 kHz, +6 dB | 1,036 counts, −30.001539 dBFS |
| 52–54 | Gap | 0 |
| 54–59 | 250 Hz + 1 kHz, +6 dB | **Combined** 1,036 counts, −30.001539 dBFS |
| 59–61 | Gap | 0 |
| 61–71 | Reference music, +6 dB | 1,788 counts, −25.261383 dBFS |
| 71 onward | Finished; digital silence indefinitely | 0 |

Each audible segment includes symmetric 30 ms fades, with exact zero at its
first/last sample. Tone phase and music position restart identically each pass.
The existing approximately 8.613-second music fixture wraps once during each
10-second music segment, retaining its existing 30 ms loop-edge fades.

The louder tones use the previously clean **−30 dBFS** sine level. Louder music
uses the previously audible hands-free test's **−12 dB fixture trim**; the lower
pass is 6 dB below each. Tone and music peaks intentionally differ to preserve
those conservative settings. The fixture's left peak is 7,119 counts before
these trims; its stereo normalization originally targeted −12 dBFS globally.

Exact gain multipliers are integers divided by 2^30:

| Signal | Low multiplier numerator | Higher multiplier numerator |
| --- | ---: | ---: |
| Tones (nominal −36 / −30 dBFS) | 17,017,661 | 33,954,698 |
| Music fixture (nominal −18 / −12 dB trim) | 135,176,087 | 269,711,752 |

The 250 Hz and 1 kHz signals have equal weight in the combined waveform. Their
**sum** is divided by its sampled peak (1.9281225340594608), then quantized to
the same 32,767-count source peak as either single tone. It is not two full-level
tones added together. Final gain/fade uses integer arithmetic and nearest
rounding, ties away from zero. Quantization makes measured peak increases
6.003848 dB for tones and 6.001190 dB for music, rather than exactly 6.000000 dB.
Full scale in these figures is 32,767 counts.

## Reference files

Generated alongside the complete flash export (kept outside Git):

- `.pio/build/audio-only-recording-sequence/sequence-reference.wav`
- `.pio/build/audio-only-recording-sequence/sequence-timestamps.csv`
- `.pio/build/audio-only-recording-sequence/sequence.json` — exact gains and hashes

The WAV contains **3,219,300 stereo frames / 73 seconds**, including the first
two seconds of the indefinite final silence. Its 44-byte WAV header is not sent
to I2S; its PCM payload is the exact interleaved sample sequence supplied to
the driver under uninterrupted operation. Both firmware and exporter use
`main/audio_sequence.h` and the same generated integer tone tables/music bytes.
The initial 1,024 DMA-preloaded zero frames are included in the opening five
seconds. There are no unexported cue sounds or segment-boundary buffer rounding.
At 71 seconds the signal stops; I2S clocks continue with digital zeros to avoid
introducing a clock-stop transition. Power-cycle/reset to replay once.

WAV SHA-256:
`c4c50fa06a2930fc0bf32f513d438863bdbff10afaf592824910fb0a05f40cb5`.

## Record once

1. Keep the phone stationary near the speaker that responds to the digital left
   channel. Keep the hardware/supply and phone position unchanged throughout.
2. Start recording **before** flashing or resetting, then let the whole sequence
   run once. Keep the serial monitor closed. Allow about 80 seconds after reset
   to include boot time, all segments and the final silence.
3. Send the **original recording file**, without trimming, normalization or
   messaging-app recompression. Note which of the eight audible segments sound
   sandy, and whether the higher pass changes that texture. Identify the active
   physical speaker; if both play, mention it.

## Verification and limits

The build runs a sanitizer-enabled native exporter and an independent integer
sample comparison across every audible frame. Checks cover all boundaries,
fades, loop wrapping, single/combined peak equality, exact inactive-channel and
gap zeros, DMA-preload alignment and the indefinite silent tail. The existing
55-case native engine/control/write regression suite also passes.

`python3 tools/check_audio_only_build.py --recording-sequence` passes export
hash/factory/layout checks, verifies the actual embedded music fixture, confirms
the isolated source/component graph and absence of encoder input symbols, and
checks both settings-preserving flash/restore plans and invalid CLI combinations.
`git diff --check` and Python syntax checks pass.

The isolated application is **1,713,616 bytes**, leaving **4,839,984 bytes** in
the app slot. Static DIRAM is **67,235 / 341,760 bytes (19.67%)**, with
274,525 bytes free. ESP-IDF is the pinned 5.5.5; Arduino is not linked.

Only the standalone project, its existing host tools and this documentation
change. No `src/`, `include/`, production build config, dependency, partition or
vendored code changes. Generated WAVs and binaries remain ignored.

Initial preparation recorded the normal-radio firmware SHA-256
`0789f30c9fc0fcaf94e3e554917e9992f1508fee39028af316befda23799d238`.
The normal native build used by `pio run -e esp32s3` also passed via
`python3 tools/build_firmware.py --prepared --output-dir .pio/build/audio-sequence-radio-check`,
using a separate export so the restore image was not overwritten. That check
image is 4,514,720 bytes, with 2,038,880 bytes of app-slot headroom; static DIRAM
260,523 / 341,760 bytes (76.23%). Existing dependency CMake warnings remain.
The first isolated configure emitted nonfatal ROM-GDB-symbol/environment and
Kconfig default notices; compilation/export succeeded.

**No device was flashed or measured.** The WAV establishes intended/submitted
digital contents, not measured wire data: actual DMA continuity, clocks,
amplifier channel selection, acoustic output and phone processing remain
unverified. Phone timing includes boot delay and acoustic latency; align using
the first tone/music, rather than assuming recording time zero equals I2S time
zero. Digital zeros do not physically disconnect the other amplifier. A phone
recording alone cannot uniquely identify an electrical or acoustic fault.

The preload interpretation was checked against the installed IDF 5.5.5 driver
and [Espressif's I2S API documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/i2s.html).

## Louder separate-speaker images

After the user supplied the recording, they reported all segments sandy and
too quiet to judge easily. [The recording analysis](../../docs/hardware/audio-sequence-recording-analysis.md)
finds added harmonics well above the clean digital reference and confirms the
programmed level step. The original left-only image, WAV and restore export
are preserved. Two additional isolated images add **6 dB to the complete
sequence**, differing only in the active I2S slot:

```sh
python3 tools/build_audio_only.py --recording-sequence --recording-speaker left
python3 tools/build_audio_only.py --recording-sequence --recording-speaker right
```

Each still runs the same eight audible segments once, without controls, then
remains silent. Timing, fades, two-tone normalization and source music position
are identical. Both use the **same left-source music waveform**, routed to the
selected slot; this does not compare different source stereo content.
Inactive-slot samples are exact zeros throughout.

| Signal | Lower pass | Higher pass |
| --- | --- | --- |
| Single tones and combined two-tone peak | 1,036 counts / −30.001539 dBFS | 2,067 counts / −24.001924 dBFS |
| Music peak | 1,788 counts / −25.261383 dBFS | 3,568 counts / −19.260237 dBFS |
| Tone Q30 gain numerator | 33,954,698 | 67,748,529 |
| Music Q30 gain numerator | 269,711,752 | 538,145,694 |

The actual Q30 integers in each `sequence.json` are authoritative. Tone targets
are nominal −30 / −24 dBFS; music trims are −12 / −6 dB relative to the existing
fixture. Digital headroom remains over 19 dB. This level increase aids listening;
it is not a distortion correction.

Flash the left-only image, keeping the serial monitor closed:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-recording-sequence-left-plus6db \
  --port /dev/cu.wchusbserial5C932096561
```

Flash the right-only image for the second recording:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-recording-sequence-right-plus6db \
  --port /dev/cu.wchusbserial5C932096561
```

Restore normal radio after recording:

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/esp32s3 \
  --port /dev/cu.wchusbserial5C932096561
```

Use the current radio port if its name changes. No flash was performed by the
agent. The settings-preserving uploader and USB restore behavior are unchanged.

Each new export directory contains `sequence-reference.wav`,
`sequence-timestamps.csv`, `sequence.json` and the complete validated firmware
artifact set. The original recording reference is still the original directory,
not these louder WAVs.

Record one complete run per image. Keep the phone stationary during each run,
with the same orientation and distance from the active speaker for both runs.
Start before reset, allow about 80 seconds after reset, and send the two original
files labelled **digital-left** and **digital-right**. Note which physical
speaker plays, which segments sound sandy, and if both speakers respond in
either run. The phone's stereo microphone channels do not identify the radio's
digital left/right channels.

For the smallest next step, record **right-only first**. Its lower-pass active
samples exactly match the original recording's higher-pass samples (including
music), routed to the other slot. This gives a same-digital-level comparison
without requiring an immediate new left recording. The louder left image is
available if a fresh pair with matching placement is needed.

Check each prepared image with:

```sh
python3 tools/check_audio_only_build.py --recording-sequence --recording-speaker left
python3 tools/check_audio_only_build.py --recording-sequence --recording-speaker right
```

Both builds and full-render checks pass; active PCM is identical after swapping
the left/right slots and each opposite slot is all zeros. Each image is
1,713,616 bytes with 4,839,984 bytes of app-slot headroom and static DIRAM
67,235 / 341,760 bytes (19.67%). The nonfatal initial configure notices described
above remain. Actual speaker selection and playback require the user's run.

At the recording follow-up, `.pio/build/esp32s3/` validates with firmware SHA-256
`119155cf211e22df6c8bdd9099a31bb8bf18d67596185941f2c1a452193c8058`.
It is a different normal-radio export from the hash recorded at initial
preparation; the earlier hash must not be treated as its current identity.
This follow-up exports its normal-radio build check separately and leaves the
current restore directory in place. The restore command above validates the
current complete export before writing it.
