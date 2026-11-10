# Touch stops responding or misses taps

**Observed:** touch fails or shifts until reboot while picture/encoder/web continue.
Computer USB present at boot usually helps; connecting it later usually does not.
That suggests investigating boot power/initialization, but proves no electrical cause.
Normal external power is regulated 5 V through two wires. Verify the actual board's
power isolation before simultaneous USB/5 V use; compare one supply at a time.

## Collect before rebooting

Keep USB serial closed: opening it resets this radio.
Open **Device & maintenance → Open diagnostics** in the browser.

1. At a fixed target, count comfortable light finger-pad attempts/actions; compare
   gentle nail taps, lifting at least 0.5 s. Record raw/screen coordinates and pressure.
2. Repeat immediately when it fails, before restarting.
3. Choose **Recover touch**, lift fully, repeat at center/edges. Note uptime so a
   reboot cannot be mistaken for touch-only recovery.
4. Compare external-only bad boot → recovery → software restart, then separate
   cold boots on computer USB and USB charger. Keep firmware/location/load constant.
5. For electrical checks, scope local 5 V/3.3 V/display supply and return paths
   during failure; inspect film, enclosure pressure and connections one change at a time.
   Do not peel the bonded touch layer or accept nail-only operation as finished.

## What the diagnostics mean

Active reader: instrumented **stock LovyanGFX XPT2046, shared SPI2, 1 MHz**,
seven Y/Z1/X/Z2 sequences, original medians/bounds/pressure gate. Host comparison
of 20,042 frames matched stock behavior. The old pressure-bypass sampler is inactive.
Normal diagnostics add no extra SPI reads or serial prints; browser polls only while visible.

GET `/api/device/touch-debug`:

| Field | Use |
| --- | --- |
| `touchAdc` | Acquired/accepted/pressure-rejected counts; pressure score/conversions; per-axis min/max/median/count and largest group within 150 |
| Rail/sparse/unstable counts | Stock range `128 < value <= 3968`: zero / 1–2 / ≥3 without three within 150. Grouping reports quality, does not reject stock-accepted taps |
| `touchTiming` | Latest/max poll gap/read duration, press/releases and off-screen mappings; maxima since boot, not percentiles |
| `touchRecovery` | Attempts/init result, age/duration and retained pre-recovery `beforeAdc` |

Calibration and rejected idle reads contribute counts; compare labelled contact
trials and deltas, not lifetime totals. Idle rejection alone is normal.
POST `/api/device/touch-recover` waits for display transfers, reinitializes touch
without freeing TFT bus/DMA, preserves calibration, and consumes contact until
80 ms release (including off-screen raw contact). It neither resets TFT/reboots,
saves settings nor runs periodically. Calibration/OTA/restart conflict: 409;
detected bus-init failure: 500. Bus availability is not a sensor acknowledgment.

## Separate acquisition from UI behavior

- Light taps look like untouched rails but nail works: check panel actuation,
  film, mounting and wires; not proof of one component's cause.
- Sparse/noisy contact unlike idle: investigate acquisition alongside false touches.
- Raw accepted but off-panel: calibration/mapping.
- On-panel accepted but no action: target ownership, wake consumption or loop delay.

Older 250 kHz/settling/pressure-bypass trials did not establish comfortable-pad
acceptance. Their serial `TouchADC` capture path is historical, not the current
web reader. Resistive pressure cannot be inferred from coordinate counters.
After any fix, test edges/rapid taps/held contact across pages/dim-wake, at least
60 s untouched and audio continuity. If the panel cannot meet comfortable taps,
a capacitive replacement needs its own controller/wiring decision.
[Current wiring](../hardware/tft-wiring.md). The intermittent fault remains unproven as fixed.
