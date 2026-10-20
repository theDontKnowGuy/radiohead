# Audio-only diagnostic implementation and validation

Date: 2026-10-19. Software implementation/build checks passed. **Not flashed by
the agent. The user has now run the hands-free image and reports the same sandy
music symptom.** See the listening record below; no root cause or repair is established.

Start with [installation, restore and the first three listening steps](../../experiments/audio-only/README.md).
Use the [blank result sheet](audio-only-results.csv) to record listening evidence.
The [original task](audio-only-firmware-agent-task.md) remains the requirements record.

## Delivered sources and isolation

- `experiments/audio-only/`: independent ESP-IDF CMake project, direct standard
  I2S output, fixed 55-case suite, deterministic synthesis, encoder classifier,
  robust partial-write helper and the user run sheet.
- `tools/build_audio_only.py`: pinned build/export, separate baseline/evidence
  directories, shared artifact validation, explicit diagnostic manifest label.
- `tools/prepare_audio_only.py`: host decode, DC removal, 30 ms loop-edge fades,
  stereo-preserving common normalization, sine table and rate-specific multitone
  normalization; generated assets stay under ignored `.pio/audio-only-assets/`.
- `tools/check_audio_only.py`, `tools/native_audio_only/check.cpp`: tests of the
  actual portable sample/control/write engine with undefined-behavior sanitizer.
- `tools/check_audio_only_build.py`: exported hashes/factory consistency,
  isolated component/source graph, pinned IDF, board settings, identical
  partition bytes and settings-preserving diagnostic/restore command checks.

No production source, shared build file, pin header, partition CSV, dependency
or vendored library was changed by this task. The pre-existing three-line
`src/media.cpp` volume-range correction remains intact. The pre-existing
investigation/task documents remain intact. No credential or generated binary
is added to tracked source. The default radio build remains the radio.

The minimal image links the IDF PSRAM support so the inherited N16R8 octal-PSRAM
configuration actually takes effect; it excludes Arduino, Wi-Fi/PHY, Bluetooth,
NVS/filesystem clients, display, Audio and Spotify components. The root board
defaults are reused with only Arduino's absent autostart symbol filtered out;
diagnostic overrides turn off console/default logs. No additional toolchain
or managed dependency is installed. Installed package versions checked:
ESP-IDF 5.5.5 (`3.50505`) and Arduino-ESP32 3.3.11 (not linked).

## Build and artifact evidence

| Export | Application bytes | App slot free | Static DIRAM used / free |
| --- | ---: | ---: | ---: |
| `.pio/build/audio-only/` | 1,731,488 | 4,822,112 | 67,343 (19.70%) / 274,417 |
| `.pio/build/audio-only-evidence/` | 1,753,056 | 4,800,544 | 71,723 (20.99%) / 270,037 |
| `.pio/build/audio-only-handsfree/` | 1,729,712 | 4,823,888 | 67,347 (19.71%) / 274,413 |
| `.pio/build/audio-only-case-20-trim-m12/` | 1,729,216 | 4,824,384 | 67,347 (19.71%) / 274,413 |
| `.pio/build/audio-only-radio-check/` | 4,514,512 | 2,039,088 | 260,523 (76.23%) / 81,237 |

Application SHA-256 values for these builds:

```text
audio-only:
9e8656334572d46113599f3f5024c46ef886c7472663500826061d0e09afaf40
audio-only-evidence:
9b90afcd7a3de461b799510865f406fd5cb25d43bd86db26728f15decc81a081
radio-check AND existing esp32s3 restore image:
192ae8b41da8f4e01f151dfb9911b7c7d9e580704526515ba4698ef2c12ff8e9
```

The diagnostics exported a validated bootloader/partition/initial-OTA/app/ELF/
factory set with SHA manifest. Their partition tables match the existing normal
radio export byte-for-byte. The settings-preserving helper's dry runs name the
explicit correct directory, write only 0x0/0x8000/0xe000/0x10000, and do not
include an erase command. NVS, second app slot, filesystem and coredump offsets
are excluded. Full baseline/evidence builds emitted no compiler or Kconfig warnings.

