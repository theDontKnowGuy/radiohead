# Audio-only diagnostic — installation and listening run sheet

For the new one-run 250 Hz / 1 kHz / two-tone / music phone-recording test,
use [the recording sequence, exact levels, WAV and flash/restore commands](RECORDING-SEQUENCE.md).
The [second recording analysis](../../docs/hardware/audio-sequence-recording-analysis.md)
records all segments sandy and links separate left/right images at +6 dB.
The [right-speaker follow-up](../../docs/hardware/audio-sequence-recording-analysis.md#third-recording-right-speaker)
records shared sandy distortion on both speakers; right-only crackle/static is
explicitly deferred at the user's request.

Built for the existing ESP32-S3 N16R8 and two MAX98357A-labelled amplifiers.
**First user listening: silence cases quiet, sine clear, music still sandy.
Root cause remains unresolved. Nothing was flashed by the agent.**
This image isolates the capped/sandy/unstable music symptom; static is recorded
separately. It cannot establish an acoustic repair from a successful build.

## Current result: dirt persists with no recorded output failure

The user's UART capture contains 126 identical complete final snapshots:

```text
audio-only id=2 trim=0 mute=0 stopped=1 fault=0 generated=4265216 accepted=4265216 partial=0 errors=0 last_error=0 generation_deadlines=0 inferred_gaps=0 peak=2364 clipped=0 driver_underruns=unavailable
```

The run finished in stopped case 02 without a reported fault. All generated
frames were accepted by the driver; no partial writes, write errors, generation
deadline misses, inferred long write gaps or clipping were recorded. These are
repeated snapshots of one run, not 126 independent tests. Peak 2,364 is expected:
this profile copies the left source (peak 7,119) at radio gain 0.3321142. The
final `trim=0` describes the stopped case and does not change the music's 18/21
gain. Hardware driver underruns remain unmeasured. The user confirms **the same
dirt was audible during this logged run**. No acoustic fix is established; do
not repeat the flash merely to obtain more copies of these counters.

The commands below reproduce the completed counter capture if needed.

The user reports the same dirt/noise with each isolated speaker and both.
The recommended extra supply capacitors were added at one speaker/amplifier
with no audible improvement. These results do not identify a failed component.
Output-task timing/write evidence has now been collected as recorded above.

The current WCH connection is a USB-UART bridge, whereas `--evidence` uses the
ESP32's different native USB serial/JTAG interface. Use the new UART variant:

```sh
python3 tools/build_audio_only.py --music-channels --radio-index 18 --evidence-uart
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-music-channels-radio18-evidence-uart \
  --port /dev/cu.wchusbserial5C932096561
pio device monitor --port /dev/cu.wchusbserial5C932096561 --baud 115200 --rts 0 --dtr 0
```

Use the current radio port if it changes. Open the monitor and **leave it open
until the sequence finishes** (about 100 seconds). Opening serial may reset the
board; let the fresh run complete. If necessary start a fresh run with RESET
while the monitor stays open. The same left/right/both music plays at radio
18/21, with cues 1/2/3 and final stop cue 4. Report whether the music remains
dirty and copy one complete `audio-only ...` line after the final cue. The line
repeats once per second while stopped, so no timing trick or encoder is required.

There are **no status writes while I2S is running**. A matching request/output
epoch guard prevents printing during a pending case start. UART0 sends bounded
64-byte FIFO chunks, waits for transmission completion before permitting a new
case, and fails into the stopped case if transmission fails. No TX ring buffer,
flow control, receive parser or Arduino `Serial` is used; RX interrupts are off.
The lower priority control task does all status work; the output task only
updates RAM counters. This opt-in build installs the UART driver and uses the
board's standard ESP32-S3 UART0 pins explicitly: **TX GPIO43, RX GPIO44**, 115200,
8N1, no RTS/CTS GPIOs. This does not require rewiring the existing USB-UART
bridge or change I2S/display pins. The baseline images do not install this driver.

The implementation was checked against installed IDF 5.5.5 UART headers/SoC pin
definitions and the [official UART API documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/uart.html).
The user's capture verifies UART status delivery and supplies the runtime
counters above; the user also confirms unchanged dirt during that run.

The useful fields are `errors`, `partial`, `generation_deadlines`,
`inferred_gaps`, `clipped`, and `fault`, with generated/accepted frame totals.
A generation deadline miss alone is not a proven audible dropout (there is
DMA buffering); a long completed-write gap is inferred starvation, not a
hardware-observed underrun counter. `driver_underruns=unavailable` is intentional.
Zero counters would mean no such failure was observed, not that all digital or
hardware faults have been excluded. UART/monitor activity differs from the
baseline; the user confirms that the dirt persisted with the monitor open too.

The user questions swapping because the two existing speaker/amplifier pairs
already sound equally dirty. Substitution would only add a clean reference if
the replacement were independently known good; extra verified hardware is not
established and no swap result has been reported. The supplied iPhone recording
was analyzed numerically, with no direct model listening available. It matches
the source through an acoustic response closely but establishes no unique
failure mechanism. See the [recording analysis](../../docs/hardware/audio-only-iphone-recording-analysis.md).
No new flash, purchase or hardware change is requested by that analysis.

Subsequent next-step recommendation: with all power including USB removed,
completely disconnect one amplifier module from the radio, preserving the other
module's wiring, then run this same image and judge music after cue 3. Earlier
channel isolation left both modules powered/clocked/connected; this comparison
tests whether the second module's physical presence matters. Do not merely
disconnect its speaker or VIN, and do not change the remaining module's SD/GAIN.
No result from this proposed physical isolation has been reported. See the
[test interpretation](../../docs/hardware/audio-noise-debug-plan.md#next-discriminating-test-one-amplifier-physically-connected--2026-10-19).

## Completed channel comparison: left-only / right-only / both at radio 18/21

The user reports **the same sandy sound in all three 16/32/16 music passes**.
Changing slot width and nominal BCLK at 44.1 kHz did not improve it. The next
comparison changes channel load while holding the per-channel signal constant:

```sh
python3 tools/build_audio_only.py --music-channels --radio-index 18
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-music-channels-radio18 \
  --port /dev/cu.wchusbserial5C932096561
```

Use the radio's current upload port; that WCH USB serial device was present at
the latest enumeration. Keep the serial monitor closed and hardware unchanged.
There is no encoder, Wi-Fi or screen dependency. After the quiet boot:

| Cue | Music data | Hold |
| --- | --- | --- |
| 1 high pip | Left channel active; right exact zero | 30 s |
| 2 high pips | Right channel active; left exact zero | 30 s |
| 3 high pips | Both channels active, same waveform and same per-channel gain | 30 s |
| 4 high pips | Finished, I2S stopped after cue | Indefinite |

Judge **music after the pips**. The pips themselves are sent to both digital
channels. Note which physical speaker carries music in each pass, and whether
sandy distortion is already present with either alone or appears/worsens when
both play. Digital left/right may not match the physical side; report what
actually sounds. If both speakers play music in an isolated pass, report that
first: amplifier channel selection/mixing may prevent real load isolation.
Do not infer a shared-supply fault from the naturally louder both-speaker pass.

For this profile, the **left source channel is used as the same mono waveform**
on every active slot. It is copied, not summed or averaged; the first two passes
have exactly the same active samples and the both pass duplicates those samples.
There is no gain compensation when enabling both. This removes source stereo
content as a variable within the channel comparison. Gain is corrected radio
index 18/21, same as the preceding format test; source peak/headroom is retained.
The inactive amplifier remains powered, connected to ground, and clocked; zero
PCM is not physical removal or shutdown of that amplifier.

Sample rate/format stay at 44.1 kHz, Philips stereo, 16-bit slots (nominal BCLK
1.4112 MHz) in all passes. Timing is the same as the previous comparison:
30-second steady holds after cues, phase/source reset per stage, smooth fade
and zero drain at changes, about 100 seconds overall, then stopped indefinitely.
Power-cycle to replay; power-off is the available early stop control. The new
profile defaults to radio index 18 if `--radio-index` is omitted; the explicit
command above records the desired level clearly.

If each isolated speaker is clean but both worsen, shared load/return or
speaker interaction becomes a candidate. If each is sandy alone too, the
fault does not require two actively driven speakers under this test; direct
output and individual signal-dependent hardware behavior remain open. Neither
outcome alone proves a component faulty. The user reports the same dirt in each
speaker alone and both; the subsequent counter capture is recorded above.

## Previous comparison: 16/32/16-bit slots at radio 18/21 — equally sandy

The computer WAV reference sounds clean according to the user; the original
hands-free radio music remains sandy. Use this separate, encoder-free A/B/A
image to compare format/clock dependence with identical music and gain:

```sh
python3 tools/build_audio_only.py --music-formats --radio-index 18
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-music-formats-radio18 --port /dev/cu.usbmodem1201
```

Reconnect the ESP32 first and use its actual USB port. `/dev/cu.usbmodem1201`
was the last detected ESP32 port; it may change or disappear when unplugged.
Identify the `USB JTAG/serial debug unit` with
`~/.platformio/penv/bin/python -m serial.tools.list_ports -v` if necessary.
The usual `pio run -t upload` still uploads the normal radio; use the explicit
artifact directory above. No agent flash has been performed.

Keep the serial monitor closed and hardware unchanged. This image does not
configure/read encoder pins. After two boot seconds it runs once:

| Cue | Stage | Listening time | Nominal BCLK |
| --- | --- | --- | --- |
| 1 high pip | A: music, 44.1 kHz, Philips stereo, 16-bit slots | 30 s | 1.4112 MHz |
| 2 high pips | B: same music/gain, 32-bit slots | 30 s | 2.8224 MHz |
| 3 high pips | A again: 16-bit slots | 30 s | 1.4112 MHz |
| 4 high pips | Finished; clocks stopped after cue | Indefinite | Stopped |

These pips identify **stages**, rather than normal menu case IDs. All three
music stages use base case 04, reset from source frame zero, source peak −12
dBFS and the **exact corrected radio gain at index 18/21**: 0.3321142
(−9.57425 dB), with no extra −12 dB trim. Output peak is about −21.57 dBFS.
This is 2.42575 dB above the earlier diagnostic level; it does not guarantee
the same acoustic loudness as a different station/source at index 18. Both channels, source bytes, rate, EQ bypass, gain and DMA
frame count stay the same. Only data/slot word width and its resulting bit
clock change. No gain matching by ear is introduced; report any loudness
change as well as sandy texture. An identical 16-bit sample sequence is packed
into 32-bit words by defined multiplication, preserving its amplitude.

`--radio-index 0…21` is available with `--music-formats` or `--music-channels` and is recorded
in the exported manifest. The directory suffix includes the chosen index.
Without that option, the original `audio-only-music-formats` build still uses
its prior −12 dB extra trim. The radio-index build replaces that trim with the
production curve; it does not stack two attenuation stages. Cue pips stay at
−36 dBFS. The saved normal-radio volume setting is not changed.

Each hold begins after the matching cue acknowledgment. Cue-end gain ramps
start at the first PCM frame, so different pip durations do not shift the
source envelope. Fade to zero and drain in the **current format** before
stopping both clocks and configuring the next width. The sequence lasts about
100 seconds including cues and fades, then stays stopped. Power-cycle to replay;
power-off is the stop control with the encoder absent.

Report **A sandy/clean → B sandy/clean → A again sandy/clean**. A repeatable
format difference points toward the format/clock/interface path, including its
receiver and electrical behavior; it does not by itself prove a software bug.
Both equally sandy makes this format distinction less useful, and the next
candidate comparison is isolated-channel music versus both channels at the
same per-channel level. Make no capacitor or wiring change during this A/B/A.

## No encoder connected: use the hands-free image

The original menu image requires the encoder; it cannot change cases with the
encoder disconnected. The separate hands-free image **does not configure or
read the encoder pins**, and needs no screen, network or serial monitor.

```sh
python3 tools/build_audio_only.py --hands-free
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-handsfree --port /dev/cu.usbmodem1201
```

Keep the serial monitor closed. After two quiet boot seconds, this short
sequence runs once automatically. High pips announce the case number:

| Cue | Case | Steady listening time | Actual output peak / action |
| --- | --- | --- | --- |
| 1 pip | 01 active digital silence | 10 s | Zero samples, clocks running |
| 2 pips | 02 stopped silence | 10 s | Clocks stop after the cue |
| 3 pips | 03 1 kHz sine | 15 s | −30 dBFS per channel |
| 4 pips | 04 stereo music | 30 s | −12 dBFS source plus **−12 dB extra trim**, peak ≈−24 dBFS |
| 2 pips | 02 stopped silence | Indefinite | Sequence finished; clocks stop |

Each timer starts after the output task acknowledges that its cue has ended.
Allow roughly **75 seconds from boot to the final stopped state**, including
cues and fades. There is no automatic repeat or level increase. Power-cycle to
replay. With no encoder there is no software mute control: turn off the device's
power if sound is uncomfortable.
Record case 04's CSV trim as **−12**, not the encoder image's default 0.

For a longer held comparison, build one fixed case, then flash its explicit
export directory. Example: hold 44.1 kHz/16-bit case 20 at its −30 dBFS sine
with a further −12 dB trim (actual −42 dBFS):

```sh
python3 tools/build_audio_only.py --case 20 --trim-db -12
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-case-20-trim-m12 --port /dev/cu.usbmodem1201
```

`--case` accepts 1…55; `--trim-db` accepts −60…0 and defaults to −12. Each
case/trim has its own directory (`trim-m0` means no extra attenuation).
A fixed image announces its case and holds it until power-off; changing to a
different case requires building/flashing that case's image. No encoder input
is read. Keep louder levels, transitions and EQ for after the first results.
Both new modes support the optional `--evidence` suffix described below; the
hands-free sequence reaches stopped case 02 for snapshots without any controls.
The normal radio restore command below applies to all these variants.

## Encoder image installation and common restore (USB; settings preserved)

Run these commands from the repository root. Replace the port if necessary.
Keep the serial monitor closed for the normal listening image. No rewiring is needed.

```sh
python3 tools/build_audio_only.py
python3 tools/check_audio_only.py
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only --port /dev/cu.usbmodem1201
```

The complete validated export is `.pio/build/audio-only/`, including
`firmware.bin`, ELF, bootloader, partition table, OTA selection data, merged
factory image, `artifacts.json` and `fixture.json`. The build uses the installed
pinned ESP-IDF **5.5.5** compiler/packages, checks the pinned Arduino **3.3.11**
package, and deliberately does not link Arduino. It does not download Spotify,
generate UI assets, or modify `.pio/build/esp32s3/` or
`.pio/build/before-volume-scale-fix/`. `afconvert` on macOS decodes the source
clip during asset preparation. If pinned packages are missing, first run
`pio pkg install -e esp32s3`; there is no alternate Arduino toolchain.

**There is no web OTA server in this image; restore using USB:**

```sh
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/esp32s3 --port /dev/cu.usbmodem1201
```

Both commands validate file hashes, factory-image consistency and the shared
partition layout before writing. They write bootloader, partition table,
initial OTA selection and app0 only; NVS, app1, filesystem and coredump regions
are not erased or written. Resetting OTA selection makes app0 boot even if the
radio last ran app1. Use the helper rather than writing the merged factory file when retaining settings.
No erase command is needed. The original radio export is
the restore image; the pre-volume-correction image is also left untouched.
Use `--dry-run` on either flash command to inspect it without contacting the board.

## First three listening steps with an encoder (about two minutes)

Use the unchanged hardware, present supply and capacitor arrangement first.
No scope captures are required. Keep USB serial closed. Write down **D** for
music distortion and **S** for static: 0 absent, 1 faint, 2 clear, 3 severe.
A short encoder press mutes immediately after debounce; stop increasing level
if buzz, harshness or instability begins. In the encoder image, selected cases never advance automatically.

1. After boot, wait for the one high pip (**01**), then listen to **case 01**
   active digital silence for 10 seconds. Hold the encoder **2 seconds and
   release** to advance; after two high pips (**02**) and the following quiet
   tail, listen for 10 seconds. **02** has both I2S clocks stopped. Compare
   these two quiet conditions; the amplifier remains powered in both.
2. Hold 2 seconds and release once more: three high pips announce **03**,
   a steady 1 kHz sine at **−30 dBFS peak per channel**, 44.1 kHz/16-bit.
   Listen for 15 seconds without touching anything. Record smooth/buzzy,
   stable/unstable, S and D. Press briefly to mute if uncomfortable; a second
   short press restores the chosen level. Turning down adds attenuation.
3. Advance once to **04**, the local stereo music loop, **−12 dBFS source
   peak at unity gain**, EQ bypass. Listen for at least three 8.61-second loops
   (about 30 seconds), including more than ten seconds after touching controls.
   Turn down if needed and record how many 1 dB steps. Report **01/02 quiet
   difference, 03 tone quality and 04 music D/S** before running the larger matrix.

A clean sine with sandy music is useful evidence. Clean direct music is also
useful; neither result alone identifies a component or proves production software faulty.

## First result and the next source check

The user has run the hands-free image: 01/02 completely silent, 03 clear tone,
04 music with the same sandy addition as regular radio. That reproduces the
symptom without the normal network/display/decoder/DSP activity; it does not
identify a hardware fault. The [listening record](../../docs/hardware/audio-only-validation.md)
contains the conditions and remaining uncertainties.

Before another flash or component change, play the same decoded source on
computer/headphones: `.pio/audio-only-assets/music-reference.wav`.
`python3 tools/prepare_audio_only.py` regenerates this playable reference alongside
the flash PCM. Its WAV audio data is byte-identical to the embedded source, with
no extra radio −12 dB trim; use a comfortable computer volume. The user reports no sandy texture in the computer reference; proceed to the
matched music-format comparison above. Hash/PCM checks do not establish how it
sounds. Preserve any listening notes in the CSV before regenerating a blank sheet.

## Diagnostic controls (intentionally different from the radio)

| Encoder action | Diagnostic behavior |
| --- | --- |
| Turn, button released | One step per four valid quadrature edges; 1 dB extra attenuation, bounded −60…0 dB. Cannot raise the listed case peak. Turning while muted keeps it muted. |
| Press | Mutes after 20 ms debounce, with a 5 ms fade plus queued DMA latency (about 23 ms maximum in healthy playback). |
| Short release, held <1.5 s | Toggle mute; restore prior trim on unmute. Cancels an ID cue if pressed during it. |
| Hold 1.5…<4 s, release | Next case, wrapping 55→01; reset trim to 0 dB; play ID, then hold that case. A 2 s hold is convenient. |
| Hold ≥4 s, release | Replay current case from the start, including its ID; reset trim to 0 dB. No sleep/power action. |
| Hold and turn | Select previous/next case, bounded 01…55; output stays muted during selection. Release starts the selected case with ID and trim 0. Time held does not also trigger Next/Replay. |

Case selection/replay unmutes intentionally. Each case starts at its fixed
listed level. The higher ladder steps are deliberate separate selections;
there is no automatic full-scale tone. Do not advance to louder cases after a
bad result. K0 is unused. The encoder uses GPIO4/7/13 and production quadrature
direction, with diagnostic detent aggregation. Physical detents/direction and
mute latency still need device verification.

IDs use **low 500 Hz pips for tens, then high 1.5 kHz pips for ones**; omit
zero digits. Example 21 = two low pips, pause, one high pip. Each pip is
100 ms on/100 ms off, with 5 ms edges and a −36 dBFS peak. There is a 300 ms
lead, a 300 ms digit separator and a 500 ms tail. Cue duration is
`1.1 + 0.2 × (tens + ones)` seconds, plus initial DMA latency. Case time zero
starts after the tail. Boot adds two quiet seconds. 01/02 cues use clocks too;
judge their steady quiet state only after the cue. In the encoder/fixed-case images, cases hold indefinitely.
Replay resets oscillator phase and music position; ordinary trim/mute changes
keep phase and source time running. No timed automatic sweep is hidden in the menu.

## Ordered suite and comparisons

The printable [55-row results sheet](../../docs/hardware/audio-only-results.csv)
is generated from the actual C++ case table. Source peak means **maximum
absolute PCM amplitude / 32767**, expressed as `20 log10`, before the listed
gain and EQ. It is not speaker power, SPL, RMS dBFS, or UI volume. L/R have
the same per-channel peak when both carry the same synthetic signal. Music's
channels stay independent. Default extra encoder attenuation is always 0 dB.

| IDs | Signal and fixed condition | Listening purpose |
| --- | --- | --- |
| 01 / 02 | Continuous digital zeros / stopped I2S | Active vs stopped silence; SD untouched |
| 03 / 04 | 1 kHz −30 / stereo music −12 dBFS | Short initial run above |
| 05–12 | 1 kHz: −48, −36, −30, −24, −20.73, −18, −12, −6 dBFS | Manual level ladder; stop on first bad step |
| 13–15 | 250 Hz / 1 kHz / 4 kHz, all −30 dBFS | Frequency dependence at fixed digital peak |
| 16 | 100 Hz −36 dBFS, 2 s with 30 ms edges then 8 s silence, repeating | Optional quiet bass test; skip until simpler cases usable |
| 17 / 18 / 19 | Left-only / right-only / both; 1 kHz −30 dBFS | Inactive channel exact zero; same level on active channels |
| 20 / 21 | 44.1 kHz, 16 / 32-bit slots; 1 kHz −30 | Identical signed 16-bit PCM content, 32-bit left aligned |
| 22 / 23 | 48 kHz, 16 / 32-bit slots; 1 kHz −30 | Follow-up only if 20/21 inconclusive |
| 24 | Equal-weight 250 Hz + 1 kHz + 4 kHz, combined peak normalized to −18 dBFS | Repeatable complex load, bypass |
| 25–31 | Music with corrected radio indices 11,14,15,16,17,18,21 | Hold each troublesome region for ≥15 s |
| 32–38 | Same music, explicit direct dB equal to each 25–31 gain | Sample-exact settled gain comparison |
| 39–45 | Same music, Spotify linear indices 11,14,15,16,17,18,21 | Separate mapping; same numbers are much louder than radio |
| 46 / 47 | Music; indices 15→16→17→16, each 12 s, repeat at 48 s | Abrupt buffer-boundary steps / 40 ms ramps; source time continuous |
| 48 / 49 | Multitone −18 source, −12 compensation; true bypass / flat coefficients | Establish EQ reference before boosts |
| 50 / 51 | Low +6 / −6 dB, with same −12 compensation | One diagnostic peaking filter at 250 Hz |
| 52 / 53 | Mid +6 / −6 dB, same compensation | 1 kHz |
| 54 / 55 | High +6 / −6 dB, same compensation | 4 kHz |

For 17–19 note which actual speaker sounds, leakage into the other speaker,
and whether enabling both worsens either. An inactive amplifier remains
powered and electrically connected; zero PCM does not remove its supply load.
If boards mix (L+R)/2, both speakers may reproduce either isolated channel.
Record that instead of assuming left/right selection or rewiring SD.

Philips stereo sends one-bit-delayed data with no MCLK output. Expected BCLK:
**20: 1.4112 MHz; 21: 2.8224 MHz; 22: 1.536 MHz; 23: 3.072 MHz**.
32-bit samples are `int32_t(sample16) * 65536`, with no undefined negative
signed shift. Thus the format pair does not change digital amplitude/content;
check acoustic matching by ear rather than changing gain to conceal a difference.

The corrected radio table ends at 55. Settled index 11/14/15/16/17/18/21 gains
are −46.87493/−33.39475/−27.00225/−20.72900/−14.83350/−9.57425/0 dB,
using `clamp(-112*t³ + 172*t² − 60, −60, 0)`, `t=volCurve[index]/55`.
Index 0 is exact zero. Spotify uses integer `floor(volCurve[index]*255/55)/255`.
UI 21 is unity gain on the **−12 dBFS music fixture**, never a full-scale tone.
Cases 32–38 expose exactly the same gain via direct dB; equality is an expected
control, not an independent test of the production decoder/DSP implementation.

Transition steps in 46/47 occur on the first 128-frame buffer boundary at/after
each 12-second mark (≤2.91 ms quantization). Listen for a brief step click and
separately for distortion still present during seconds 10–12 of each hold.
Replay for a repeatable A/B/A comparison. Case selection has a 30 ms fade to
zero, drains more than the DMA queue in zeros, disables both clocks, then
recreates and preloads the new format with zeros before its cue.

EQ uses a single RBJ peaking biquad, Q=1/√2, transposed direct form II, with
separate L/R history. Flat is an exact identity through the filter path;
bypass skips it. Every EQ/reference case has 12 dB common compensation before
the filter (linear scaling commutes for this filter); this exceeds the 6 dB
boost and gives transient headroom. Host checks validate center response at
44.1/48 kHz and render all offered EQ cases without clipping. This differs
from production shelves/tone processing and cannot prove production EQ correct.
Run 48–55 only after bypass audio is usable.

## RAM evidence without disturbing the baseline

The baseline never installs a USB serial driver or prints from the output task.
Counters stay in RAM until reset: frames generated/accepted, partial writes,
write/configuration errors and last error, generation deadline misses, long
write-service gaps, peak sample and clipped samples. Peak includes cues; totals
are cumulative across cases. Generated frames include queued zeros and cues.
Accepted means copied into driver DMA, not confirmed acoustically played.
`generation_deadlines` means a block took longer than its audio duration to
produce. `inferred_gaps` means >one entire DMA queue duration between completed
writes; it is **inferred starvation**, not a measured underrun counter. This
IDF TX API has no directly observed underrun count; status says `unavailable`.
An unrecoverable error stops output and latches a fault until another case/replay.

For counters, use the separate, opt-in evidence image:

```sh
python3 tools/build_audio_only.py --evidence
~/.platformio/penv/bin/python tools/flash_built_firmware.py \
  --artifacts-dir .pio/build/audio-only-evidence --port /dev/cu.usbmodem1201
```

**Opening USB serial resets this radio.** Open the reader before repeating the
chosen test and keep it open throughout that evidence run. For example:

```sh
pio device monitor --port /dev/cu.usbmodem1201 --baud 115200
```

Select 02 for the first snapshot, select the desired held case, then return to
02 using hold+turn; it emits cumulative snapshots once per second without a
restart. Compare totals before/after. Do not open the port after an unrecorded
listening run expecting old RAM counters to survive. Closing the port or reset
may also affect the board; capture the snapshot first. Evidence mode adds a
USB ISR/ring buffer and bounded zero-timeout status writes from the lower
priority control task, **only while output is stopped**. It may drop status if
there is no reader. It never prints during playback. Its USB activity differs
from the baseline, so repeat any acoustic conclusion with the baseline image
and monitor closed. No serial command parser or background telemetry is required.

## Fixture, isolation and limits

The fixture uses the entire `docs/audio/boot.m4a` passage: 379,840 stereo frames,
8.613151927 s at 44.1 kHz. Host preparation removes DC −26.33977/−26.34840
16-bit counts, fades both loop edges over 30 ms, and applies common gain
**1.9095204151913163 (+5.61848611784256 dB)**. Quantized peak is 8,231 counts
(−11.99968 dBFS), residual DC is −0.33162/−0.34417 count, and clipped sample
count is zero. No in-device decoder, resampler or network client runs.
`fixture.json` includes the source/PCM SHA-256 and exact processing metadata.
The multitone's pre-normalization combined peaks are 0.937420446862 at 44.1 kHz
and 0.935270241401 at 48 kHz; each rate uses its own normalization.

The independent CMake graph links only this experiment and required IDF support.
Audio uses GPIO39/40/41 from `app_audio_pins.h`; no `app_state.h` or production
global constructors. It releases GPIO1 retention and holds backlight LOW,
deselects TFT CS8 and touch CS14, and makes no SPI/reset/render/PWM operations.
It never initializes Wi-Fi/BT, reads credentials, mounts storage, writes
Preferences, migrates settings or runs OTA. Screen and radios being inactive
is not the same experiment as physically disconnecting the screen/amplifiers.
I2S creation allocates driver resources only at case changes; sample production
uses static buffers and a static dedicated core-1 task. Eight 128-frame DMA
buffers provide ~23.2 ms at 44.1 kHz / 21.3 ms at 48 kHz. USB, encoder and
format setup work do not occur inside sample generation. Driver timeouts retry
only the unaccepted suffix, including when a timeout accepted a partial prefix.
Unaligned/oversized progress or repeated zero progress fails closed.

| Result | Next candidate; not proof |
| --- | --- |
| Direct music clean | Omitted application processing/activity or source differences; consider separate production-path fixture later |
| Only one slot format bad | Format, clock-speed or digital-interface path |
| Each isolated channel clean, both worse | Shared supply/load/return path |
| Specific frequency or level bad | Signal-dependent amplifier/speaker/supply behavior |
| Active zero noisy, stopped quiet | Clock/data activity or amplifier operating-state sensitivity |

Do not run a second production Audio/DSP image until this baseline's first
results are discussed. That optional comparison is deferred; no vendor patch
was made. For physical limits, source code and host tests cannot verify DMA
continuity, actual clock edges, speaker fidelity, encoder detents or supply behavior.

## Optional separate capacitor A/B/A experiment

First record unchanged hardware. With power disconnected, an additional trial
is **470 µF / 16 V low-ESR electrolytic plus 100 nF X7R ceramic (≥16 V)** locally
across **VIN/GND at each amplifier**, retaining existing bypass parts. Electrolytic
positive to VIN, striped negative to GND; keep leads short. Repeat the identical
held case and trim. Additional bulk is an experiment, not a demonstrated fix.
The [manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
(page 33) specifies nearby 0.1 µF + 10 µF supply bypassing; onboard values remain
unknown. If the 10 µF is absent, use a suitable 10 µF X7R (≥16 V; allow for
DC-bias capacitance loss). Do not attach capacitors to I2S or speaker outputs,
change SD, join speaker outputs, or connect scope ground to a speaker terminal.
Speaker outputs are bridge-tied; neither terminal is ground.

Implementation uses the installed 5.5.5 headers plus the
[IDF standard-I2S documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/i2s.html).
See [validation evidence](../../docs/hardware/audio-only-validation.md) for the
build report and checks actually performed.
