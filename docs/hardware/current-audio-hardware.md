# Current audio hardware

Confirmed 2026-10-26: **PCM5100 DAC → HX-A244 stereo amplifier → Dayton PC83-4
speakers**, plus the owner's installed two-transistor mute circuit.
MAX98357A boards were permanently removed.

| Signal | ESP32-S3 GPIO |
| --- | --- |
| I2S BCK / DIN / LRCK | 39 / 40 / 41 |
| Speaker mute | 2: HIGH silent, LOW sound |

Local playback and Spotify share `include/app_audio_pins.h`.
See [audio wiring](audio-wiring.md) and [mute behavior](speaker-mute.md).

The red DAC was previously labelled PCM5102/PCM5102A in photo notes; the owner
calls it PCM5100. Exact silicon, VIN rating and jumper states remain unverified.
The mute schematic, transistor values and reset-time behavior are also undocumented.
Do not copy a relay or DAC-XSMT circuit on the strength of the transistor count.

Installation is confirmed; new channel/noise/cold-boot/mute/sleep checks are not.
The amplifier reference uses **12 V**. The proposed 5 V battery system does not
yet supply or shut down the whole assembly.