The normal radio was rebuilt through the same native delegate used by
`pio run -e esp32s3`, with a separate export path to satisfy the task's requirement
not to overwrite either existing export:

```sh
python3 tools/build_firmware.py --prepared --output-dir .pio/build/audio-only-radio-check
```

Its application SHA exactly matches the existing `.pio/build/esp32s3/` image.
`.pio/build/esp32s3/` and `.pio/build/before-volume-scale-fix/` were preserved.
No flash helper was executed without `--dry-run`; no hardware was altered.

## Checks performed

```sh
python3 tools/build_audio_only.py
python3 tools/build_audio_only.py --evidence
python3 tools/check_audio_only.py
python3 tools/check_audio_only_build.py
python3 tools/check_firmware_artifacts.py
python3 -m py_compile tools/build_audio_only.py tools/prepare_audio_only.py \
  tools/check_audio_only.py tools/check_audio_only_build.py
git diff --check
```

The host tests exercise:

- All 65,536 signed 16-bit values packed into 32-bit words without undefined
  shifts; sample-exact 16/32 content at 44.1 and 48 kHz.
- Sine frequency/period/energy and target peak, deterministic multitone combined
  peak, active/stopped zeros, inactive-channel exact zero and no clipping for
  every offered case. The music render crosses its actual loop boundary.
- Fixture length/bounds, zero endpoints, residual DC <1 count, −12 dBFS peak and
  no clipped source/output samples. The full 8.613 s stereo source is preserved.
- Radio curve arithmetic, Spotify integer mapping and the sample-exact settled
  direct-dB counterparts on identical source samples.
- Exact flat/bypass equality, each diagnostic EQ center's ±6 dB response at
  both test rates (within 0.015 dB), and rendered offered EQ headroom.
- 12-second transition selection quantized to 128-frame boundaries, bounded
  40 ms ramps, and uninterrupted source phase/time through mute/trim envelopes.
- Cue duration and conservative peak for every ID, bounded controls,
  quadrature selection, bounce, exact hold thresholds, replay and no second
  action on held-button release.
- A driver simulation that accepts a partial prefix while timing out, then
  resumes at the correct suffix; repeated zero progress, non-frame-aligned or
  oversized progress, and non-retryable errors fail explicitly.
- Export layout/hash validation and rejection of NVS writes, corrupt/stale
  artifacts, factory mismatch and oversized applications by the existing checks.

The results CSV has 55 cases and 18 consistent fields per row. It now includes
the first user observations; unreported ratings, supply/capacitor arrangement
and output errors remain unfilled or explicitly uncollected. `python3 tools/check_audio_only.py --write-run-sheet` regenerates
this blank CSV from the actual suite; do not use it over recorded results.

## Encoder-disconnected follow-up

The user reports that the encoder is disconnected. The initial encoder menu
cannot switch cases in that configuration. The separate `--hands-free` image
now runs **01 for 10 s → 02 for 10 s → 03 for 15 s → 04 for 30 s → 02 stopped
indefinitely**, with quiet ID cues. Music has an extra **−12 dB trim**, giving
approximately −24 dBFS output peak. Timers start only on a matching output-task
case/epoch acknowledgment after the cue. A fault prevents automatic advancement;
there is no automatic repeat or higher-level ladder. Power-cycle to replay.
No encoder GPIO is initialized/read in this mode. Power-off is the available
stop control with the encoder absent; keep the serial monitor closed.

`--case N --trim-db D` also builds a separate image holding any bounded case
at a bounded extra attenuation (default −12 dB). Changing that fixed mode
requires a different flash; case 20 at −12 trim was built as the representative
fixed image. Build directories include the case and trim; original encoder,
evidence, normal radio and pre-correction exports remain separate.

Additional commands verified:

```sh
python3 tools/build_audio_only.py --hands-free
python3 tools/build_audio_only.py --case 20
python3 tools/check_audio_only.py
python3 tools/check_audio_only_build.py
```

