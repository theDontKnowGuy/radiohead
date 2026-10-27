# Installed audio hardware

Updated 2026-10-26 from the owner's installation report.

## Current assembly

- ESP32-S3 stereo I2S feeds the **PCM5100 DAC**, then the **HX-A244 stereo amplifier**
  and the speakers. The owner confirmed the amplifier model on 2026-10-26;
  earlier notes transposed it as XH-A244. The speakers are recorded as Dayton PC83-4.
- The owner's **two-transistor speaker-mute circuit is installed**.
- **All MAX98357A amplifier boards are removed and permanently retired.** Do
  not propose reinstalling them, buying replacements, or continuing their
  SD_MODE/channel-selection troubleshooting.

The owner calls the installed DAC PCM5100. The 2026-10-25 photo-based wiring
notes called the red module PCM5102/PCM5102A. Use PCM5100 for the current assembly;
retain those earlier labels only as provenance for the board drawing and pad
reference. The exact silicon marking, module supply rating and jumper states
are not established by this installation report.

## Signal and mute interfaces

| Interface | Current firmware assignment |
| --- | --- |
| I2S BCK | GPIO39 |
| I2S data from ESP32 to DAC DIN | GPIO40 |
| I2S LRCK / WS | GPIO41 |
| External speaker mute | GPIO2: HIGH = silent, LOW = sound enabled |

I2S pins are shared by local playback and Spotify through
[`app_audio_pins.h`](../../include/app_audio_pins.h). The analog DAC outputs feed
left/right amplifier inputs. The amplifier drives the speakers.
See the [board wiring reference](../../README.md#pcm5100-dac--hx-a244-amplifier-wiring)
and the [speaker-mute contract](speaker-mute.md).

GPIO2's firmware contract includes a minimum four-second startup mute and
playback-state gating. Installation of the two-transistor circuit is confirmed;
its transistor types, component values, connection schematic and reset-time
behavior have not been documented here. Do not infer a relay topology or a
connection to DAC XSMT or an amplifier shutdown pin from the transistor count.

## Evidence and remaining checks

The installation report establishes which hardware is present. It does not
establish a new firmware flash, noise-free playback, channel separation, or
successful cold-boot/mute/sleep tests. Earlier MAX98357A listening and scope
results remain historical and do not validate this assembly.

The existing amplifier wiring reference uses a separate 12 V supply. The
battery proposal's former assumption that two amplifiers share the radio's
5 V rail is obsolete; amplifier power and whole-system shutdown must be
revisited for this assembly before battery implementation.
