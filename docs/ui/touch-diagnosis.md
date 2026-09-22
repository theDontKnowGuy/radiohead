# Finger-pad touch acceptance

Normal, light finger-pad taps must reliably operate the radio. The September 22
report means acceptance is **failed**, regardless of earlier build/upload passes.

The earlier 250 kHz acquisition, same-axis settling, pressure-gate bypass,
largest-cluster filter and immediate first-contact actions remain in source.
There is no demonstrated rollback of those changes. This does not identify the
firmware currently on the device or exclude latency elsewhere in the main loop.

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
