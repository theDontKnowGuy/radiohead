# TFT white-screen investigation

Audio hardware update, 2026-10-26: PCM5100 + new amplifier + the owner's
two-transistor mute circuit are installed; MAX98357A is permanently retired.
See the [current hardware record](current-audio-hardware.md). This installation
does not establish that the recorded TFT fault is resolved.

## User observations, 2026-10-16

- Display: 2.8-inch ILI9341 TFT; exact module PCB/revision not yet identified.
- Available instrument: Instrustar ISDS210A USB oscilloscope.
- The user needs beginner-friendly, step-by-step oscilloscope guidance.
- The display can work, then turn white following a tiny physical movement.
- Restart restores the picture most of the time.
- Audio continues and the web UI remains responsive while the screen is white.
  These are confirmed observations; do not repeatedly ask for them.
- The user reports replacing hardware and wiring except the TFT itself, and
  replacing the jumper connections twice. No spare TFT is available.
- Moving individual connections has not isolated a culprit.
- Encoder behavior is also unresolved; do not assume it shares the TFT fault.
- TFT jumper length and exact scope software/setup remain unknown.

## Interpretation and next measurements

Continuing audio and HTTP responses argue against a whole-device crash during
the observed failure. They do not rule out TFT-local power/reset interruption,
SPI corruption, a display-module fault, or display-specific software failure.
No cause is established. Initial scope observations are recorded below.

Prioritize capturing TFT power and reset at the display end during the event.
An intact waveform measured only after the failure does not exclude a transient.
Attach ordinary scope probe grounds only to circuit GND, never to either
amplifier speaker output (including the installed HX-A244). Use short ground
connections and avoid slipping between adjacent TFT header pins.

Repository inspection: reset is GPIO10; TFT SPI write clock inherits the pinned
LovyanGFX default of 16 MHz. The flashed image has not been verified in this
investigation. Current wiring is documented in `touch-spi-wiring.md`; TFT-only
colour-test firmware and `/api/device/encoder-debug` already exist.

Firmware experiment preparation is recorded below. Do not mark the fault
resolved based on a build.

## Guided scope observations, 2026-10-16

- Software visible in screenshots: Multi VirAnalyzer 3.13.12.0.
- Initial X10 captures showed approximately ten times the expected supply
  voltage despite the user confirming X10 on both probe and software. The
  discrepancy is unresolved; do not interpret those captures as actual rail
  overvoltage. A subsequent off-scale roughly 50 Hz trace also required checking
  probe contact. It does not establish noise on the TFT supply.
- With physical probe and software at X1, DC coupling, 2 V scale and 1 ms time
  setting, a VCC screenshot reported 4.785–5.066 V. These are displayed extrema,
  not an independently verified ripple measurement.
- CH1 Normal/falling-edge triggering at 4.5 V was verified by moving only the
  probe tip from VCC to GND. In Normal mode, "Wait" is an expected armed state;
  do not insist that the upper-left label must read "Run".
- The user then reproduced the white screen on VCC without a new capture.
  This is limited negative evidence for a 5 V input crossing below 4.5 V; it
  does not exclude faster missed events or faults on the module's internal rail.
- The user moved the probe to what they identified as RESET and observed
  captures during touch navigation to another screen. Screenshot settings:
  Normal, CH1, Fall Edge, 2.000 V trigger, 2 V scale, 10 ms time, 100 kSa/s.
  The cropped trace has a high-to-low transition followed by repeated brief
  higher intervals. Its zero marker and voltage extrema are not visible.
- Next: verify the physical RESET label/probe point (documented between CS and
  DC, connected to GPIO10), and whether these captures occur while the TFT
  continues displaying normally or coincide with white-screen failure. A capture
  on CS/DC could be ordinary transfer activity; a verified RESET assertion
  during normal navigation would be unexpected. Do not declare a reset fault
  solely from the cropped waveform.
- User subsequently confirmed that this capture was on **CS, not RESET**.
  The navigation-associated activity is consistent with display transfers;
  it is not evidence of an unexpected reset. Actual RESET measurement is next.
- On the actual RESET pin, the user reproduced the white screen without a
  trigger (requested setup: falling edge, 2 V). The idle level and a fresh
  trigger self-check on RESET were not separately reported. Treat this as no
  captured reset event, not proof that reset/power integrity is established.
- During testing, the user twice observed a negative-colour-looking image and
  supplied a photo with recognizable layout and altered colours. This is rare
  and was reported as a possible clue. Timing relative to probe movement,
  startup and white-screen failure is not yet established.
- ILI9341 has display-inversion commands 0x20/0x21; inversion changes displayed
  bits without modifying frame memory (datasheet pages 105–106:
  https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf). No explicit inversion
  calls were found in the inspected application display/state code. The photo
  alone cannot establish that an inversion command was received; SPI command
  corruption, controller state, internal supply/panel faults remain hypotheses.
- Before interpreting further signal captures, distinguish probe-induced
  disturbance from the original fault. The X10 attenuation discrepancy remains
  unresolved; X1 probe loading makes fast SPI waveform interpretation unsuitable
  without first validating the measurement setup. A reduced TFT write-clock
  experiment with otherwise identical firmware is a possible subsequent test.

## 4 MHz TFT write-clock experiment

The user asked to move on from the rare colour inversion and proceed with
diagnosis. `src/app_state.cpp` now explicitly sets `busConfig.freq_write` to
4,000,000 Hz, replacing the inherited 16 MHz default for this temporary test.
This is the only firmware source change. Touch stays at 1 MHz; no pin mapping,
inversion setting, persistence, or other application logic was changed.
Slower display transfers may affect redraw latency and servicing cadence;
note audio continuity during the test as well as display stability.

Verification: `pio run -e esp32s3` and `git diff --check` passed. Application
image: 4,514,288 bytes; free app-slot space: 2,039,312 bytes. Static DIRAM:
260,523 bytes (76.23%), 81,237 bytes free. Export manifests/hashes validate.

- Test artifacts: `.pio/build/esp32s3/`; OTA file: `firmware.bin`.
- Previous validated local build preserved before rebuilding at
  `.pio/build/before-tft-4mhz-test/` (app image 4,514,272 bytes). This backup is
  a local build, not a dump or verification of what was running on the device.
- No radio USB port was enumerated. `radio.local` resolved, but a read-only
  request to `/api/device` timed out. Current IP requested from the user.
- **Not flashed; hardware result pending.** After installation, disconnect
  scope probes and compare the same movement/touch navigation that previously
  caused white screens. Keep USB serial monitoring closed. A successful test
  supports, but does not prove, a communication-margin issue. Reproduction at
  4 MHz does not exclude signal integrity or intermittent connections.