All four exported variants pass the graph, board, hash, shared partition and
dry-run flash checks. The host suite now also verifies hands-free hold
boundaries, cue/epoch acknowledgment, no advancement on a fault, conservative
music trim and final indefinite stop. Invalid case/trim values and conflicting
control flags are rejected before asset preparation/build. No physical listening
or flash was performed for the follow-up.

## First user listening result — 2026-10-19

The user reports the hands-free sequence played and gives these observations:

| Case | User observation | Tested condition |
| --- | --- | --- |
| 01 active digital silence | Complete silence | 44.1 kHz/16-bit, continuous zero frames, amplifiers powered |
| 02 stopped output | Complete silence | I2S clocks stopped after the ID cue, amplifiers powered |
| 03 sine | Tone clear | 1 kHz, −30 dBFS per channel, bypass |
| 04 music | Same “sandy” addition as regular radio | 44.1 kHz/16-bit, flash PCM, −12 dBFS source plus −12 dB trim, bypass |

These are user listening observations, not measured THD, verified I2S waveforms
or output-task counter evidence. Music severity and static were not assigned
numeric ratings. Current capacitor/supply configuration was not newly specified.
The results CSV retains these limits rather than inventing missing values.

The music symptom is reproducible in the direct-I2S diagnostic with Wi-Fi and
screen inactive and production streaming/decoder/EQ paths absent. Those omitted
activities are not necessary for this reproduction. A clean single sine does
not establish clean complex-signal output; neither quiet silence case rules out
signal-dependent amplifier, speaker, power or digital-output behavior. The
fixture's own audible content and the remaining direct output path are still
candidates. No root cause is selected by this result alone.

Next: listen to `.pio/audio-only-assets/music-reference.wav` on computer speakers
or headphones at a comfortable level. It is an uncompressed WAV wrapper whose
audio bytes exactly match the source PCM embedded in the installed diagnostic:
379,840 stereo 16-bit frames, 44.1 kHz, 8.613 s, source peak −12 dBFS. The WAV
has no additional −12 dB trim; the radio applies that separately. Adjust computer
listening volume normally. No reflash or hardware change is needed for this
source-content check. `tools/prepare_audio_only.py` now emits the reference;
its data was checked against the installed image's fixture SHA-256. This helper
change does not alter the embedded PCM or existing firmware images.

## Source reference clean; matched music-format follow-up — 2026-10-19

The user listened to the computer WAV reference and reports **no “sand.”** The
exact computer output device and matched acoustic SPL were not specified.
This is source-listening evidence, not an electrical measurement or proof that
all sample handling on the ESP32 is correct. The observed sandy addition is
specific to the radio playback path under these listening conditions.

Added the separate `--music-formats` profile: identical base-case-04 music in
**16-bit → 32-bit → 16-bit** Philips stereo slots at 44.1 kHz, 30 seconds per
stage after the cue, then stopped output indefinitely. All stages use the same
source PCM, −12 dB extra gain, phase reset from source frame zero, both channels,
EQ bypass and eight 128-frame DMA descriptors. Cues 1/2/3 identify stages; cue 4
announces the final stopped state. No encoder GPIO is configured/read. Hardware
changes and automatic higher-level tests remain excluded.

The output task now retains an effective case object so zeros, drain lengths,
preload, write sizes and frame alignment all use the current slot width during
transitions. Cue-end envelopes begin at the first PCM frame rather than the
following block boundary, keeping sample envelopes independent of pip count.
No production source or shared build/partition file changed.

Host tests render the actual stereo fixture across two full loops in all three
stages with the same startup ramp. The 16-bit samples are identical, and 32-bit
words recover those samples exactly; all peak at 2,068 counts (about −24 dBFS),
with no digital clipping. Tests also cover epoch/cue acknowledgment, the exact
30-second hold boundaries, fault suppression and final indefinite stop.

```sh
python3 tools/build_audio_only.py --music-formats
python3 tools/check_audio_only.py
python3 tools/check_audio_only_build.py
```

