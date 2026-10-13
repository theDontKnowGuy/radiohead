# Touch diagnosis and recovery

## 2026-10-12: intermittent touch failure

### Clarified user observation: computer USB during boot usually helps

The radio normally receives regulated 5 V through two wires supplying the
assembly. The user clarifies that the USB cable connects to a **computer**:
booting with it connected usually gives working touch, while attaching it to an
already-running board usually does **not** recover touch. This supersedes the
earlier interpretation that USB restores touch immediately. It is user-observed
evidence, not a controlled measurement. Power-ramp/local-ground conditions at
boot and touch-bus/controller initialization deserve investigation; ongoing
power noise and USB-host-dependent execution are still possible. It does not
establish that extra grounding or periodic reset is a cure.

Startup inspection: `setup()` initializes display/touch near the start, before
Wi-Fi and normal playback; `initializeDisplay()` has a 50 ms delay, and LovyanGFX
initializes touch after the panel. There is no application-level wait for USB
connection or branch selecting a different touch setup. This does not exclude
framework USB timing or differences in the actual supply ramp.

Useful next trials: from external-power-only startup, try **Recover touch** on a
bad boot, then a web software restart with external power unchanged. Compare
fully powered-down boots with computer USB versus a correctly wired USB charger
supply alone. Keep workload/tap location and time since boot comparable and note
whether the fault is present immediately or develops later. These trials remain
unperformed; they distinguish touch-only reinitialization, MCU reboot, computer
USB presence and supply conditions without choosing a fix prematurely.

Check Device uptime before/after the event to distinguish reset from recovery
within the same boot. For supply comparisons, test external supply only versus
USB supply only, holding firmware/wiring/audio/tap location constant. A regulated
source's label or unloaded reading does not establish clean voltage at the TFT
or ESP32 under load. Measure the 5 V input, ESP32 3.3 V and display/touch supply
relative to their local grounds; a scope can expose dips/ripple missed by a meter.
Inspect shared ground returns and power distribution, including amplifier loads.
Do not infer that additional grounding or capacitors are the confirmed cure.

