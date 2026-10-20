# Agent task: build an isolated audio-only diagnostic firmware

## Mission and requested deliverable

Implement, build and document a reversible ESP32-S3 diagnostic image to isolate
the **capped/sandy/unstable sound** from this radio. Static is secondary unless
it helps identify the same cause. The user explicitly requests **sound checks
only, no Wi-Fi, no screen**. Stop cycling through inconclusive scope captures.
Deliver a usable standalone test image and a short, ordered listening protocol.
Do not claim to have reproduced or fixed the acoustic fault without user/device
evidence. A clean waveform in a host test is not a clean speaker observation.

This file is a self-contained handoff prepared on 2026-10-19. The diagnostic
firmware is not implemented by this document. Build it; do not automatically
flash, erase storage, publish or change hardware as part of this handoff.

## Repository and instructions

- Workspace: `/Users/avner/Documents/Projects/ESP32Projects/radiohead`.
- Read `AGENTS.md` and `.agents/skills/radiohead-firmware/SKILL.md`; use the skill.
- Read `.agents/skills/radiohead-firmware/references/architecture.md`, but inspect
  actual source: at handoff, `src/main.cpp` still contains setup/loop despite
  architecture prose mentioning `application.cpp`.
- Read `docs/ui/configuration-and-controls-plan.md` for normal encoder behavior;
  document any diagnostic-only control changes explicitly.
- Preserve existing work. At handoff, `src/media.cpp` has a three-line volume
  initialization correction; `docs/hardware/audio-noise-debug-plan.md` is an
  untracked investigation record. Inspect current status/diffs before editing.
- Do not edit vendored `lib/ESP32-audioI2S-master/` or managed dependencies.
- This is deterministic embedded work. No AI service, paid API or credentials
  belong in the firmware. Do not reintroduce removed UI features.

## Hardware: use this mapping, no rewiring

ESP32-S3 devkit, 16 MB QSPI flash, 8 MB octal PSRAM. Two mono MAX98357A-labelled
breakouts share the digital bus, each driving its own speaker. Speakers were
reported as Dayton PC83, 4 ohm. Supply is nominally regulated 5 V. All wiring
was rebuilt with soldered AWG22 leads shorter than 10 cm. Assume basic soldering
is sound. Swapping whole soldered boards is not the first experiment.

| Function | GPIO / connection |
| --- | --- |
| I2S BCLK | 39 |
| I2S DIN: ESP32 data output | 40 |
| I2S LRCLK / WS | 41 |
| MCLK | Unused; MAX98357A does not require it |
| Encoder A / B | 4 / 7 |
| Encoder push switch | 13, active low with pull-up |
| Separate K0 | 6; do not assume available as a diagnostic button |
| TFT backlight | 1; off = LOW in current code |
| TFT CS / DC / reset | 8 / 9 / 10 |
| TFT/touch SCLK / MOSI / MISO | 12 / 11 / 5 |
| Touch CS | 14 |

Reuse `include/app_audio_pins.h` without pulling in production global objects.
Do not include `app_state.h` merely to obtain pins if this instantiates/links
the TFT, Audio object or normal application. GPIO39–41 also occupy external
JTAG; USB remains GPIO19/20.

Board front pin order, terminal block at top: LRC, BCLK, DIN, GAIN, SD, GND,
VIN. Rear printing says MAX98357A, 4–8 ohm/3 W, 2.5–5.5 V, default 9 dB,
(L+R)/2. GAIN appears unconnected on the shown board, a valid 9 dB setting.
Chip marking itself was not readable. Passive parts are fitted; values and
complete circuit have not been established. Do not assume missing capacitors.
User now reports **SD voltages correct**; treat that check as completed, with
exact current numerical values not supplied. No ESP32 shutdown GPIO is wired.
Do not drive or short SD: one board may tie it directly to VIN.

Speaker outputs are bridge-tied. Neither SPK+ nor SPK− is GND. Never connect
scope ground to either, join outputs, or add arbitrary capacitors to outputs.

## Observations already established

1. Both speakers exhibit static plus a capped/sandy or unstable quality in music.
2. The music distortion is the priority. Static is conspicuous around web volume
   changes but also occurs otherwise.
3. On the corrected local-radio image, bass/mid/treble all zero:
   - index 11 barely audible, subjectively about 1/10 loudness;
   - index 16 worst, unstable/sandy, described as sounding underpowered;
   - index 21 reasonably OK but still not full/clean sounding.
   These are listening impressions, not measured speaker power or THD.
4. The distortion persists after at least ten seconds without adjusting controls.
5. Spotify also sounds similarly distorted after settling, with flat EQ and
   approximately matched acoustic loudness on the rebuilt hardware.
