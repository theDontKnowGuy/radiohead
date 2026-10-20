# iPhone recording of diagnostic music — 2026-10-19

## Scope and listening limitation

User supplied `/Users/avner/Downloads/Yefe Nof Street.m4a` and asked to inspect
the beginning only for the sandy/dirty sound. User confirms this is the
diagnostic music loop. The model's available audio-content tool returned
`audio content omitted because you do not support audio input`; **no direct
listening judgment was possible**. Findings below are numerical signal analysis,
not a claim that the agent heard, reproduced or recognized the subjective dirt.

Input SHA-256:
`ae79d3a143ce7bcec2502224b968f5ca5c141d55cd2d153a2dc5110cafac9ee9`.
The file is stereo AAC, 48 kHz, approximately 16.21 s. It was decoded locally
with macOS `afconvert` to signed 16-bit PCM. Analysis is confined to 0–6 s;
the source comparison uses the audible opening passage, principally 0.85–3.15 s.
Later contents were not analyzed. No audio was uploaded to another service.

## Measurements

- Opening decoded peak: −17.11 dBFS (right microphone channel); no samples reach
  either full-scale rail. This excludes decoded full-scale sample saturation in
  the excerpt, **not** analog clipping, phone processing or amplifier clipping.
- The exact flash fixture's **left source channel** matches the recording.
  Bandpassed two-second template matches have absolute correlations around
  0.79–0.82, with a reference offset approximately +2.6093 s. The left source
  fits better than the right or stereo average, consistent with the current
  channel profile copying the left source onto active slots.
- A regularized 481-tap linear acoustic filter at 12 kHz (40.08 ms support),
  fitted on alternating guarded time segments and evaluated on held-out
  segments, yields correlation **0.9940** and normalized error power
  **−19.12 dB** on the held-out samples. This indicates that the dominant
  waveform is strongly consistent with filtered source music. It does not
  establish that a weaker audible artifact is absent.
- That residual is **not THD, THD+N, a distortion percentage, or isolated sand**.
  It includes room response outside the model, clock/interpolation mismatch,
  microphone/codec processing, noise and any true distortion. No calibrated
  electrical or clean acoustic reference was captured with the same phone.
- In the aligned 0.85–3.15 s passage, recorded 40–100 Hz band power is about
  **17.66 dB lower relative to the 200–500 Hz band** than the clean source.
  The 2–4 kHz band is about 4.73 dB lower on the same normalization. Speaker,
  enclosure/baffle, placement, room and microphone response are confounded;
  this is not evidence of a specific bad amplifier or a firmware EQ fault.
- Dominant spectral peaks follow source musical notes (about 130, 174, 221,
  262 and 349 Hz), rather than identifying a distinctive mains-hum signature.
  No unique hum, click, bit-error or dropout cause is established by this clip.
  Short periodic template matches can select neighboring musical periods;
  they are not measurements of sample-clock jitter or individual dropped frames.

## What the earlier tests and this recording support

The subjective dirt persists with direct flash PCM and no production decoder,
Wi-Fi or display initialization, while runtime clipping/write/deadline/inferred
gap counters remain zero. Both widths and isolated active channels sounded
equally dirty. The exact reference WAV sounds clean on the user's computer.
Changing the external supply and adding the suggested capacitors on one board
did not audibly improve the result. These observations reduce the case for a
normal-app-only bug, source-file defect or a fault requiring two active speakers.
They do not prove the full software/digital transport correct or power fault-free.

The recording supplies no strong evidence of gross sample corruption and no
unique signature that selects between electrical I2S receiver behavior, common
amplifier-board behavior, local power/return effects and acoustic hardware. A
large linear component also cannot exclude lower-level signal-dependent grit.
Calling the boards counterfeit, the speakers damaged, or the supply inadequate
would go beyond the evidence. No corrective firmware patch is justified here.

One useful scale check comes from the already logged diagnostic peak of 2,364
counts (−22.8358 dBFS), independently of the phone's gain. For a correctly
operating MAX98357A at the reported default **9 dB** gain, the manufacturer's
full-scale DAC reference of **2.1 dBV** implies approximately **0.366 V peak**
at that sample amplitude: `sqrt(2) * 10^((−22.8358 + 2.1 + 9)/20)`.
Even 15 dB gain gives about 0.731 V peak. These are conditional nominal
calculations, not output measurements. Ordinary maximum-power clipping into a
normal 4-ohm load is consequently a weak explanation for the logged low-level
test, unless actual hardware, loading, supply or gain differs materially.
The sine-equivalent power at the peak is not the music's average power.

Primary references:

- [MAX98357A/MAX98357B data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf),
  gain equation and Table 8, printed page 28. MAX98357A uses I2S; B uses
  left-justified input, so board lettering alone does not authenticate silicon.
- [ESP-IDF 5.5.5 I2S documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/i2s.html).
  The diagnostic uses the installed S3 Philips-stereo defaults: one-bit shift,
  MSB-first, 16/32-bit slots with correctly packed samples. Driver-accepted
  samples and inferred write gaps do not measure receiver electrical margins.

## Correction to the spare-component proposal

The user points out that the existing two speaker/amplifier pairs already sound
the same and questions the value of swapping. Exchanging those equally suspect
parts would not create an independently clean reference. The earlier answer
"Both" must not be treated as proof of a third, independently verified clean
speaker/amp pair; additional spare availability and quality are unestablished.
The prior substitution was proposed, not performed. No substitution result is
recorded. No further rewiring, purchase, repeat capacitor trial, firmware flash
or scope-capture loop is requested by this analysis.

## Local analysis artifacts

Generated files remain outside source control in the ignored directory
`.pio/audio-only-recording-analysis/`:

- `opening-analysis.png`: opening waveform, spectrogram and initial spectrum.
- `aligned-comparison.png`: aligned source/acoustic-model comparison.
- `summary.json`, `aligned-summary.json`: numerical measurements and parameters.
- `analyze_recording.py`, `compare_aligned.py`: local analysis scripts.

Dependencies were installed into the isolated ignored
`.pio/audio-analysis-venv/`: NumPy 2.4.6, SciPy 1.17.1, Matplotlib 3.11.2.
Firmware source, exported images and device hardware were unchanged. No build
or device flash was needed for this recording analysis.
