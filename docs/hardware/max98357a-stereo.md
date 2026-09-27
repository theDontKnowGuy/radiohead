# MAX98357A stereo speaker wiring

The Radiohead firmware already emits left and right samples. The normal audio
path configures Philips I2S in stereo mode, and the optional Spotify sink also
requires two-channel PCM. Adding the second speaker is therefore a wiring and
MAX98357A channel-selection change; it does not require a second ESP32 data pin
or a firmware downmix/upmix setting.

## Shared I2S bus

With power disconnected, wire both mono MAX98357A boards in parallel on the
digital side:

| ESP32-S3 | First amplifier | Second amplifier | Purpose |
| --- | --- | --- | --- |
| GPIO 15 | BCLK | BCLK | I2S bit clock |
| GPIO 17 | LRC/WS | LRC/WS | Left/right word select |
| GPIO 16 | DIN | DIN | Interleaved left and right audio data |
| GND | GND | GND | Common logic and power ground |
| Amplifier supply | VIN | VIN | Same regulated supply, sized for both amplifiers |

The MAX98357A does not need MCLK. Keep the three I2S branches short and route a
ground alongside them. Powering two amplifiers can roughly double peak supply
current, so confirm that the regulator, wiring, and supply are suitable before
testing at high volume.

Connect one speaker only to `SPK+` and `SPK-` on its own amplifier. Do the same
for the other speaker and amplifier. MAX98357A speaker outputs are bridge-tied:
neither speaker terminal is ground, and outputs from the two boards must never
be joined.

## Select one channel on each amplifier

`SD_MODE` voltage selects the mono signal produced by each MAX98357A:

| `SD_MODE` voltage, measured to GND | Output |
| ---: | --- |
| Below 0.16 V | Shutdown |
| 0.16 V to 0.77 V | `(left + right) / 2` mono mix |
| 0.77 V to 1.4 V | Right channel |
| Above 1.4 V | Left channel |

Configure the first amplifier above 1.4 V for left and the second between
0.77 V and 1.4 V for right. These voltage windows are the authoritative check.
Breakout boards differ: the common Adafruit-style board has a 1 Mohm pull-up
from `SD_MODE` to VIN in addition to the chip's internal 100 kohm pull-down, but
clone boards may use a different network. Do not assume that two boards at their
default setting form a stereo pair; defaults commonly produce the mono mix.

For a common 5 V Adafruit-style board, tying `SD_MODE` to VIN selects left. An
additional 1 Mohm pull-up from `SD_MODE` to VIN normally moves the onboard
divider into the right-channel window. Before connecting speakers or raising
volume, power the boards and verify each `SD_MODE` voltage with a multimeter.
Choose a resistor for the measured voltage rather than copying that value if the
board or supply differs.

## Device check

1. Start at low volume and play a known left/right channel test.
2. Confirm that the left prompt is heard only from the left speaker and the
   right prompt only from the right speaker.
3. If both prompts play from both speakers, recheck the two `SD_MODE` voltages.
4. If channels are reversed, swap the channel-selection configurations (or the
   complete speaker positions), not the bridge-tied speaker leads between amps.
5. Listen for clipping and check amplifier/regulator temperature before raising
   the normal listening level.

Firmware compilation verifies the stereo I2S configuration, but separation,
polarity, power integrity, and thermal behavior require this physical test.