Export: `.pio/build/audio-only-music-formats/`. Application 1,729,664 bytes;
4,823,936 bytes free in the app slot. Static DIRAM 67,347 bytes (19.71%),
274,413 free. No compiler/Kconfig warnings. The manifest identifies control
`music-formats`; hashes/factory consistency, pinned isolated graph, identical
partition bytes and settings-preserving flash dry run pass. The source PCM hash
matches the previously installed hands-free image. Inspection of the compiled
application object's imports finds no encoder GPIO read/config calls. Prior
exports, including the installed hands-free image and both normal restore
exports, were preserved. No flash was performed by the agent.

The new A/B/A acoustic result is pending. A format-dependent difference could
involve word handling, clock/data margins or the amplifier's digital receiver;
it is not proof of a particular component. If equally sandy in both, the next
comparison is music on each isolated channel versus both at unchanged
per-channel level. Do not repeat the already answered source-cleanliness test.

## User-requested fixed volume 18/21 — 2026-10-19

The user needs a higher fixed test level because the encoder is disconnected
and the diagnostic has no Wi-Fi volume control. Added
`--music-formats --radio-index 18`, exported separately to
`.pio/build/audio-only-music-formats-radio18/` so the previous comparison image
remains intact. All three 16/32/16 stages use the exact corrected **radio**
curve at index 18: `radioGain(18)=0.332114206…`, −9.57425094 dB. This replaces
the old −12 dB trim (it is not applied in addition), a 2.42574906 dB increase.
The flash source remains −12 dBFS peak, giving approximately −21.57 dBFS output;
equal UI indices across other content or Spotify do not imply equal loudness.
Cues stay quiet at −36 dBFS. No normal Preferences/settings change or network
control is introduced. Sequence timing, source, channel and format comparison
remain unchanged. New CLI index input is bounded to 0…21 and requires this profile.

Host rendering validates identical 16/32 music samples at that gain, equality
to explicit direct application of `radioGain(18)`, no stacked trim, peak 2,734
counts and no clipping. The gain override persists through all three stages
and clears for final stopped output. Updated host/build-artifact checks pass.

Build: `python3 tools/build_audio_only.py --music-formats --radio-index 18`.
Application 1,729,712 bytes; 4,823,888 bytes free. Static DIRAM 67,363 bytes
(19.71%), 274,397 free. No compiler/Kconfig warnings. The manifest declares
`radio_index: 18`; hashes, shared partitions, source-PCM identity, isolated
component graph and settings-preserving flash plan pass. No agent flash or
new listening acceptance is claimed. Existing diagnostic and restore exports
are preserved. USB device enumeration now shows a WCH USB serial adapter rather
than the previous native ESP32 USB serial port; select the radio's actual
current upload port when installing.

## Format comparison equally sandy; channel-load follow-up — 2026-10-19

After the volume-18 format comparison, the user reports **all three passes
sound the same, with the unwanted sandy addition**. No separate loudness
measurement or numeric severity was supplied. The 44.1 kHz 16/32/16 slot-width
and nominal BCLK change produced no audible improvement. This does not exclude
all digital-output faults or establish a particular hardware cause. Source WAV
cleanliness, quiet zero/stopped cases and the clean 1 kHz sine remain established
user observations; do not ask for these same tests again without new evidence.

Built a separate `--music-channels --radio-index 18` image, defaulting to the
user-requested 18 if no explicit index is supplied. It plays music on left-only,
right-only and both, 30 seconds per pass after cues 1/2/3, then cue 4 and
indefinite stopped output. The left source channel is copied to each active
slot, with exact zero in the inactive slot: no stereo summation/averaging or
per-channel gain change. Rate stays 44.1 kHz, Philips stereo/16-bit in all passes;
radio gain/headroom match the preceding test. This mono-channel fixture is an
explicit diagnostic-only change from the original stereo source, designed to
remove left/right source content as a comparison variable.

