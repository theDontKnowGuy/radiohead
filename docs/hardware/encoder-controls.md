# Encoder wiring and use

Power off before wiring. Numbers are GPIOs, not board header positions.

| Encoder | ESP32-S3 |
| --- | --- |
| VCC | 3.3 V; never 5 V |
| GND | GND |
| CLK/A / DT/B / SW | GPIO4 / GPIO7 / GPIO13 |

A bare encoder needs no VCC: common and one switch contact go to GND,
A/B to 4/7 and the other switch contact to 13. Firmware enables pull-ups.
Leave optional K0/GPIO6 disconnected; never bridge 6 and 13.

- Turn: volume; click/release: mute without restarting playback.
- Hold 3 s: light sleep. Release, then press again to wake.
- Hold during power-on/reset: Wi-Fi recovery; release after setup starts.
  This does not start touch calibration.

The 2026-10-13 remap moved A/B/SW from 39/40/38 to 4/7/13 and backlight 3→1.
Old touch clock/data on 4/7 must move to shared 12/11 first. GPIO35–37 are PSRAM;
39–41 now carry audio. [Remap diagnostics](touch-spi-wiring.md).

## Checking missed clicks

While a station is playing at a nonzero volume, open
`http://radio.local/api/device/encoder-debug` using the displayed IP if needed.
Read the counters, click once, then refresh. Repeat while loading the web UI
or switching stations. A normal click adds one to `buttonPresses`,
`buttonClicks` and `buttonClicksConsumed`. The web player's `muted` state at
`/api/player` should toggle each time.

- `rawSwitch` is 1 when released and 0 when held. `buttonEdges` includes bounce.
- Edges without a debounced press point to a brief or unstable signal. If the
  edge count stays flat, check GPIO13 at the pin and look at `maxSampleGapMs` for
  long gaps between encoder task samples.
- A detected click can wait in `pendingButtonEvents` while the main loop is
  busy. `buttonEventsDropped` records a full event buffer. A consumed click
  with unchanged `muted` state points downstream of GPIO13.
- A 3-second hold adds one to `buttonHolds`; its release does not count as a click.

Current sleep is not electrical off. The [battery button interface](battery-power-architecture-plan.md)
is a different, untested wiring proposal. Physical remap/sleep acceptance remains pending.