6. HTTP mute completely silences the speakers. It sets digital gain to zero
   without intentionally stopping streaming/I2S/Wi-Fi. This does not prove a
   software cause: output switching/current changes too.
7. Replacing the external power source improved neither static nor distortion.
   Local power distribution/decoupling remains possible.
8. X10 probing of BCLK did not audibly change sound. Old X1 probing did disturb
   playback; do not repeat fast-clock measurements at X1.
9. Current scope clock readings: BCLK ~2.82243 MHz, LRCLK ~44.1005 kHz, consistent
   with 64 clocks per frame. A short BCLK/DIN capture showed data changing near
   falling clock edges and settled before rising edges. It does not prove all
   frames, data, or electrical margins valid.
10. Scope displayed large edge spikes, but at the same 100 MSa/s/500 ns setting
    CH2 showed ~0.935 Vpp even with its tip and ground clip **physically touching
    each other on the same amplifier GND pin**. Measurement pickup is substantial.
    Do not diagnose real 6 V GPIO spikes from those screenshots.
11. Instrument: Instrustar ISDS210A, Multi VirAnalyzer 3.13.12.0. User needs clear
    one-step instructions. X10 DC scaling now reads ~5 V correctly. User has
    no probe ground-spring accessory. An ordinary extension spring is unsuitable.
12. Earlier records mention channel mixing and ground-wire improvements before
    this rebuild. Keep as historical leads, not current demonstrated faults.

Complete chronology: `docs/hardware/audio-noise-debug-plan.md`;
older observations: `docs/hardware/audio-static-diagnosis.md` and
`docs/hardware/tft-white-screen-diagnosis.md`.

## Existing audio paths and the volume correction

Current radio path: bundled Audio library, Philips stereo I2S with 32-bit
slots/sample words and source sample rate. Application does not enable its
optional resampler. EQ precedes gain. Spotify: separate decoder and
`components/radiohead_spotify/MAX98357AAudioSink.cpp`, Philips 16-bit stereo at
44.1 kHz; flat EQ bypasses its tone filters. Both use the shared pins above.
No newly proven source defect explains distortion on both paths.

UI indices 0–21 use this table in `src/app_state.cpp`:

```text
0,1,2,3,4,5,6,8,10,12,14,17,20,23,26,30,34,38,42,46,50,55
```

Previously, the local library's default maximum of 21 clamped UI indices 13–21
to full digital gain. `mediaConfigureOutput()` now calls
`audio.setVolumeSteps(volCurve[21])` before restoring volume. Preserve this fix.
The library then applies its own nonlinear curve:

```text
t = volCurve[index] / 55
dB = clamp(-112*t^3 + 172*t^2 - 60, -60, 0)
gain = 10^(dB/20); index 0 is exact zero
```

Settled UI 11/16/21 => approximately -46.87/-20.73/0 dB, respectively.
These are digital gains relative to the SAME source PCM, not perceived loudness
or absolute output dBFS. Spotify uses `floor(volCurve[index]*255/55)/255` linear
gain. Matching UI numbers between sources does not match acoustic level.

The correction was built and the user reports testing it. Normal export:
`.pio/build/esp32s3/`; prior export: `.pio/build/before-volume-scale-fix/`.
Application image 4,514,512 bytes, slot free 2,039,088 bytes; static DIRAM
260,523 bytes (76.23%), 81,237 bytes free. Don't overwrite either export.

## Minimal firmware contract

- No Wi-Fi initialization, reconnect, scans, credentials access or web server.
  No Bluetooth, Spotify session, network-dependent playback or OTA background work.
- No TFT/touch initialization, rendering, SPI traffic or backlight PWM. Release
  any retained GPIO1 hold and hold backlight LOW; deselect TFT/touch with their
  CS pins HIGH. Do not perform screen operations to turn it off.
- Hardware remains physically connected. Thus Wi-Fi/screens inactive is not
  identical to physically removing those components; state that limitation.
- No Preferences/filesystem writes, formatting, calibration or settings migration.
- Direct ESP-IDF standard I2S output for the primary tests. Do not initialize the
  production Audio object or cspot in this minimal path.
- Use bounded static buffers, a stable dedicated output task and robust partial
  write handling. No heap allocation, logging or UI work in the sample loop.
  Preserve stereo frame alignment, handle write errors explicitly, avoid replay
  or loss of partially written frames, and keep enough buffered audio.
- Keep counters in RAM: frames generated/accepted, partial writes, errors,
  deadline misses, and sample peaks/clipping. Distinguish inferred starvation
  from actual driver-observed underruns; don't invent a supported counter.
- Do not print USB Serial from the output task. A connected USB host without a
  serial reader previously made HWCDC writes stall about two seconds. Opening
  USB serial resets this device. Tests must work with the serial monitor closed.
