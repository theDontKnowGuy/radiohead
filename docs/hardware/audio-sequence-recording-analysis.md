# Automatic audio sequence: phone recording comparisons

Latest result: the [Mac control](#fourth-recording-mac-control) has substantially
weaker added harmonics, including at a comparable recorded fundamental level.
This strengthens evidence for distortion in the radio playback chain but does
not yet separate I2S, amplifier and speaker behavior.

Earlier observation: the [third recording](#third-recording-right-speaker) has
right-only crackle/static **and** a sandy addition resembling the left speaker.
The user explicitly asks to defer the crackle and focus on the shared sand.

2026-10-20. User supplied `/Users/avner/Downloads/Yefe Nof Street 2.m4a` and
reports that **all audible segments sounded sandy**, with low volume making the
sound difficult to judge. This updates the current listening evidence; the
earlier report of a clean quiet 1 kHz sine remains a separate prior observation.

No direct audio-listening tool is available in this session. The findings below
are numerical analysis, not a claim that the agent heard the sandy sound.
Analysis and derived listening copies were made locally. The supplied file
was not modified or uploaded to another service.

## What this recording establishes

All eight audible segments are present, including both music passages. The
sequence is offset by approximately **+2.0 seconds** within the phone file:

| Signal | Low pass in recording | Higher pass in recording |
| --- | --- | --- |
| 250 Hz | 9–14 s | 42–47 s |
| 1 kHz | 16–21 s | 49–54 s |
| 250 Hz + 1 kHz | 23–28 s | 56–61 s |
| Reference music | 30–40 s | 63–73 s |

The recording lasts 73.5573 seconds, leaving about half a second after the final
music segment. The offset comes from thresholded narrowband envelopes, with
roughly 40 ms spread between the four 250 Hz onset anchors. These are alignment
estimates, not measurements of frame-level timing, jitter or dropped samples.

The second pass's recorded fundamental increases by about **6.21–6.22 dB at
250 Hz** and **5.62–5.74 dB at 1 kHz**, depending on the phone microphone channel.
The intended digital increase was 6 dB. Thus the programmed level step appears
in the recording. The modest difference does not identify amplifier compression
or phone gain processing. Whole-music RMS is contaminated by large transients
near the end and is not used to infer this gain step.

The original decodes to 48 kHz stereo float PCM. Its largest peaks are −3.5353
and −6.4921 dBFS, with no decoded samples reaching full scale. This rules out
decoded full-scale sample saturation, not prior analog clipping or nonlinear
phone/speaker/amplifier behavior. Quiet playback does not prevent analysis of
the tonal lines: they remain well above the surrounding recorded spectrum.

## Added harmonics beyond the digital reference

Steady-tone analysis excludes one second at each end, keeping three seconds
per tone. Hann-windowed one-second spectra are averaged with 50% overlap;
line power is integrated within ±4 Hz of each listed frequency. The same
measurement is applied to the **exact sequence WAV**, before any listening-copy
gain or limiting. Levels below are relative to the relevant fundamental, not
absolute acoustic output or amplifier THD.

| Input / measured line | Phone microphone 1, low | Phone microphone 1, higher | Digital reference, low / higher |
| --- | ---: | ---: | ---: |
| 250 Hz / 750 Hz third harmonic | −26.08 dB | −27.09 dB | −86.03 / −96.50 dB |
| 1 kHz / 3 kHz third harmonic | −21.45 dB | −20.59 dB | −86.03 / −96.50 dB |
| 1 kHz / 5 kHz fifth harmonic | −30.38 dB | −24.48 dB | −83.93 / −93.18 dB |

For scale, the first microphone's 3 kHz line is about 8.5–9.3% of its recorded
1 kHz amplitude. **That is a microphone-path harmonic ratio, not an amplifier
distortion specification.** The second microphone also records excess lines,
with different ratios: its 1 kHz third harmonic is −31.64 / −29.04 dB. The
speaker, room and microphone responses weight different frequencies differently.

The two-tone sections also contain added lines beyond the clean reference.
Because 1 kHz is exactly four times 250 Hz, harmonic and intermodulation products
overlap. This test cannot label a particular extra line as pure intermodulation
distortion or derive a calibrated IMD figure.

These repeated, signal-linked lines support **added distortion somewhere along
the playback/recording chain**. They are much stronger than the reference's
quantization harmonics, and are present even in the quieter pass. This is
stronger evidence than a subjective music-only comparison. It does not isolate
I2S receiver/packing behavior, amplifier circuitry, speaker/enclosure or the
phone's microphone/processing. A linear room response can change harmonic
ratios but does not by itself turn a pure sine into new harmonic frequencies.
No component failure, clipping mechanism or firmware fix is established.

## Louder listening copies and reproducibility

Artifacts are under ignored `.pio/audio-sequence-recording-analysis/`:

- `listening-plus12db.wav`: full recording, **+12 dB gain**, with a −1 dBFS
  peak ceiling for large transients. The limiter reduces gain over about 2.65%
  of the file including release tails. It does not reduce gain in any measured
  steady-tone window. This is a listening aid; do not analyze it as original
  evidence. It cannot recover acoustic detail absent from the original.
- `reference-plus12db.wav`: original digital reference with the same +12 dB
  gain, without limiting or other processing. The unused digital right slot
  remains zero. Absolute acoustic loudness cannot be matched through a phone's
  unknown recording gain.
- `analysis.png`: complete timeline, 1 kHz spectra and spectrogram.
- `recording.wav`: unnormalized float PCM decoded with macOS `afconvert`.
- `summary.json` and `analyze.py`: measurements, offsets, parameters and script.

Source SHA-256:
`f9d776ff5e6ad25670a7bf026e1cd0cfdde61fd8783960d9b4a28cbb9be644cf`.
The reference WAV SHA-256 is
`c4c50fa06a2930fc0bf32f513d438863bdbff10afaf592824910fb0a05f40cb5`.
Dependencies reuse the existing `.pio/audio-analysis-venv/`; none were added to
the firmware or installed for this follow-up.

## Separate speaker tests

The recorded firmware already drove **only the digital left slot**. The phone
file's two channels are its microphone channels; they do not establish which
radio speaker played or provide a right-output test. Earlier user reports found
the same dirt on each isolated active channel and both; a new right-channel
recording supplies a comparable measured reference, rather than an assumed
clean side.

Two separate images now reuse exactly the same automatic sequence and music
waveform, with **6 dB added to the complete sequence** for easier listening.
Only slot selection differs between them. No both-speaker phase is added.
Each image has its own exact WAV, timestamps and manifest. See the updated
[recording run sheet](../../experiments/audio-only/RECORDING-SEQUENCE.md#louder-separate-speaker-images).
No automatic flash is performed and no production behavior or volume curve
changes. These tests compare sides; equally sandy results still would not
identify a shared cause.

The recommended next recording was the new **right-only** image. Its lower pass
contains exactly the same active samples as this recording's higher pass, so
that comparison holds digital level/content constant. A new left-only run is
optional if a fresh matched pair is useful. The original reference was also
regenerated with the current renderer in a temporary directory and matches its
original WAV byte for byte. Both new firmware builds and the isolated graph,
artifact, channel-swap, silent-slot, full-render, CLI and diff checks pass;
normal firmware also builds into a separate export. Nothing was flashed.

## Third recording: right speaker

Later on 2026-10-20, the user supplied
`/Users/avner/Downloads/Yefe Nof Street 3.m4a`. Their distinction is explicit:

- **Crackle/static:** occurs only on the right speaker. Leave it alone for now.
- **Sandy addition:** also present on the right and resembles the left speaker.
  This remains the active investigation.

These are the user's listening observations. Direct agent listening is still
unavailable. Numerical analysis below does not claim to identify the subjective
sand uniquely, separate the two noises perfectly, or prove they have independent
causes. No crackle repair, wiring change or firmware change is proposed here.

The file is 69.8027 seconds of stereo AAC at 48 kHz. Decoded float peaks are
−4.6305 / −5.4677 dBFS, with no samples reaching full scale. The tone timing
matches the prepared right-only sequence at an offset of about **−1.07 s**:

| Signal | Lower pass in recording | Higher pass in recording |
| --- | --- | --- |
| 250 Hz | 5.93–10.93 s | 38.93–43.93 s |
| 1 kHz | 12.93–17.93 s | 45.93–50.93 s |
| Two-tone | 19.93–24.93 s | 52.93–57.93 s |
| Music | 26.93–36.93 s | 59.93–69.80 s captured |

All eight audible segments have useful coverage. Estimated sequence end is
69.93 s, about 0.13 s after the file ends; the steady-tone comparisons are
unaffected. The assumed image is the right-only +6 dB image from the preceding
handoff, consistent with the user/context and timing. Acoustic recording alone
does not independently verify which image/slot was active on the device.

### Comparison focused on sustained distortion

The new lower pass uses exactly the same digital waveform and level as the
previous left recording's higher pass: tones at nominal −30 dBFS, music peak
1,788 counts. Compare those passes, rather than comparing the files' differently
labelled lower passes. Each file's two channels are phone microphones, not the
radio's left/right output slots. Placement and microphone gain are uncalibrated.

Analysis uses each steady tone's middle three seconds, excluding fades and
transitions. Median-averaged Hann spectra (one-second windows, 50% overlap,
±4 Hz line bands) reduce brief broadband events' influence. Three separate
one-second spectra also check persistence. No samples are deleted and no
declicking, filtering or attempted noise separation is applied to the original.
Median spectra are an analysis aid, not proof that every crackle was excluded.

| Equal digital input / measured harmonic | Earlier left, phone mic 1 | New right, phone mic 1 | Digital reference |
| --- | ---: | ---: | ---: |
| 250 Hz / 750 Hz | −27.01 dB | −32.16 dB | below −90 dB |
| 250 Hz / 1,250 Hz | −31.62 dB | −23.94 dB | below −90 dB |
| 1 kHz / 3 kHz | −20.65 dB | −25.33 dB | below −90 dB |
| 1 kHz / 5 kHz | −24.49 dB | −23.94 dB | below −90 dB |

Ratios are relative to the recorded fundamental, not calibrated amplifier THD.
The reference's tiny quantization harmonics were recomputed with the same
method. The new right recording's second microphone likewise shows excess
harmonics: −21.36 dB at 3 kHz and −23.10 dB at 5 kHz during the lower 1 kHz tone.

The lines persist across the individual steady one-second windows, supporting
sustained signal-linked distortion rather than an explanation based solely on
a few brief crackles. Both sides have added harmonic structure well beyond the
digital source. **The right-only crackle is not needed to account for that
shared observation.** Exact harmonic ratios differ between recordings; that
does not establish identical faults or rank the two amplifiers' distortion,
given different speaker/room/microphone responses and possible recording gain
processing. The new higher pass also retains harmonics; this is not a clean
high-volume pass or evidence of a repair.

The active symptom is therefore the **shared sandy, signal-linked coloration**.
The right-only crackle remains separately recorded and deferred. The evidence
does not yet select between I2S interpretation/electrical behavior, amplifier
nonlinearity, speaker/enclosure behavior, or a common phone-recording effect.
It does not justify changing the production volume curve, firmware DSP or
another supply/capacitor experiment. No new flash or repeat recording is needed
just to establish that the symptom affects both speakers.

### Artifacts and checks

Ignored local directory `.pio/audio-sequence-right-recording-analysis/` contains:

- `recording.wav`: unnormalized float decode of the original.
- `compare.py`, `summary.json`: full parameters, alignment and per-microphone
  measurements, including one-second persistence checks.
- `same-level-comparison.png`: left/right/reference spectra at identical
  digital levels, normalized by each recording's fundamental.

Input SHA-256:
`9f3b7b085eb1dbfcae2968c69cb0bd8962e93821dd822085b85daeb4e6d9212e`.
Analysis reuses the existing local
Python environment and reference assets. The original file and firmware exports
remain untouched; only this investigation record and ignored analysis artifacts
are updated. `git diff --check` passes. No source/configuration changes, builds,
flashes or hardware measurements were performed for this recording analysis.

## Fourth recording: Mac control

The user supplied `/Users/avner/Downloads/Yefe Nof Street 4.m4a` after being
directed to play the right-only +6 dB sequence reference on the Mac. They report
much better sound and that the combined tone now sounds like two distinguishable
tones, unlike the radio. These are user listening observations; the agent's
comparison remains numerical, not direct listening.

The original was decoded locally with `afconvert` to float WAV without gain or
filtering. Duration is 71.0827 seconds; estimated sequence offset is +1.34 s.
All steady-tone interiors are captured; approximately 1.26 s of the final music
pass is missing. Decoded peaks are about −12.50 / −11.89 dBFS. The exact playback
volume, microphone gain/processing and acoustic placement are not calibrated.
The assumed reference is the supplied right-only +6 dB file, not independently
verified from the recording alone.

Analysis uses the same middle-three-second, median Welch method as the previous
comparison, with ±4 Hz bands and no audio alteration. All three recordings were
recomputed together. Two different comparisons must be distinguished:

- The Mac lower pass and right-radio lower pass have the same intended source
  samples as the original left-radio higher pass. Their recorded levels differ.
- The Mac lower pass and original left-radio lower pass have different intended
  digital levels, but closely matched recorded fundamentals on phone microphone
  1. This is useful for checking whether the phone inevitably creates the radio's
  large harmonics at that recorded level; it is not an amplifier gain comparison.

| Phone mic 1, lower passes | Original left radio | Mac control |
| --- | ---: | ---: |
| Recorded 1 kHz fundamental, integrated mean-square dBFS | −35.5 | −35.9 |
| 3 kHz / 1 kHz harmonic ratio | −21.5 dB | −73.4 dB |
| 5 kHz / 1 kHz harmonic ratio | −30.3 dB | −74.2 dB |
| Recorded 250 Hz fundamental, integrated mean-square dBFS | −38.3 | −38.5 |
| 750 Hz / 250 Hz harmonic ratio | −26.1 dB | −46.3 dB |

The 1 kHz third-harmonic contrast persists in separate one-second windows:
radio −21.62, −21.48, −21.15 dB; Mac −73.19, −71.03, −74.40 dB.
Both Mac microphone channels show much weaker 1 kHz harmonics than either radio
recording, at both sequence levels. Values near the recording floor should be
treated as measured band energy, not precise isolated harmonic amplitudes.

Both intended frequencies, 250 Hz and 1 kHz, are present in the radio two-tone
recordings as well as the Mac control. The radio did not simply lose one tone.
The Mac lower pass's 1 kHz/250 Hz ratio is +0.7 / −1.5 dB on its two microphones;
the right-radio lower pass is +4.6 / +7.7 dB, and the earlier left lower pass is
+3.9 / +9.5 dB. The radio recordings also contain substantially stronger extra
spectral lines. Different balance and added distortion are consistent with the
reported poorer perceptual separation, but neither is uniquely established as
its cause. These two frequencies remain harmonically related, so pure IMD cannot
be separated from harmonic distortion using this sequence.

This control substantially weakens an explanation based solely on unavoidable
phone distortion or the supplied source. It supports prioritizing the radio's
playback chain. It does not fully eliminate phone processing, differing acoustic
conditions, or frequency-response weighting, and it does not identify clipping,
a particular bit/format error, a failed amplifier or a speaker fault. Mac playback
is not a calibrated reference; its 250 Hz harmonics increase at the higher level,
and its measured level increments are not uniformly 6 dB.

No further recording of the same radio sequence is needed to re-establish this
contrast. A useful next boundary test is either the existing speaker driven by
an independently clean amplifier, or a properly filtered differential electrical
measurement of the MAX98357A output. No firmware or capacitor change is justified
by the control alone.

Artifacts: `.pio/audio-sequence-control-analysis/recording.wav`, `analyze.py`,
`summary.json`, and `control-comparison.png`. The plot uses matched intended
digital levels (Mac/right lower, old left higher); the table above deliberately
uses comparable recorded levels instead. Source SHA-256:
`95be81b634c33ac0a1e3b2afd71a661437ca90bd69e96a9612c35b287627e0e3`.
The comparison script was executed and its figure inspected. Original recordings,
existing firmware/source edits and exports were preserved. No build or flash was
performed because only analysis artifacts and this record changed.