Host tests exercise actual rendered music over two loops, including the startup
ramp, and verify exact inactive zero samples and identical active sample streams
across all three stages, no gain increase per active channel, matching peaks and
no clipping. Cue/epoch gating, 30-second boundaries, fault handling and final
stop also pass. Cue pips remain on both slots, so physical speaker observations
must refer to the music after the cue.

The inactive amplifier is not electrically removed or shut down. If both
speakers carry music during a digitally isolated pass, real speaker/load
isolation has not been demonstrated; document channel-selection/mixing behavior
before drawing shared-load conclusions. A loudness increase with two speakers
is expected and is not, by itself, worsening distortion.

Export: `.pio/build/audio-only-music-channels-radio18/`. Application 1,729,760
bytes; 4,823,840 free in the app slot. Static DIRAM 67,363 bytes (19.71%),
274,397 free. Build emits no compiler/Kconfig warnings. Manifest profile/index,
source PCM identity, image hashes/factory consistency, shared partition bytes,
isolated graph, no encoder imports and settings-preserving flash dry run pass.
CLI profile conflicts/invalid indexes are rejected. Other exports, production
source and the user volume correction are preserved. The agent has not flashed
this image; its physical speaker/distortion results remain pending.

## Channel result, one-amplifier capacitor trial and UART evidence — 2026-10-19

The user reports the same “noise”/dirt in each speaker and both during the
channel profile. No improvement from isolating an active speaker was reported.
Do not treat this as physical amplifier disconnection: the inactive amp stayed
powered, grounded and clocked. No new electrical measurement or numeric rating
was provided. The fault does not require two actively driven speakers under
the reported test; it does not select a particular digital or hardware cause.

Asked whether the recommended 470 µF + 100 nF VIN/GND capacitors had already
been added. The user replies that they were added to one speaker/amplifier,
with **no audible difference**. Record this negative trial rather than asking
the same question or prescribing the same trial again. Exact installed part
values/placement were not separately documented beyond the answer's context.
This does not exclude all supply/return issues or prove a capacitor fault.

Next evidence uses the same music/channel/gain run, now with stopped-only status
over the currently observed WCH USB-UART bridge. Original `--evidence` targets
native ESP32 USB serial/JTAG and is unsuitable for reading through that other
connector. Added mutually exclusive `--evidence-uart`; its isolated export is
`.pio/build/audio-only-music-channels-radio18-evidence-uart/`.

Explicit UART mapping: UART0 TX GPIO43 / RX GPIO44, standard ESP32-S3 SoC defaults,
115200 8N1, flow control off. Verified with installed 5.5.5 headers (including
`soc/uart_pins.h`) and official Espressif UART API docs. Only this opt-in build
adds the IDF UART component; it installs no native-USB serial driver. RX interrupts
are disabled, no command parser is added, and no hardware rewiring is requested.
Driver allocation occurs before audio starts. No production pin/configuration or
source change is made.

The control task snapshots counters and request together, and prints only when
I2S is stopped and epochs match, excluding a pending case start. A nonblocking
FIFO writer uses chunks up to 64 bytes, bounded 20 ms drain waits and a 200 ms
transfer deadline (at most one final drain-wait beyond it). It drains UART
before the sole request-writer task can advance playback. On transfer failure,
that control task remains latched in the stopped case. No queued TX ring,
host-reader dependency or status/log call is introduced in sample generation.
Native USB evidence retains its zero-timeout write in the same stopped guard.
All output-task counters remain RAM-only during music.

Build: `python3 tools/build_audio_only.py --music-channels --radio-index 18 --evidence-uart`.
Application 1,760,128 bytes; 4,793,472 bytes free. Static DIRAM 71,827 bytes
(21.02%), 269,933 free. No compiler/Kconfig warnings. Manifest transport/index,
image hashes/factory consistency, identical partition table/source PCM, isolated
graph and flash dry run pass. Compiled application imports UART FIFO/drain APIs
and has no native-USB write, blocking `uart_write_bytes` or encoder calls.
Host tests cover byte-aligned snapshot prefix handling and partial/fatal output
in addition to the existing waveform/packing/control tests. Existing images
remain unchanged. Physical UART delivery and actual runtime counters are pending;
no agent upload or acoustic fix is claimed.

