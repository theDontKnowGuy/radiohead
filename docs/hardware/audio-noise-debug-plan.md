# Audio noise isolation plan — 2026-10-19

## Current baseline

The user reports working firmware and rebuilt, soldered AWG22 wiring shorter
than 10 cm. Assume basic soldering is sound; do not restart with joint repair
or wholesale rewiring. Both speakers exhibit the noise.

Track two symptoms independently:

- **S:** static/interference, conspicuous during web volume adjustment but also
  occurring at other times.
- **D:** music sounds capped/clipped or has a sandy layer.

Neither cause is established. Similar symptoms in both speakers make shared
power, ground, digital audio, software and gain worth testing first. This does
not exclude identical amplifier configuration problems.

Existing records: [earlier audio investigation](audio-static-diagnosis.md),
[TFT/scope observations](tft-white-screen-diagnosis.md), and
[stereo wiring](max98357a-stereo.md). Old measurements describe the old assembly;
do not treat them as measurements of this rebuilt assembly.
Recorded hardware: two MAX98357A boards, Dayton PC83 speakers reported as 4 ohm,
I2S BCLK=39, DIN=40, LRCLK=41. Record any changed components/power arrangement
when beginning the bench session, plus the running firmware version/build.

## Confirmed source observations, not measured causes

These observations describe the baseline before the correction recorded below.

- `src/app_state.cpp`: UI indices 0–21 map to a curve ending at 55.
- `src/media.cpp`: local playback passes that curve to `audio.setVolume()`.
- Bundled `audiolib_structs.hpp`: default volume steps = 21;
  `Audio.cpp::setVolume()` clamps to that limit. No application call to
  `setVolumeSteps()` was found. Index 12 requests 20; index 13 requests 23 and
  therefore clamps to 21, as do all higher indices. This can reach full digital
  level prematurely. It does not prove clipping or explain every noise.
- Spotify uses a separate 0–255 scaling path; the same mapping defect cannot
  by itself explain equal symptoms on both sources.
- Modern web volume POST calls `setRadioVolumeIndex()`, which requests redraw,
  and queues a settings save after 1,000 ms. The old `/setvol` route saves
  synchronously and redirects. The current browser slider waits 140 ms after
  input before sending; player polling occurs every three seconds.
- The local audio library already has volume ramping. Do not assume its volume
  setter necessarily produces an instantaneous discontinuity. Spotify's sink
  applies an atomic gain value to buffers; inspect transition behavior if tests
  implicate that path.
- Main-loop settings saves and display work are candidates for timing tests,
  not established underruns. Do not claim a compile proves clean audio.

## Experimental rules

Use the same short passage and chosen low listening level. Record S and D
separately (0 absent, 1 faint, 2 clear, 3 severe), source, volume index, EQ,
browser state, display state, USB connection and power source. Repeat a useful
comparison three times, then restore the original condition (A/B/A).

Change one variable at a time. Keep the serial monitor closed. Do not add USB
Serial prints to the Spotify output task; that has already caused audio stalls.
Initial tests need no new firmware. Later diagnostic images are proposed here,
not implemented, built or flashed.

## 1. Listening baseline and event timing

1. Record existing bass/mid/treble, then set all three to zero. Start quietly;
   do not sweep to maximum to demonstrate the mapping defect.
2. Play a repeatable passage, close radio browser tabs and listen for 30 seconds.
   Record S and D. Reopen the player without touching controls and repeat.
3. Make one small web volume step, wait five seconds, then reverse it. Observe
   whether S occurs immediately, about one second later, or near disappearance
   of the volume overlay roughly three seconds later. Separate a transient
   burst from simply making existing noise louder.
4. Close the web page and repeat equivalent encoder steps. A common response
   implicates shared gain/redraw/save paths or changing electrical load;
   web-only noise implicates additional HTTP/network work, not necessarily RF.
5. At unchanged volume, compare simple web reads and TFT navigation. Avoid
   scans, firmware updates or other deliberately disruptive operations.
6. Mute without pausing, then unmute. Separately pause if the source supports
   it. Software mute is not amplifier shutdown; quiet after mute can reflect
   zero samples, changed amplifier switching/load, or removal of source noise.
7. Compare radio and Spotify, if available, at approximately equal acoustic
   loudness, not identical slider numbers. Recheck known left/right material
   without assuming the old channel-separation problem remains.

Decision: EQ-dependent D leads to headroom tests; event-linked S leads to
timing/power tests. Both can remain active branches.

## 2. Validate the Instrustar before trusting captures

Use the ISDS210A / Multi VirAnalyzer setup already recorded. Its previous X10
scaling discrepancy is unresolved, and probing BCLK at X1 previously silenced
audio. First verify probe attenuation on a known DC rail, preferably checked
with a multimeter: physical X10 and software X10 should report the actual rail,
not ten times it. Compensate the probe using its documented calibration method.
If X10 remains unreliable, resolve it before probing digital clocks.

All ordinary probe ground clips go only to circuit GND. Never connect one to
SPK+ or SPK−: these are bridge outputs. Treat both channel grounds and the USB
computer ground as connected; do not use them to bridge different nodes.
Do not float protective earth. Attach clips with power off and avoid adjacent
pin slips. Note if attaching the scope changes the sound: it changes the test.

## 3. Measure supply behavior at each amplifier

With one channel first:

1. Ground clip at the amplifier's own GND, with the shortest available ground
   connection. Probe tip on that same GND: capture the measurement noise floor.
2. Move only the tip to that amplifier's VIN. Start DC coupling, approximately
   1–2 V/div and 1 ms/div, adjusting offset/range to show the entire supply.
   Confirm the expected nominal rail before selecting sensitive scales.
3. Capture steady playback and isolated volume steps. Then use AC coupling,
   initially 100 mV/div and 1 ms/div, adjusting to fit. This shows ripple, not
   the absolute supply. Compare to the shorted-tip capture at the same settings.
4. Repeat at 10 ms/div for slower bursts and 10 us/div for faster interference.
   Check displayed sampling rate; a slow acquisition cannot exclude fast events.
   Use normal/single triggering on a reproducible dip once its scale is known.
5. Repeat on the other amplifier and the ESP32 3.3 V rail. Log minimum DC
   voltage and ripple with probe settings; do not rely on automatic peak-to-peak
   statistics alone or call every sharp spike audible noise.