- Boot quietly and conservatively. Fade envelopes and format switches to zero;
  stop/reconfigure the I2S peripheral coherently. Avoid leaving BCLK running
  while LRCLK is stopped. No DC test waveforms to speakers.

## Required checks: implement a small, deterministic test menu/suite

Provide fixed cases with IDs, repeatable timing, audible identification cues
at low level before each case, and a printed run sheet. Avoid a giant sweep
where the user cannot identify which setting sounded bad. Every case must be
replayable/holdable long enough to listen and take a scope capture.

1. **Active digital silence.** Continuous zero samples with I2S clocks running.
   Separate this from stopped-output silence. Establish which quiet condition
   each test actually uses.
2. **Single sine and level ladder.** Start with 1 kHz at a conservative peak
   level such as -30 dBFS. Include -48, -36, -30, -24, -20.73, -18, -12 and
   -6 dBFS with short smooth fades and stable holds. Define dBFS as peak sample
   amplitude and report the definition. Start low; no automatic full-scale tone.
   Provide instant mute. Stop increasing if clipping/buzz/instability begins.
3. **Frequency changes.** At fixed safe peak level, compare 250 Hz, 1 kHz and
   4 kHz. Optional short 100 Hz case at reduced level for bass-related problems;
   no loud low-frequency sweep or sustained near-DC output. Record frequency
   dependence, not just louder/quieter impressions.
4. **Channel isolation.** Same waveform left-only, right-only, then both with
   the SAME per-channel level. Inactive channel is exact zero while its amp
   remains powered. Check sound from the intended speaker, leakage, and whether
   enabling the second channel worsens the first. This tests shared supply/load
   effects without swapping soldered boards. State that it does not electrically
   remove the inactive amplifier.
5. **Matched 16/32-bit I2S.** 44.1 kHz Philips stereo, 16-bit slots versus
   32-bit slots, same waveform, frequency and acoustic target. Use an identical
   16-bit PCM sequence sign-extended/left-aligned correctly into 32-bit words so
   format comparison does not accidentally change content. Use defined integer
   arithmetic (no undefined left shift of negative signed values). Follow with
   48 kHz comparisons if the first pair is inconclusive. Document expected BCLK:
   44.1k x 32 = 1.4112 MHz; x 64 = 2.8224 MHz; 48k => 1.536/3.072 MHz.
6. **Complex waveform and actual music.** Use a deterministic multitone with
   explicitly normalized combined peak and a short local music/speech loop.
   Existing project sources `docs/audio/boot.m4a` and `docs/audio/boot.aac` can
   be inspected for a useful repeatable excerpt. Decode on the host to bounded,
   flash-resident PCM; do not run a decoder/network client in the baseline.
   Preserve stereo channels, check DC offset/clipping, normalize conservatively
   (e.g. peak -12 dBFS), document source, duration and exact gain. Synthetic tones
   alone cannot reproduce a subjective loss of fullness in music. If existing
   material is unsuitable, explain and request a short user-provided clip.
7. **Reproduce the problematic volume behavior.** Apply the exact corrected
   production curve to the SAME local PCM, especially indices 11, 14, 15, 16,
   17, 18 and 21. Hold each, then compare with explicit direct dB control at
   the identical gain. Add the Spotify linear mapping as a separate case.
   UI 21 means gain 1, not necessarily a 0 dBFS source; retain safe source
   headroom. Keep absolute waveform-level tests distinct from curve tests.
8. **Volume transitions versus sustained sound.** Repeated small changes around
   the 16 region; compare abrupt buffer-boundary gain changes with a 20–50 ms
   ramp. Separate transient clicks from distortion continuing ten seconds later.
   Keep oscillator phase continuous across ordinary level changes.
9. **Tone/EQ changes with headroom.** First establish a clean bypass. Then
   low/mid/high boosts/cuts (e.g. +/-6 dB), one at a time, on the identical
   multitone/PCM with compensation sufficient to prevent digital clipping.
   Flat coefficients and a true bypass should be comparable. Use explicitly
   documented filters and validate them; a different diagnostic EQ is not
   proof about the production EQ. Do not run this until simpler cases are usable.

Optional second-stage comparison: the same local fixture through the production
Audio decoder/DSP, still with no Wi-Fi or screen. Keep this separate from the
minimal direct-I2S image so it cannot contaminate the baseline. Do not modify
vendored code; use application/public APIs. Defer this if both direct formats
already reproduce the fault. No firmware can guarantee reproducing an unknown
hardware symptom; clean direct audio is an equally useful result.

## Controls and evidence without a display/network