Opening serial can restart the board: open the monitor, leave it open for the
run, and copy one final repeating `audio-only` line after cue 4. Also report
whether the dirt remains. Counter meanings/limitations are in the run sheet;
zero software counters do not establish correct analog output or actual I2S
receiver margins, and no supported driver underrun count is invented.

## UART runtime capture received — 2026-10-19

The user supplied a monitor capture from `/dev/cu.wchusbserial5C932096561` at
115200 8N1, following the reset/DTR/RTS messages. It contains 126 identical
complete final snapshots (and an incomplete final line):

```text
audio-only id=2 trim=0 mute=0 stopped=1 fault=0 generated=4265216 accepted=4265216 partial=0 errors=0 last_error=0 generation_deadlines=0 inferred_gaps=0 peak=2364 clipped=0 driver_underruns=unavailable
```

This verifies physical UART status delivery. Playback reached final stopped
case 02 without a reported fault. Generated/driver-accepted frame totals agree;
no partial writes, output errors, generation deadline misses, inferred long
completed-write gaps or clipping were recorded. Repeated lines represent one
unchanged final snapshot, not separate listening runs. Acceptance is a driver
copy count, not confirmation of samples reaching the receiver. Actual driver
underruns remain unavailable, not measured zero.

Peak 2,364 is consistent with the channel profile's copied left-source peak
7,119 multiplied by radio-index-18 gain 0.332114206 and quantized to signed
16-bit. The earlier stereo-format profile's 2,734 peak included the louder
right source; that difference is expected. Final `trim=0` belongs to stopped
case 02; it does not mean the music ran at volume zero or unity radio gain.
No new build or upload is needed to obtain further identical snapshots.

The user subsequently confirms **"Yes, same dirt"** during this logged run with
the monitor open. The symptom therefore persisted while these recorded counters
remained zero. Prior channel and format listening results remain valid; zero
counters do not establish an acoustic fix, exclude electrical I2S faults or
identify a failed amplifier. Wi-Fi, the display, production decoding/DSP and
reported write/clipping/timing failures are not necessary to reproduce the
symptom in this standalone image.

Proposed next branch is one controlled known-good speaker or I2S amplifier
substitution with the current firmware/gain retained, rather than another
subjective format/channel repeat. Asked which spare components are already
available before choosing the least disruptive comparison. No purchase,
hardware change or new flash has been performed. A speaker substitution also
changes the electrical/acoustic load, so improvement would not by itself prove
the original driver defective; an amplifier substitution similarly changes its
local circuitry and connections.

The user subsequently questions that proposal because both existing pairs
already sound equally dirty. No swap has been performed or reported; the
earlier "Both" answer does not establish extra independently verified good
hardware. The opening of the supplied iPhone diagnostic-music recording was
analyzed numerically; direct model listening is unavailable. The source matches
closely through a fitted acoustic response, but no unique dirt/fault signature
is established. See [the detailed recording analysis](audio-only-iphone-recording-analysis.md).

## Hardware acceptance still pending

- USB restore, encoder mute/selection behavior, and additional profiles. The hands-free sequence has now been heard by the user.
- Real I2S rates/slot packing at the amplifiers and actual DMA continuity.
  Runtime generation-deadline/inferred-gap counters were zero in the capture
  above, but they do not directly measure receiver timing or driver underruns.
- Additional sine/music levels, intended speaker/channel behavior and format dependence.
- Root cause of the reproduced subjective music distortion. Inactive connected hardware remains a limitation.
- Further supply diagnosis if justified; the user already reports no benefit from the extra capacitors on one amplifier.

The optional production Audio/DSP fixture image is deferred until the first
listening evidence warrants it. The baseline includes neither that path nor any
claim that its diagnostic EQ proves the production EQ implementation correct.