Noise correlated with a local rail disturbance supports a power/return-path
branch. A clean capture only excludes disturbances visible with that setup.
For ground-return comparison, use one clip at the common hub and the tip at
amplifier GND; label this ground-path voltage, not local supply ripple. Do not
attach another channel ground at the other end and accidentally bypass the path.

## 4. Controlled software audio: silence, tone and gain

Prepare a reversible diagnostic image if the initial tests do not resolve it.
Retain the production image and identify each build. Use a fixed, local PCM
source so playback does not depend on Wi-Fi or a decoder: continuous digital
zero and a smooth-start 1 kHz sine beginning at −30 dBFS, with left-only and
right-only options. Keep the same known I2S format/pins and bounded buffers.
Avoid full-scale tones and begin with low output gain.

Compare the normal playback path with the minimal PCM output path, documenting
which processing was bypassed. Quiet minimal output implicates some omitted
software/activity but does not alone identify a bug. Noise even with local PCM
keeps format, electrical integrity, amplifier and supply branches open.

Test a separate application-level correction aligning local library steps to
the 0–55 curve, after reviewing initialization and restored volume behavior.
Do not edit vendored code or combine this with pin changes. Verify effective
volume and low-level behavior before listening louder. Compare at matched
loudness: merely making everything quieter is not proof of a complete repair.

For persistent D, vary PCM level conservatively, inspect amplifier GAIN wiring,
and determine where distortion starts. Use pre/post-processing peak/clipping
counters where appropriate. If DSP clips before final attenuation, lowering
the final volume does not repair the waveform. Test flat EQ and source material
independently. Speaker/enclosure distortion remains a later branch if clean
electrical audio is established.

## 5. Separate Wi-Fi, display and storage effects

With the exact same local PCM running, compare Wi-Fi off, Wi-Fi associated
without intentional traffic, and controlled HTTP traffic. Keep display,
gain and tone unchanged. Disabling Wi-Fi during a stream is not this test.

If Wi-Fi changes S, measure rails concurrently. That result can arise from
RF pickup, supply-current bursts or software scheduling. It does not uniquely
prove radiated RF. The ISDS210A cannot directly characterize 2.4 GHz RF.

Use separate variants to hold backlight duty constant, suppress TFT transfers,
and defer settings writes. Do not disable all three together. With appropriate
in-memory counters, correlate audio service gaps, PCM starvation/I2S failures,
HTTP processing, redraws and saves. Fetch results outside critical output work.
Only then test physical antenna/cable separation, without changing grounds or
placing conductive shielding over the antenna.

## 6. Components, decoupling and signal integrity

Inspect both amplifier board types, existing bypass parts, GAIN and SD_MODE
networks. Short thick wires do not replace local decoupling or prevent shared
return impedance. Compare the actual boards to the manufacturer's reference
circuit, which shows 10 uF and 0.1 uF supply bypass capacitors. Verify what is
already fitted before adding parts. Test additional local bulk capacitance or
an alternative regulated supply only as a measured A/B experiment, with correct
polarity/ratings and power off for wiring. Do not parallel supply outputs or
backfeed USB. Do not attach arbitrary capacitors to I2S or speaker outputs.

MAX98357A uses digital I2S input; an analog input coupling capacitor is not
normally needed. GAIN intentionally unconnected is a defined 9 dB setting;
it is not inherently a floating-input defect. Check actual breakout resistor
networks before changing gain or grounding SD_MODE (which may be tied to VIN).

Adjacent GPIO numbers alone are not evidence of interference. Inspect shared
return paths, parallel routing, speaker-pair proximity and clock waveform at
the receiver first. With validated X10 probing, examine LRCLK, BCLK and DIN at
each amplifier, referenced locally. Start around 10 us/div for LRCLK and
100–200 ns/div for BCLK, adapting to actual clock rate and scope capability.
Check frequency, clean threshold crossings and timing; the BCLK/LRCLK ratio
depends on configured slot width, not just audio sample bit depth.

The two-channel scope cannot establish complete three-signal I2S validity in
one capture. Ringing seen with a long probe ground may be a measurement artifact.
If probing changes the sound, resolve loading/contact before interpreting it.
Only if evidence supports ringing/crosstalk, trial source-series damping or
rerouting one variable at a time. Reserve GPIO reassignment for a controlled
later experiment. Do not start by replacing boards or desoldering the assembly.

If clipping must be measured at speaker outputs, prepare a separate differential
measurement procedure with suitable filtering first. Raw class-D PWM is not the
audio waveform; ordinary ground-to-SPK− probing is unsafe.

## Closure and first session

First session: complete the listening baseline and validate the scope; then
capture VIN relative to local amplifier GND. Stop and interpret these results
before choosing a firmware experiment or modifying hardware.

Accept a cause only after a repeatable intervention changes the relevant symptom
and, when practical, restoring the old condition restores it. Final acceptance:
clean intended listening range, smooth web/encoder volume, correct channels,
normal Wi-Fi/display activity, both playback paths if used, and serial closed.
Document S and D separately even if one remains unresolved.

The initial plan was documentation only. Subsequent implementation and
verification are recorded below; hardware results remain pending.

