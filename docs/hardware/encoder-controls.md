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

Current sleep is not electrical off. The [battery button interface](battery-power-architecture-plan.md)
is a different, untested wiring proposal. Physical remap/sleep acceptance remains pending.
