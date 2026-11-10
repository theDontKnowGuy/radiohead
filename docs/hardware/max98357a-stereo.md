# Retired MAX98357A notes

**Historical only. All MAX98357A boards were permanently removed on 2026-10-26.**
Build the [PCM5100/HX-A244 path](current-audio-hardware.md) instead.

The old two-mono-board arrangement shared Philips stereo I2S BCLK/LRC/DIN on
GPIO39/41/40 and common GND/supply. It replaced old 15/17/16 pins; the stale Spotify
assignment was corrected to shared `include/app_audio_pins.h` during the
[static investigation](audio-static-diagnosis.md). Build passed, physical separation
was not established there. External JTAG conflicts with 39–41; USB uses 19/20.

Old `SD_MODE` windows, relative to each amp's GND:

| Voltage | Mode |
| --- | --- |
| <0.16 V | Shutdown |
| 0.16–0.77 V | Mono (L+R)/2 |
| 0.77–1.4 V | Right |
| >1.4 V | Left |

Clone pull-up networks differed, so resistor recipes did not establish channel
selection without voltage measurement. Default boards could both output mono.
Each board drove only its own speaker; bridge-tied negatives were never ground
and could not be joined. Existing listening/scope evidence applies only to that
retired assembly and does not validate the current DAC/amplifier.