Primary hardware reference: [Analog Devices MAX98357A/B datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf),
gain selection and application circuits. Instrument documentation entry point:
[Instrustar ISDS user guides](https://enbbs.instrustar.com/forum.php?mod=viewthread&tid=2);
the guide listing was discoverable, but its page could not be opened during this
planning pass, so no unverified software menu names are prescribed here.

## First experiment prepared: local volume scale correction

At the user's request on 2026-10-19, `mediaConfigureOutput()` now calls
`audio.setVolumeSteps(volCurve[21])` before restoring playback volume. The maximum
is therefore 55, matching the existing table. Slider indices, persisted volume,
mute behavior, EQ and Spotify scaling remain unchanged. The setting persists
through the inspected stream reset and Spotify-to-local output restoration.
No vendored code or pin assignments changed.

Expected behavior: local playback is quieter at most existing slider positions;
indices 13–21 now continue increasing output rather than all selecting maximum.
The maximum available digital output remains the same. This also affects local
boot audio and podcasts. It does not establish that static or distortion is fixed.

`pio run -e esp32s3` and `git diff --check` passed. Exported application image:
4,514,512 bytes, with 2,039,088 bytes free in the application slot.
Static DIRAM usage is 260,523 bytes (76.23%), leaving 81,237 bytes.
The matching export is `.pio/build/esp32s3/`; the prior export was preserved at
`.pio/build/before-volume-scale-fix/`. Neither export is added to version control.
The correction has not been flashed or listening-tested.

After installation, start at low volume with flat EQ, raise gradually to a
comfortable level, and compare S/D at similar acoustic loudness to the baseline.
Check small web and encoder steps, mute/unmute, station changes and return from
Spotify. Avoid a maximum-volume test; unchanged maximum output can still clip
the amplifier or expose a separate fault.

## Listening report after volume correction — 2026-10-19

The user reports testing the new volume behavior:

- Index 11 is barely audible, subjectively about 1/10 loudness.
- Index 21 sounds reasonably OK but lacks full sound and retains some capped
  quality; this is not a clean-audio acceptance result.
- Index 16 sounds worst: unstable music, described as if speakers lack power.
  That description is a listening impression, not a power measurement.

The user subsequently confirmed internet radio, bass/mid/treble all zero, and
continued instability after ten seconds without control input. Device build
identity has not been independently read back. This report does not test Spotify.

Source calculation for local playback: the application curve feeds the library's
own nonlinear attenuation curve. At settled gain, UI 11/16/21 correspond to
approximately -46.87/-20.73/0 dB, or 0.453%/9.195%/100% sample amplitude relative
to the same PCM at full volume (before considering other processing). These are
not perceived-loudness percentages or speaker-power measurements, and do not
apply to Spotify. The clamp correction therefore leaves a strongly compressed
useful slider range; volume usability needs a separate follow-up.

Hold the current firmware for the next comparison. Persistent D at fixed index
16 with flat EQ is confirmed. Use repeatable material and close the browser to
remove polling as a confounder. Compare a nearby higher comfortable level,
without requiring maximum volume. A stable-level fault cannot be explained solely by
the brief web volume request. Fixed interference becoming more audible relative
to quieter music, sample-dependent digital faults, DSP and supply behavior remain
possible; no cause is proven. Next capture local amplifier VIN at index 16,
starting with validated X1/DC supply measurement because X10 scaling remains
unresolved. X1 is for this initial rail measurement, not fast I2S probing.
Compare at a higher comfortable level after validating the first capture.

## First new supply capture — 2026-10-19

The user supplied a scope screenshot following the local amplifier VIN test
instructions. Visible settings: CH1 X1, DC, 2 V/div, 1 ms time setting,
1 MSa/s, Auto trigger, trigger style None. Displayed CH1 extrema:
Vmin 4.785 V, Vmax 5.488 V, Vpp 702.703 mV. The trace is near 5 V with
narrow excursions and no obvious sustained collapse in the displayed record.
The trigger-level readout 132.173 V is not the measured supply voltage.

The screenshot does not independently verify physical probe attenuation,
contact locations, or whether probe attachment changed the sound. Do not treat
automatic extrema as established local ripple or as proof of adequate power.
Next: retain the local GND clip, select AC coupling and 200 mV/div, keep 1 ms,
and capture with the probe tip touching that same GND point. This establishes
the measurement floor before a matching VIN capture. Keep playback at 16 and
do not probe I2S using this X1 setup.

## Alternate power source capture — 2026-10-19

The user replaced the power source and reports about 300 mV displayed range
in DC coupling. A new screenshot is explicitly VCC-to-GND, AC coupled, not
the requested tip-to-local-GND measurement floor.

Visible settings: CH1 X1/AC, 200 mV/div, 5 ms time setting, 100 kSa/s,
Auto trigger, style None. Displayed extrema: -38.489 mV to +46.345 mV,
84.834 mV peak-to-peak. These AC values do not give the absolute supply voltage.
The old capture used DC/2 V, 1 ms and 1 MSa/s; differing acquisition settings
and lack of a measurement-floor capture prevent a direct quantitative claim
of supply improvement. Faster disturbances may not be resolved here.

Next establish whether the source change improved S, D, both or neither at
volume 16. Keep the new source and present scope settings, move only the tip
to the same local amplifier GND as the clip, and capture the measurement floor.
Effect of probe attachment on sound is still unreported. No cause established.

## Ground-reference capture and supply comparison result — 2026-10-19

The user explicitly reports that changing the power source did not improve
either static or sandy/unstable music at volume 16.

Screenshot supplied in response to the tip-to-local-GND request: CH1 X1/AC,
200 mV/div, 5 ms, 100 kSa/s. Displayed Vmin -17.281 mV, Vmax 14.532 mV,
Vpp 31.813 mV. The trace is visibly flatter than the prior VCC capture.
At these settings, VCC shows more variation than the measurement floor, but
peak-to-peak values cannot simply be subtracted to obtain true supply ripple.
Neither capture establishes an audible-noise mechanism.

Deprioritize further external-supply swapping. Local bypassing, return paths,
and disturbances outside this acquisition's resolution remain open.
Next, remove the scope probes and note any audible change, then use the
encoder's short-click mute at fixed volume 16 (do not pause/stop/change station).
Listen after ten seconds and unmute. Record whether steady static remains,
vanishes or only becomes quieter, separately from the click/transition.
Mute retains streaming but removes intended signal; a quiet result would not
uniquely prove software, since amplifier switching/load also changes.

## Mute result — 2026-10-19

After receiving instructions for HTTP `/api/player/mute`, the user reports
that mute silences the speakers completely. The reported result is audible
silence, not a measured zero-output waveform. The mute implementation retains
streaming and sets digital volume to zero; it does not intentionally stop I2S
or shut down Wi-Fi. Static therefore disappears with removal of intended audio,
but changed amplifier switching/current can also remove hardware interference.
Do not conclude that the amplifier, supply or Wi-Fi is proven good.

Next compare current Spotify playback with internet radio at approximately
matched acoustic loudness, EQ flat, on the same rebuilt hardware and power
source. The post-fix local volume curve differs markedly from Spotify, so
identical slider indices are not a controlled comparison. Use familiar clean
material, allow controls to settle, and assess steady S and D separately.
Clean Spotify would prioritize the local decoder/DSP/I2S configuration path;
similar symptoms would prioritize shared output/power/signal-integrity paths,
without excluding shared software. Earlier pre-rewire Spotify symptoms are
historical evidence, not a replacement for this current comparison.

## Distortion prioritized; current cross-source test — 2026-10-19

The user explicitly prioritizes capped/sandy music over static, using static
only when it helps isolate the same fault. The current Spotify comparison at
flat EQ and approximately matched listening loudness is also similarly
distorted. This is new post-rewire evidence for a problem shared across paths.

Read-only HTTP inspection at the previously recorded address confirmed radio
playing, volume 18, mute false and bass/mid/treble zero at that instant. This
does not replace the user's observations at 16. Resource diagnostics at uptime
545,031 ms showed zero allocation failures and lifetime loop/audio/web maxima
of 1,468,079 / 288,289 / 216,159 us. These are cumulative maxima, not events
correlated with the distortion; do not call them proof of audio starvation.
Spotify write counters read zero while radio was active, which is not a current
Spotify output validation.

Read-only source review: radio uses signed 32-bit stereo Philips I2S, Spotify
signed 16-bit stereo Philips I2S, sharing GPIO39/41/40. Both word widths are
supported by MAX98357A per its datasheet. Local EQ uses the stereo DSP routine
with separate channel state; Spotify flat EQ bypasses its tone filters. The
local resampler is not enabled by the application. No newly established source
defect explains the common distortion. No new firmware change is justified yet.

Next prioritize shared electrical/configuration paths, retaining decoder/timing
as unexcluded alternatives. Before clock probing, validate physical X10 plus
software X10 on the known amplifier VIN-to-local-GND rail in DC coupling. It
should agree approximately with the prior X1 DC voltage, not read ten times
it. Resolve any scaling discrepancy before probing BCLK/DIN. A correct DC ratio
is necessary but not sufficient: probe compensation, short ground, acquisition
rate and whether contact changes sound must also be checked for fast signals.

## X10 DC scaling check — 2026-10-19

Screenshot after the X10 VIN request shows CH1 X10/DC, 2 V/div, 1 ms,
1 MSa/s, Auto/style None. Displayed Vmin 4.917 V, Vmax 5.235 V,
Vpp 318.127 mV. The indicated voltage is consistent with a nominal 5 V rail;
the prior tenfold scaling discrepancy is not reproduced. This is not a probe
compensation test or proof of clean supply. The displayed 7.111 kHz automatic
frequency reading on this DC rail is not established as a physical noise source.

Next measure BCLK at the amplifier using X10/DC, 1 V/div, approximately
200 ns/div, shortest practical local GND connection, and CH1 rising-edge
trigger around 1.5 V in Auto mode. Capture several cycles and visible sample
rate. Observe whether touching BCLK changes or silences audio. If it does,
withdraw the tip and treat the capture as a disturbed circuit. Do not infer
ringing or timing failure solely from uncompensated/poorly grounded probing.

## X10 BCLK capture — 2026-10-19

Screenshot in response to amplifier BCLK request: CH1 X10/DC, 2 V/div
(not the requested 1 V/div), 200 ns, 100 MSa/s, Auto/rising-edge trigger
at 478 mV. Displayed frequency 2.822 MHz, extrema -1.445 V to +4.599 V,
Vpp 6.044 V. Several roughly regular cycles are visible, with flat high/low
regions and pronounced edge overshoot/undershoot. 2.822 MHz is consistent with
44.1 kHz x 64 clocks/frame for the local 32-bit stereo path, but LRCLK has
not yet been measured alongside it and correct data timing is not established.

The edge excursions are a specific signal-integrity lead, not a confirmed
electrical fault: probe compensation and ground-loop geometry remain unverified.
Do not prescribe termination, capacitors, GPIO changes or amplifier replacement
from this capture alone. Next request a close photo showing probe tip and ground
connection at the amplifier, plus whether probing BCLK changes playback. If a
long clip lead is present, repeat with a suitable short ground spring/local
connection before judging overshoot; also validate probe compensation as needed.

## Probe setup photo and loading observation — 2026-10-19

User confirms that the sound is the same with the measurement probe attached.
A close photo shows the probe at the amplifier header and a conventional
ground clip/lead arrangement, not a short tip ground spring. The entire return
lead geometry and pin labels are not visible; do not identify pins by wire
color or infer a solder short from the photo. The photo does not establish
whether the apparent edge spikes are real or a probing artifact.

Current X10 BCLK probing causes no reported audible change, unlike the earlier
X1 observation. Keep the measured BCLK frequency as useful evidence; treat
edge extrema as provisional pending compensation/short-return verification.
Next move only the tip to labelled LRC/LRCLK/WS at the same amplifier,
X10/DC, 1 V/div, 10 us/div, Auto rising-edge trigger at about 1.5 V.
Keep the same playback source to compare BCLK and LRCLK meaningfully.
For a 2.8224 MHz clock with 64-bit frames, expect approximately 44.1 kHz
(22.68 us period). Agreement verifies clock ratio only, not decoded audio
contents, data setup/hold or absence of intermittent faults.

## LRCLK capture — 2026-10-19

Screenshot shows CH1 X10/DC, 1 V/div, **1 us** time setting (rather than
requested 10 us), 100 MSa/s, Auto/rising-edge trigger at 1.5 V. Hardware
frequency counter reads 44.100497 kHz; waveform measurement reads 44.101 kHz.
Displayed extrema -1.127 V to +4.175 V, Vpp 5.302 V. Main view shows one
low-to-high edge, with small narrow spikes on both plateaus; overview shows
repeated clock activity. The same probe-ground caveat applies to extrema.

Measured LRCLK and earlier rounded 2.822 MHz BCLK agree with approximately
64 clocks per stereo frame. This supports nominal 44.1 kHz/32-bit stereo
clocking but does not establish data validity or continuous fault-free timing.
No need to repeat this capture solely to change the time setting.

Next simultaneous two-channel capture at the same amplifier: CH1 BCLK,
CH2 DIN, both probes X10/DC with matching software factors, both grounds on
the same local amplifier GND, 1 V/div, about 500 ns/div and CH1 rising-edge
trigger at 1.5 V. Separate vertical positions so traces are readable. Observe
whether the second probe alters playback. Check DIN stability around rising
BCLK edges; a small capture cannot prove all words correct. No component
changes based on edge spikes until probing artifacts are addressed.

## Simultaneous BCLK/DIN capture — 2026-10-19

Paused screenshot: both channels X10/DC, 1 V/div, 500 ns, displayed
100 MSa/s, CH1 rising-edge trigger at 1.5 V. BCLK counter reads
2.822433 MHz. CH1 extrema -1.127 V to +4.387 V (5.514 Vpp);
CH2 extrema -2.180 V to +6.133 V (8.314 Vpp).

Assuming requested connections CH1=BCLK and CH2=DIN, visible major DIN
transitions align approximately with falling BCLK edges, with settled data
before subsequent rising edges. No obvious phase reversal is evident in this
small capture. It does not verify all bits/frames, exact setup/hold margins,
or intermittent errors. The data waveform frequency statistic is not an audio
sample rate. CH2 amplitude/edge excursions require measurement validation;
do not interpret its reported 6.133 V maximum as established GPIO overvoltage.
Whether the second probe changes playback has not been separately reported.

Next keep CH1 on BCLK, both ground clips at the same local amplifier GND,
and the identical fast acquisition settings. Move only CH2's tip to that same
GND point, acquire and pause. This checks clock-correlated pickup/ground-lead
effects at 100 MSa/s; the earlier 100 kSa/s ground reference is insufficient
for this comparison. If CH2 shows large spikes even here, correct probing
before judging DIN ringing. A quiet shorted-tip trace alone would still not
exclude probe resonance while connected to a driven DIN signal.

## Fast CH2 ground-reference result — 2026-10-19

Screenshot supplied after the CH2-tip-to-local-GND request, with CH1 retained
on BCLK: both X10/DC, 1 V/div, 500 ns, 100 MSa/s, Auto/rising-edge 1.5 V.
CH2 reports -517.647 mV to +417.647 mV, 935.294 mVpp, and 2.815 MHz;
the visible small spikes broadly correlate with clock transitions. CH1 counter
is 2.822432 MHz, extrema -1.127 V to +4.493 V, 5.620 Vpp.

Assuming CH2 tip and clip are on the same physical GND point as instructed,
this establishes substantial high-frequency measurement pickup/artifact. It
does not establish 0.94 V of actual local ground movement, prove all prior
overshoot false, or identify the audible distortion's cause. If the tip and
clip are at different ground locations, actual return-path voltage is also
included and their locations must be clarified. Do not subtract Vpp values.

Next reduce the measurement loop with the probe's proper short ground-spring
accessory at its ground sleeve. Validate CH2 tip-to-same-GND again at the same
fast settings before repeating a driven-signal measurement. If that accessory
is unavailable or unidentified, request its availability/photo before giving
hardware-specific fitting instructions. No termination/component changes or
source-code fixes are justified by these edge extrema yet. Capped music
remains the primary unresolved problem.

User confirms CH2 tip and ground clip were on the same amplifier GND pin and
also physically touching one another. Thus the fast shorted-input capture was
performed as intended; do not ask for this confirmation again. User did not
understand the ground-spring accessory request. Explain it with a recognizable
reference picture before requesting any fitting/removal. Accessory availability
is still unknown; do not assume it was supplied with this particular probe.

User has no probe ground-spring accessory and supplied a photo of an ordinary
long extension spring. This is not a suitable substitute for the short probe
ground contact; do not instruct fitting it to the probe or radio. Proceed without
requiring an accessory purchase. Suspend diagnosis based on fast edge amplitudes.

Next obtain clear component-side photographs of both amplifier boards, including
chip markings, VIN/GND/GAIN/SD_MODE labels and their connections. The existing
side-on probe photo obscures these details. Establish exact part/board type,
actual gain/channel-select networks and fitted bypass components before any
change. A defined floating GAIN configuration is not automatically a defect.
Capped audio across both paths remains unresolved; no new cause claimed.

## Amplifier board photos — 2026-10-19

User supplies rear and component-side views of a purple breakout. Rear
silkscreen: MAX98357A, 4–8 ohm 3 W, VIN 2.5–5.5 V, default 9 dB gain and
(L+R)/2 output. Front pin order, left to right with speaker terminal at top:
LRC, BCLK, DIN, GAIN, SD, GND, VIN. Main IC top marking is not readable;
board branding does not independently authenticate silicon. Do not assume
these views establish that both amplifier assemblies are identical.

Rear view shows the middle GAIN header pin apparently without an external
wire. This is consistent with intentional default 9 dB gain, not a missing
required bias. Front shows several fitted passive parts near supply and speaker
output. Their values and full nets cannot be established from these photos;
do not diagnose absent decoupling or recommend indiscriminate extra capacitors.
Photos do not justify a soldering fault diagnosis. A loose lead visible while
the assembly is being handled is not evidence of the powered test connection.

Next measure SD relative to each amplifier's own GND during playback, using
DC coupling. Obtain both voltage levels and whether they remain steady.
This checks actual channel selection in the rebuilt assembly; previous 0.9 V
and 3.8 V reports were from the old wiring with local-reference uncertainty.
Do not ground or change SD wiring as a test: it may be tied directly to VIN.
Channel selection instability/mixing is a hypothesis, not an established cause
of capped audio. If the two SD levels are stable and appropriate, proceed to
a controlled signal/channel-isolation test instead of further speculative
configuration changes.

## User-directed change of investigation — 2026-10-19

User reports SD voltages correct; exact new numbers not provided. Treat that
check as completed and do not ask for another SD capture without new evidence.
User says the investigation is circling and requests a complete task for another
agent to build audio-only diagnostic firmware: no Wi-Fi, no screen, volume and
tone tests aimed at reproducing capped music. Also requests concrete amplifier
supply capacitor suggestions. Prepared self-contained handoff:
[audio-only firmware agent task](audio-only-firmware-agent-task.md).
This is a task document, not implemented/flashed diagnostic firmware.

Recommended capacitor experiment is local 470 uF/16 V low-ESR electrolytic plus
100 nF X7R across each amplifier VIN/GND, preserving fitted bypass parts.
Manufacturer baseline is 100 nF + 10 uF close to the IC; actual onboard values
remain unverified. Establish the diagnostic-firmware baseline before adding
capacitors so software and hardware changes are not confounded. No promised
cure and no component installation performed by the agent.


## Direct audio-only reproduction — 2026-10-19

The standalone diagnostic has now been implemented and the user reports running
its hands-free sequence with no encoder connected. Cases 01 (active I2S zeros)
and 02 (stopped I2S) are completely silent. Case 03 (1 kHz, −30 dBFS peak) is
clear. Case 04 (local stereo PCM music, 44.1 kHz/16-bit, −12 dBFS source with
additional −12 dB gain, true bypass) has the same “sandy” addition as regular
radio. No numeric music severity, runtime error counters, or newly confirmed
capacitor/supply arrangement was provided. Preserve these observations without
asking the user to repeat the same first-run questions.

This reproduces the symptom without initialized Wi-Fi/screen or production
stream decoder/DSP code. It does not prove hardware is faulty: the audible
fixture content, the direct I2S output path and signal-dependent hardware
behavior remain open. Clean zeros and a clean single sine do not establish
clean music reproduction. Do not call the root cause fixed or resume inconclusive
fast-clock scope captures based on this result.

Next source check: `.pio/audio-only-assets/music-reference.wav`, a playable WAV
with byte-identical PCM to the flash fixture (no extra radio attenuation).
Listen on computer/headphones at comfortable volume and establish whether the
same texture is in the source before another format/channel or capacitor A/B.
The host reference has been generated and byte/hash-verified; its acoustic
result is pending. See [the implementation/listening record](audio-only-validation.md)
and [run sheet](../../experiments/audio-only/README.md).


## Computer source clean; next matched music format A/B/A — 2026-10-19

User played the WAV reference and reports no “sand.” Preserve this as a completed
source-content check; do not ask for it again. Output device/SPL were not specified.
The sandy addition is heard on radio playback but not on that computer reference.
Direct sample handling/transport and signal-dependent amplifier/speaker/power
behavior remain unresolved; do not declare a hardware fault from this alone.

Built `.pio/build/audio-only-music-formats/`: an encoder-free 44.1 kHz stereo
music A/B/A, 16-bit slots for 30 s, 32-bit for 30 s, 16-bit again for 30 s, then
stopped indefinitely. Stage cues are 1/2/3 pips and a final 4-pip stop cue. Source
and gain match the first music test: −12 dBFS source with −12 dB extra gain.
Only word/slot width and resulting BCLK change. Samples/packing, timing and
headroom pass host checks; no acoustic result for this image yet. No agent flash
or hardware change was performed. Use the [current run sheet](../../experiments/audio-only/README.md).

Compare texture and any loudness difference, maintaining hardware as-is. A
repeatable 16/32 difference points toward the format/clock/interface path without
proving software versus receiver/electrical cause. If both equally sandy,
continue with same-music isolated left/right/both, not another scope-pickup loop.


## Requested level increase for the format comparison — 2026-10-19

User requests volume 18/21 because the encoder is disconnected and Wi-Fi is
absent. Built the separate `audio-only-music-formats-radio18` image, selected by
`python3 tools/build_audio_only.py --music-formats --radio-index 18`. All music
stages use corrected radio gain 0.3321142 (−9.57425 dB), replacing the previous
−12 dB extra trim; actual increase 2.42575 dB. Source PCM stays at −12 dBFS peak;
therefore this does not promise normal-station acoustic loudness at index 18.
Sequence remains 16/32/16 slots at 44.1 kHz, 30 s each, then stopped output.
Host samples/packing/gain/headroom and the exported artifact checks pass. No
agent upload or new A/B/A listening result is recorded yet. Keep the prior
clean computer reference and sandy direct-16 music observations established.


## 16/32/16 equally sandy; isolate channels with music — 2026-10-19

User reports all three passes have the same unwanted sandy sound in the format
comparison after the requested volume-18 build. No numeric rating or separate
loudness observation was supplied. Keep this as completed evidence: changing
44.1 kHz slot width/BCLK between 16 and 32 bits gave no audible improvement.
Do not repeat that format sequence as the next test or claim all digital
faults excluded. The computer source was clean; zeros/stopped silent; sine clear.

Next built profile: `--music-channels --radio-index 18`, export
`.pio/build/audio-only-music-channels-radio18/`. Left-only, right-only and both
play the same left-source music waveform at unchanged per-channel gain, 30 s
per stage, then stopped output. All stages remain 44.1 kHz/16-bit Philips. The
left source is copied, not mixed, onto active slots; inactive slots are exact
zero. Cues 1/2/3 indicate music stages and cue 4 is final stop. Cues themselves
are on both slots; assess actual speakers during the following music.

Record which physical speaker sounds in each isolated pass and whether each
alone is sandy or only both worsen. If both speakers sound during an isolated
pass, channel selection/mixing prevents treating this as an isolated speaker
load. An inactive amplifier remains powered/clocked/connected. A louder both
pass is expected from two speakers and is not proof of distortion or a supply
fault. No rewiring, shutdown-pin change, capacitor trial or scope capture is
required. Software/build checks pass; this new listening result is pending.


## Each channel still dirty; added capacitors on one amp did not help — 2026-10-19

User reports the same noise/dirt with either speaker alone and both in the
music-channel profile. This fails to show an improvement from reducing active
speaker load; inactive amplifiers remained powered/clocked/connected. No new
numeric rating or electrical measurement was given. Do not repeat this channel
comparison purely for another subjective verdict.

In response to the question about the recommended 470 µF + 100 nF VIN/GND
additions, user reports having added them to one speaker/amplifier, with no
audible difference. Exact component/placement details were not independently
specified. Preserve this negative trial; do not ask again whether the suggested
capacitors have been tried or claim local supply issues definitively excluded.

Next is objective on-device output evidence, not another format/level or scope
capture: `--music-channels --radio-index 18 --evidence-uart`. It runs the same
music at 18/21 and emits RAM counters only after I2S has stopped. The new mode
uses UART0 TX43/RX44 at 115200 through the currently observed WCH USB-UART
connector, distinct from native USB/JTAG `--evidence`. No rewiring is requested.
A bounded control-task FIFO transfer drains before any subsequent case can
start; the sample task never logs. Build/host/artifact checks pass; serial
hardware delivery and counter observations are pending. Open the monitor before
letting a fresh run finish, keep it open, copy a final `audio-only` line, and
state whether the dirt still occurs with this evidence mode. See the
[updated run sheet](../../experiments/audio-only/README.md).

## UART capture: no recorded output failure — 2026-10-19

The user supplied 126 identical complete stopped snapshots from the WCH UART
monitor: case 02, stopped=1, fault=0, generated=accepted=4,265,216 frames,
partial=errors=last_error=generation_deadlines=inferred_gaps=clipped=0,
peak=2,364, driver_underruns=unavailable. UART delivery is now verified. These
are repeated final counters for one run, not repeated independent tests.
The peak matches the copied left source at radio index 18; final trim=0
describes the stopped case. No output failure was detected by these counters;
electrical receiver behavior and actual DMA underruns were not measured.

User confirms **"Yes, same dirt"** during this logged run. The symptom persists
with no failure detected by these counters. This does not establish a faulty
amplifier or exclude electrical I2S/power/load behavior. Do not repeat the
flash/counter capture, capacitor trial, or previous channel/format comparisons
merely because the software counters are zero. Next proposed branch is a single
known-good speaker or amplifier substitution, keeping this firmware and gain;
asked what spare parts are already available before selecting the comparison.
No purchase, hardware change or new upload has been performed. See the
[runtime interpretation](audio-only-validation.md#uart-runtime-capture-received--2026-10-19).

## Spare components available; speaker substitution next — 2026-10-19

User confirms having both a spare amplifier board and another passive 4–8 ohm
speaker. First proposed comparison replaces one speaker only, with all power
(including USB) disconnected during the change. Use the same impedance as the
original if available, retain the existing amplifier and its two output
terminals, and leave the other channel and current radio-index-18 diagnostic
unchanged. Listen specifically to the replacement during music after cue 3,
when both data channels carry the same mono signal. Report cleaner/same/worse;
no result has been supplied yet. Speaker/load changes can affect amplifier
behavior, so a cleaner result would narrow the cause without proving the
original speaker defective. Do not swap the spare amplifier simultaneously.

## User questions substitution; diagnostic music recording analyzed — 2026-10-19

User explains that the existing two speaker/amplifier pairs both sound equally
dirty, and supplies `Yefe Nof Street.m4a` to analyze the beginning. The opening
is confirmed to be the diagnostic music loop. Do not assume the earlier "Both"
answer establishes extra independently verified good hardware, and do not
record the proposed speaker swap as having occurred. Exchanging the two equally
suspect pairs would not provide a clean reference.

Direct audio input is unavailable to this model; no claim of hearing the dirt
is made. Local analysis of the opening finds the exact left-source music,
no decoded full-scale sample saturation (peak −17.11 dBFS), and a strong linear
acoustic-model match on held-out samples (correlation 0.9940). A lower-level
audible artifact may remain; model residual is not THD or isolated distortion.
Low bass is attenuated relative to source but speaker/room/phone response is
confounded. No unique failure mechanism or corrective firmware change follows.

At the prior logged sample peak, nominal genuine MAX98357A gain arithmetic
also puts the output far below normal maximum-power clipping; that calculation
does not measure actual board gain, receiver data or supply health. See the
[recording analysis](audio-only-iphone-recording-analysis.md) for scope,
parameters, measurements, conditional reasoning and primary references.

## Next discriminating test: one amplifier physically connected — 2026-10-19

User asks for concrete next steps after the recording analysis. Recommend a
physical single-amplifier comparison using the current diagnostic image, not
swapping the two equally suspect pairs. Previous left/right-only playback set
inactive PCM to zero while both amplifier modules remained powered and attached
to the shared bus. Complete removal of one module has not been reported.

With all power including USB disconnected, completely detach one amplifier
module's connections to the radio (VIN, GND, BCLK, LRC, DIN and any separate
external control connection), preserving every connection to the remaining
module. Do not merely disconnect its speaker or VIN. Do not alter the remaining
SD/GAIN wiring. Keep the remaining speaker, supply, firmware and radio-index-18
gain fixed. After power-up, judge the remaining speaker during music after
three high pips, when both slots carry the same mono source.

If cleaner, the second module's presence or the connections changed during
removal matter: shared bus loading, power/ground coupling or interference become
testable branches. This does not uniquely identify the second module as faulty.
If unchanged, a cause requiring the second module is not supported under this
comparison; an independent audio path/reference or actual digital-data capture
is more informative than another width/level repeat. No result, rewiring or
flash has been performed by the agent.

Primary hardware reference: [MAX98357A/B data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
distinguishes powered quiescent operation, shutdown and clock-absent standby;
digital silence is not physical module removal. This experiment tests that
remaining limitation rather than presuming that interaction is the cause.

## Sixth reported amplifier failure; volume-event audit — 2026-10-20

User reports another amplifier "burnt", the sixth, and repeatable static with
every web volume change in either direction. Failure timing, physical evidence
of damage versus loss of output, installed firmware and scope connection at
failure are not yet established; a clarification was requested. Do not infer
that a volume change caused the failure. Further powered tests/replacement
amplifiers should wait for this electrical-failure clarification. This changes
the priority of the previous single-module playback proposal.

Reviewed the completed audio-only validation and recording analysis. Sandy
music persists without Wi-Fi, display, production decoder/EQ or settings writes,
including a run reporting generated=accepted=4,265,216, partial/errors/
generation_deadlines/inferred_gaps/clipped all zero. Actual driver underruns
remain unavailable. The recorded format/channel/capacitor comparisons gave no
reported improvement. These results weaken ordinary production task starvation
as the explanation for continuous dirt; they do not establish electrical
receiver correctness or explain physically damaged amplifiers.

Read-only source/configuration audit (local source, not a live task dump):

- Radio library `PeriodicTask`: core 0, priority 2; default in Audio.h and
  `Audio::startAudioTask()`. No application override found.
- Spotify output: core 0, priority 4; connect/session task core 1, priority 5.
- Wi-Fi driver is configured on core 0; encoder Ctrl task core 0, priority 1.
  TCP/IP has no fixed core affinity.
- Arduino loop is core 1 and services radio `audio.loop()`, HTTP, UI/display
  and deferred settings saves. Weather and updater workers are core 1,
  priority 1. Sharing a core is not evidence of starvation.
- Standalone audio-only output: core 1, priority 20, without normal application
  services. Its observed dirt is therefore not dependent on the production
  core assignment.

`POST /api/player/volume` calls `setRadioVolumeIndex()`, queues persistence and
returns player state. The setter updates gain, records lastVolChange and requests
a display redraw. It does not reinitialize I2S or change the sample rate.
`serviceSettingsSave()` calls `saveSettings()` after 1,000 ms from the last
queued change. That routine visits the full settings set, including stations,
not just volume; unchanged values need not cause physical flash writes.
`.pio/idf-build/sdkconfig` has I2S ISR IRAM safety, PSRAM instruction/rodata
placement and flash auto-suspend disabled. Espressif documents deferral of the
default I2S interrupt during flash writes/erases. Thus persistence is a concrete
candidate for a delayed transient, not a proven underrun or explanation for the
standalone dirt. Moving a task alone does not address cache-disabled intervals.

Gain-path distinction: local radio already calls `gain_ramp()` after audio-task
iterations (default fading speed 50); do not claim it has no ramp. Spotify
loads an atomic gain once per block (up to 256 stereo frames) and scales with
that fixed value, without a sample-level ramp. That is a candidate for Spotify
volume-transition clicks, not proof of the common noise or amplifier failures.

After failure circumstances are understood, the next software comparison should
separate deferred persistence from gain/redraw/network activity: a separately
built temporary no-persistence volume experiment, then redraw isolation if
needed, and a matched sample-ramp comparison for Spotify. Keep identical source,
level and hardware; correlate immediate versus roughly one-second-late noise.
Do not silently change normal persistence semantics, move cores blindly, or
repeat already negative standalone format/channel/capacitor tests.

No firmware source/configuration change, flash, live-device command or powered
hardware test was performed in this audit. No claim that slow sample supply
alone burns a properly powered, correctly loaded amplifier is justified.

Reference: [ESP-IDF 5.5.5 I2S DMA and IRAM safety](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/i2s.html).

### Failure symptom clarified — 2026-10-20

User does not know when the latest amplifier failed. It now produces no sound
and its chip becomes very warm very quickly when power is applied. Smoke,
measured temperature/current, a measured short, and scope connection at failure
are not established. Record this as rapid heating with loss of output, not
confirmed internal destruction or failure during a volume change.

This raises electrical damage, an external output short/overload, or protection
operation above ordinary missing PCM as the explanation for this module.
Keep it unpowered; do not use repeated powered playback as the next test.
Disconnect all sources including ESP32 USB before handling wiring. If isolating
the failed board, isolate its signal/control connections as well as power,
so a connected ESP32 cannot drive an unpowered suspect module.

Asked whether a multimeter with resistance/continuity is available and whether
each speaker has an independent two-wire connection exclusively to its own
amplifier's bridge outputs. Shared speaker returns, chassis contact or scope
ground on a speaker terminal are not presumed; these are concrete electrical
checks, consistent with accepting basic soldering quality. Capacitor charge
must be allowed to dissipate and voltage verified near zero before resistance
measurements. Distinguish a brief capacitor-charging continuity indication
from a sustained low resistance. No powered test or repair was performed.

### Instruments and independent speaker wiring confirmed — 2026-10-20

User reiterates owning a multimeter as well as the Instrustar scope; retain both
as established equipment and do not ask again. Each speaker has two separate
wires going only to its own amplifier, with no speaker connection to GND.
Do not continue treating intentionally shared/grounded speaker returns as an
unanswered wiring question.

Next proposed unpowered measurements: completely isolate the suspect amplifier
from power, signals/controls and speaker; verify VIN-to-GND voltage near zero
before measuring resistance. Measure isolated module VIN-to-GND resistance in
both probe polarities, allowing bypass-capacitor charging indications to settle,
and separately measure the disconnected speaker/wire pair resistance. Stable
near-zero module supply resistance would support a supply short but not identify
its origin or establish why earlier boards failed. A non-short reading does not
clear a module that rapidly heats under power. No measurement result yet.

### Isolated supply and speaker resistance results — 2026-10-20

User reports the requested measurements:

- Suspect amplifier VIN–GND: 5 MΩ.
- Same pair with probes reversed: 5 MΩ.
- Disconnected speaker and cable: 4 Ω.

These do not show a hard supply-rail short or a shorted/open speaker cable and
voice-coil path under the meter's test conditions. They do not clear the
amplifier, exclude a fault appearing only under power, or establish the cause of
the repeated failures. Speaker DC resistance alone is not an acoustic test.

Next bounded unpowered check: diode mode between each speaker output and each
supply rail (SPK+ to GND/VIN; SPK− to GND/VIN), each in both probe orientations.
Record displayed voltage or OL; do not prescribe a universal normal diode drop
for this assembled board. Near-zero readings in both orientations would support
a shorted output-to-rail path. Other readings are inconclusive for powered
operation. Keep the module isolated and discharged; no new powered test yet.

## User redirects to storage-isolation firmware — 2026-10-20

User explicitly prioritizes crackles/sandy audio and requests disabling all
"SD activity" after boot values are read. Current application has no SD access;
implemented a separate opt-in normal-radio image suppressing persistent data
storage (NVS, LittleFS, OTA), including Wi-Fi/PHY NVS configuration. Cached boot
settings/Spotify credentials remain available. Volume/EQ/mute operate in RAM;
gain, timing, audio tasks and the pre-existing volume correction are unchanged.
Normal firmware remains the default. Saved artwork and updater activity are
also absent in this experiment, so a positive result would need a narrower
follow-up before assigning the cause specifically to deferred volume saves.

The diagnostic and normal builds and relevant host checks passed. Original
radio export preserved at `.pio/build/storage-quiet-baseline`; diagnostic at
`.pio/build/storage-quiet`. No flash, runtime/storage trace or listening result
yet. The experiment does not eliminate instruction/constant reads from program
flash. See [build evidence and install/restore steps](../../experiments/storage-quiet/README.md).

### Storage-isolation listening result — 2026-10-20

User reports **"static noise when change volume is now gone."** Record this as
the first positive listening result for the volume-change transient. Storage-
related activity is implicated, with the deferred settings save a leading
candidate; suppressing several storage paths together prevents attributing the
result exclusively to that save or claiming a measured DMA underrun.
Asked whether sandy/capped music remains with EQ zero after ten seconds at 16.
Keep this result separate from steady-state dirt and the amplifier failures.

User answers **"Still present."** The storage-quiet image therefore removes the
volume-change static without removing steady sandy/capped music under the
specified condition. Retain the test image as the current listening baseline.
Do not repeat storage suppression or infer that changing task affinity will
fix the remaining symptom: direct audio-only playback already reproduced it
on a different core with no recorded software timing/write/clipping failure.
Electrical I2S receiver behavior, amplifier/output/load and acoustic behavior
remain unresolved. No new build, flash or powered measurement accompanies
this result.

### Correction: running storage test not verified — 2026-10-20

User observes settings still save. Read-only GET of
`http://192.168.11.127/api/player` returns `storageQuietTest: false`, volume 14,
EQ all zero, source radio. The current radio therefore has persistence enabled.
The agent did not flash the diagnostic; building normal and diagnostic exports
does not establish which the user installed. Earlier attribution of vanished
volume-change static to this storage experiment was premature and must not be
treated as confirmed root-cause evidence. The earlier symptom reports remain
valid, but running mode at their exact time was not read back. Verify true mode
after explicitly installing `.pio/build/storage-quiet` before making that
comparison. The independent audio-only observations remain valid.
