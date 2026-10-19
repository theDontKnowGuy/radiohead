# Playback static and channel separation investigation

## User observations, 2026-10-16

- Both internet radio and Spotify have harsh static that grows with volume;
  low-volume playback has little audible noise. Pausing Spotify silences it.
- Regulated supply set to 5 V; user reports no current limiting and about
  180 mA on its display. This is not a measurement of transient amplifier current.
- Speakers are Dayton PC83, reported 4 ohm. Wiring is soldered; swapping whole
  amplifier boards is not a practical next test for the user.
- Physical I2S wiring confirmed: BCLK GPIO39, LRC GPIO41, DIN GPIO40.
- A Spotify left/right test plays both announcements from both speakers.
  The exact test separates correctly through headphones and previously worked
  on the radio. Do not dismiss this as ordinary centred musical content.
- SD readings reported as 0.9 V and 3.8 V, nominally right and left selection.
  Whether measured against each amplifier's local GND remains unconfirmed.
- Second amplifier was initially disconnected entirely. The first became clean
  after adding a short thick ground wire from the hub to the amplifier.
- Reconnecting the second amplifier with its own short thick ground wire left
  the first clean but the second noisy. Probe contact at the second amplifier's
  GND reduces its noise without removing it completely.

## Scope observations and limitations

Instrument is Instrustar ISDS210A with Multi VirAnalyzer 3.13.12.0. The earlier
TFT investigation records an unresolved X10 scaling discrepancy. These audio
captures used X1 on probe/software. The user needs one clear step at a time.

The probe ground clip was actually at the radio ground hub throughout the
initial measurements, not at amplifier GND as originally requested:

- VIN-to-hub, DC, 5 V/div, 1 ms: displayed min 4.504 V, max 5.207 V.
- VIN-to-hub, AC, 200 mV/div, 1 ms: playing 668 mV peak-to-peak;
  paused 414 mV peak-to-peak, with broadly similar spiky traces.
- Moving only the probe tip to amplifier GND made the first amplifier's music
  loud and clear; removing it restored noise. That capture displayed 764 mV
  peak-to-peak despite clean sound.

These are displayed extrema, not established local supply ripple. VIN-to-hub
includes ground-path voltage differences and measurement pickup. Probe tip on
amplifier GND with clip at the hub is not a zero-input noise-floor measurement.
Probe capacitance or contact pressure can alter behaviour; the tip is not a
direct grounding jumper. The added-wire result implicates the ground path but
does not identify a particular bad joint or prove the cause of channel mixing.
Never attach a scope ground clip to either bridge-tied speaker output.

## Firmware inspection and prepared correction

Both inspected output paths configure Philips stereo I2S. No application
force-mono call was found; the local library defaults forceMono to false.
Spotify processes the interleaved samples separately and uses 44.1 kHz,
16-bit stereo output. No deliberate downmix was found in the inspected sink.

The Spotify sink still hard-coded GPIO15/17/16 while local playback used
39/41/40. Both now consume `include/app_audio_pins.h`. This is a confirmed local
source defect, but it does not by itself explain recognizable Spotify playback
on the user's reported new pins. Do not declare it the observed fault's cause.

Read-only HTTP access to 192.168.11.127 reported firmware 0.2.1, build
Oct 16 2026 18:57:16, sketch size 4,514,288 bytes. The diagnostics snapshot was
paused, with Spotify owning output, full PCM buffer, zero partial writes and
zero write failures. Those counters do not establish electrical signal integrity.

The shared-pin correction passed `pio run -e esp32s3` and `git diff --check`.
Image size: 4,514,288 bytes; app-slot headroom: 2,039,312 bytes. Static DIRAM:
260,523 bytes (76.23%), 81,237 bytes free. Existing 4 MHz TFT experiment and user
changes were preserved. No vendor changes or volume changes were made.

- Prepared image: `.pio/build/esp32s3/firmware.bin`.
- Previous local export: `.pio/build/before-shared-audio-pins/`.
- Not installed; all hardware acceptance remains pending.
- Next proposed measurement: LRC at the noisy amplifier, referenced to its
  local GND, to check for a regular frame clock during Spotify playback.

A separate existing local-radio volume mismatch was found: volCurve reaches
55 while the bundled library defaults to 21 steps. This has not been changed;
Spotify scales separately, so it cannot alone explain both sources' symptoms.

## Later measurements and direct MP3 comparison

- Live LRC capture: X1/DC, 1 V/div, 10 us/div, 50 MSa/s. Frequency counter
  reports 44.100512 kHz, approximately 50% duty cycle. Displayed extrema
  -184 mV to 3.583 V. Clock is present; this does not establish valid I2S data
  timing. There are sloping plateaus and spikes, with probe effects unresolved.
- User reports probing BCLK silences the amplifier. X1 probe loading or contact
  disturbance can invalidate that measurement; do not declare an absent clock.
  X10 setup should be validated before further fast-clock probing.
- DIN capture shows digital activity, not evidence of correct stereo contents.
- Direct MP3 test URL: https://aoakley.com/articles/stereo-test-long.mp3
  Saved station slot 1, name "Left right". HTTP POST /api/stations/test with
  name/url starts it without saving station edits. This uses local-radio decoding
  and still uses Wi-Fi. It is a finite 18-minute file, not a live stream.
- Download verified HTTP 200, audio/mpeg; decoded format is 16 kHz stereo.
  PCM analysis of the beginning confirms exact zero samples in the opposite
  channel during multiple left-only and right-only speech intervals. The centre
  announcements intentionally use both channels. This rate differs from Spotify.
- Following the direct-test instructions, user reports static returns when the
  probe is disconnected; left speaker plays both channels, and right speaker
  produces speech-correlated static during left speech (a "static ghost").
  This is not evidence that Spotify detects mono hardware. Audio-correlated
  contamination could involve ground/power coupling or I2S interpretation;
  the exact mechanism is unproven.
- Next useful input: physical wiring photos, including ESP32 ground to the hub,
  each amplifier ground, board pin labels and the three I2S branches. Soldered
  wiring makes board-swapping impractical. Avoid prescribing more rewiring before
  seeing this layout.