The documented ESP32-S3-DevKitC-1 power options are mutually exclusive;
simultaneous USB/5 V input needs verification against the **actual** board's
power isolation. See [the Espressif power-supply instructions](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#power-supply-options).

### Current diagnostic firmware

The user reports that touch stops responding or returns displaced coordinates
until reboot, while the picture, encoder and web continue working. This narrows
the observed symptom to touch acquisition/input; it does not identify a wire,
prove an electrical cause or establish a software fix.

The active reader is now an instrumented version of the pinned **stock**
LovyanGFX XPT2046 reader: shared SPI2, **1 MHz**, seven Y/Z1/X/Z2 conversion sequences,
the same ADC bounds/medians and pressure gate. The older `LightTouchXPT2046`
pressure-bypass reader remains in source but is inactive. Host comparison of
20,042 frames confirms identical wire commands, acceptance, coordinates and
pressure results. This does not reproduce physical panel behavior.

Open **Device & maintenance → Open diagnostics** in the normal web UI. ADC
diagnostics run without enabling the red marker; browser polling runs only while
the panel is open and the tab is visible. No additional SPI reads or Serial
writes are needed. Keep the USB serial monitor closed: opening it resets this
radio and could erase the failure we are trying to inspect.

1. After boot, record comfortable taps at one fixed location, including attempted
   taps/actions and the reported raw/screen coordinates, X/Y groups and pressure.
2. When the failure occurs, record the same observations **before rebooting**.
3. Click **Recover touch**, lift your finger for at least half a second, then
   repeat the same taps. Record whether accuracy and normal actions return.
4. Repeat center/edges, dim-wake, repeated quick taps and untouched idle periods,
   checking audio and web/encoder operation. A successful bus initialization is
   not evidence that the intermittent fault was recovered.

The existing `GET /api/device/touch-debug` also exposes:

- `touchAdc`: acquisition/accepted counts, pressure rejections with valid X/Y,
  latest stock pressure score and number of positive pressure conversions.
  Each axis reports its last minimum/maximum, in-range median/count, largest
  group within 150 counts, and cumulative rail/sparse/unstable-frame counts.
  Rail means no values in the stock range (128 < value <= 3968); sparse means
  one or two; unstable means at least three but no group of three within 150.
  The grouping is diagnostic only: it does **not** reject a stock-accepted tap.
- `touchTiming`: latest/maximum poll gap and read duration, press/release counts,
  and mapped off-screen sample count. Maxima are since boot, not percentiles.
- `touchRecovery`: attempts/initializations, age/duration and result of the last
  attempt, plus a retained `beforeAdc` snapshot. Counters include calibration
  reads and rejected idle readings. Idle rejection/instability alone is not a
  fault; compare labeled contact trials and counter differences.

`POST /api/device/touch-recover` reinitializes touch on the shared SPI2 bus without
freeing the TFT's bus or DMA resources. It waits for display transfers and suspends
any active display transaction during initialization, retains the panel's current
calibration/mapping and clears press ownership by consuming
contact until an 80 ms release. Off-panel accepted raw contact also remains
consumed. The endpoint excludes calibration, OTA/release maintenance and pending
restart, returning 409 for those conflicts and 500 on detected bus-init failure.
It does not reset the TFT, reboot the radio, save Preferences or run periodically.
The init check verifies SPI bus availability/capacity, not sensor acknowledgment.

Software cannot distinguish a bad clock/data/CS wire from analog panel or supply
noise solely from ADC quality. Electrical diagnosis still needs scope/logic
analyzer evidence; use the [current wiring](../hardware/touch-spi-wiring.md).

## Historical finger-pad acceptance and serial experiments

Normal, light finger-pad taps must reliably operate the radio. The September 22
report means acceptance is **failed**, regardless of earlier build/upload passes.

The earlier 250 kHz, same-axis settling, pressure-bypass and cluster experiments
below describe September revisions. Their custom sampler is retained but is
inactive in the current stock-reader build. Use the web protocol above for the
current image; the serial ADC summaries below observe only the old custom reader.

## Measure before changing another threshold

Build with `-DTOUCH_DEBUG_ENABLED=1` in PlatformIO's build flags, and install that
build on the identified radio. Normal firmware leaves diagnostics disabled.
Complete startup/calibration first. Keep audio playing and compare four separate
15-second captures: untouched, fingernail, comfortable light finger pad, and firm
finger pad. Use the same screen location (a non-action area initially); lift for
at least half a second between taps. Record attempted taps and observed actions
separately; neither counter alone proves that an intended button worked.

Example, using the current port from `pio device list` and Python with pyserial:

```sh
python3 tools/capture_touch_diagnostics.py --port /dev/cu.usbmodem2101 --label idle --seconds 15 --output .pio/touch-trials.jsonl
```

Repeat with `--label nail`, `--label light-pad`, and `--label firm-pad` while
performing those actions. The capture saves only the two touch-report prefixes.
It appends phase labels and host-relative timestamps, never other serial logs.

`TouchADC` observes all production frames, before the coordinate filter:

- `frames` / `accepted`: all acquisitions / both axes accepted.
- `rail_xy`: frames with no in-range post-settling readings, separately X/Y.
- `sparse_xy`: only one in-range reading; two are required.
- `noise_xy`: at least two in-range readings but none within the 150-count span.
- `last_x` / `last_y`: last frame's minimum:maximum/valid-count/cluster-count.
  These are raw ADC coordinates, **not pressure**. The last frame is a snapshot;
  use the full-window counters when interpreting short taps.

`Touch` reports accepted raw contacts, mapped on-panel contacts, begins/releases,
maximum polling gap and read duration. Its `raw` field is **after** filtering.
USB output is bounded and skipped/deferred if its buffer lacks room; no extra
SPI reads or waits are introduced. Calibration reads can contribute ADC frames
without normal polling events, so do not measure during calibration.

Interpret the paired trials:

- Light-pad readings resemble untouched rails, but nail/firm contact is accepted:
  investigate panel actuation, protective film, mounting pressure and connections.
  This pattern suggests a hardware/contact limit; it does not prove its cause.
- Light-pad contact produces sparse/noisy readings unlike idle: use that evidence
  to test acquisition/filter changes, checking idle false activations alongside.
- Raw acceptance with low on-panel acceptance: investigate calibration/mapping.
- On-panel acceptance with missed actions: investigate targets, press ownership,
  wake consumption and main-loop latency rather than electrical sensitivity.

The [XPT2046 datasheet](https://files.waveshare.com/wiki/common/XPT2046_Datasheet.pdf)
describes the resistive controller and settling/pressure measurements. Software
cannot guarantee capacitive-style actuation from a mechanically resistive panel.
If this panel cannot meet comfortable finger-pad operation, evaluate a capacitive
display module with confirmed wiring/controller support instead of accepting a
nail-only interaction as finished.

After a measured correction, repeat center and edge targets, rapid taps, a held
finger across page changes, wake-only touch, at least 60 seconds untouched, and
audio continuity. Record results; a successful compile does not close this issue.