Prefer existing encoder: turn adjusts a bounded documented level, short click
mutes/unmutes, hold advances/replays a case with a clearly documented diagnostic
behavior. Start each case at a known conservative level. A scripted suite is
also useful, but allow holding/repeating a case and ensure mute remains effective.
Don't depend on K0, a new LED, new wiring or USB serial being open. Document exact
case order/durations and cue scheme so a phone stopwatch can identify each case.

Optional USB command/status mode may be available while output is explicitly
stopped, with strict parsing and bounded output. State the reset-on-open caveat.
Counters must be retrievable without restarting and losing them, or offer a
clearly documented compile-time evidence mode whose runtime impact is disclosed.
Do not make continuous serial telemetry necessary for normal listening tests.

Supply a compact result table: case ID, waveform, frequency, source peak dBFS,
gain/curve index, sample rate/slot width, channel mode, EQ, distortion/static
ratings, supply/capacitor configuration and any output errors. Identify one
short initial run; do not ask the user to run the full matrix before discussing
the first discriminating result.

## Build, isolation and rollback

Pinned toolchain: ESP-IDF 5.5.5, Arduino-ESP32 3.3.11. Normal `pio run -e esp32s3`
delegates to `tools/build_firmware.py`; don't introduce a second unrelated
Arduino toolchain. Inspect root/source CMake, `sdkconfig.defaults`,
`tools/firmware_artifacts.py`, `tools/flash_built_firmware.py` and the TFT-only
experiment for existing mechanisms. An isolated experiment CMake project or
explicit mutually exclusive audio-test build option is appropriate. The normal
default must remain normal radio firmware. Do not accidentally link production
global objects/constructors into the minimal experiment.

Use separate paths such as `experiments/audio-only/`, `.pio/idf-audio-only/`,
`.pio/build/audio-only/`. Avoid unnecessary Spotify downloads/UI asset generation
for a minimal diagnostic build. Preserve existing NVS/OTA/filesystem partition
layout and 16 MB flash settings so restoration retains user settings. Output a
validated artifact set compatible with the existing flash helper if practical.

Deliver exact build and settings-preserving flash/restore commands. The flash
helper accepts `--artifacts-dir`; never flash the normal default by accident.
Warn simply that an audio-only image has no web OTA server, so restoration
requires USB flashing. Do not execute flash/erase/publish in this task.

## Validation and completion

- Meaningful host tests for sample bounds/headroom, zero inactive channel,
  signed PCM packing, 16/32-bit equivalence, deterministic timing and gain math.
  Verify frequency/peaks and absence of unintended clipping in generated PCM.
  For EQ, verify flat/bypass and expected frequency response at test rates.
- Build the diagnostic; inspect RAM/flash and exported manifest. If shared build
  files change, also build `pio run -e esp32s3` and relevant artifact checks.
- `git diff --check`; final review for accidental production behavior changes,
  generated binary commits, secrets, dependency/vendor edits and partition changes.
- Include a short decision table: direct clean -> omitted application/software
  or activity; only one format bad -> format/clock-speed path; one channel alone
  clean but both bad -> shared load/return candidate; one frequency/level bad ->
  signal-dependent amplifier/speaker/supply candidate. None is proof by itself.
- Do not mark the root cause fixed or on-device checks passed from a build.
- Hand back source, image location, validated commands, first three listening
  steps, results sheet and limitations. Hardware acceptance is still pending.

## Capacitor experiment: separate from firmware comparison

User also requested a supply-filtering recommendation. Manufacturer baseline:
0.1 uF + 10 uF close to the IC between VDD and GND. Boards already have passives;
their values are not known. A practical additional trial is 470 uF / 16 V
low-ESR electrolytic across VIN/GND at each amplifier, plus 100 nF ceramic
X7R (16 V or higher, 50 V common) directly across the same pins with short leads.
Keep existing bypass parts. Additional bulk value is an experiment, not an ADI
mandate or an established fix. A 10 uF X7R ceramic, 16 V or higher with suitable
effective capacitance at 5 V, addresses the baseline if onboard 10 uF is absent.

First record audio-only firmware on unchanged hardware, then add capacitance
with power disconnected and compare the identical held case/level. Electrolytic
positive goes to VIN, striped negative to GND. Fit locally, not solely at the
remote supply. No capacitor on BCLK/LRCLK/DIN or speaker terminals. This can
help local supply impedance; it cannot correct a wrong digital word format,
excess gain or a damaged speaker. Don't require this modification to run tests.

Primary references:
- [MAX98357A/B datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf),
  I2S timing, gain/channel selection and page 33 supply bypassing.
- [Espressif ESP32-S3 I2S documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/i2s.html)
  (verify the installed 5.5.5 headers/API while implementing).
- [Tektronix probe grounding](https://www.tek.com/en/documents/whitepaper/abcs-probes-primer).
