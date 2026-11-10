# PCM5100 DAC + HX-A244 amplifier wiring

Current assembly: [installed hardware record](current-audio-hardware.md).

This is the installed audio path. The earlier wiring notes identified the DAC
module as PCM5102/PCM5102A; the owner now calls it PCM5100. The drawing below
retains the earlier board identification, without claiming the exact chip
variant has been verified. Disconnect all power before any wiring changes.
The sketch matches the **back of the owner's red DAC board**, photographed 2026-10-25:
six holes across the top, nine down the left, and the audio jack on the right.
Follow the printed labels when turning the board over; the view will be mirrored.

```text
                    PCM5102A — BACK VIEW, as in the photo
                  +---------------------------------------+
Top header:       |  o     o     o     o      o      o     |
                  | SCK   BCK   DIN   LRCK   GND    VIN    |
                  |                                       |
Left edge:        | o FLT     [solder configuration pads]   |
                  | o DEMP                                |
                  | o XSMT                                |
                  | o FMT                                 |
                  | o A3V3                                |
                  | o AGND                                |
                  | o ROUT                         [audio |
                  | o AGND                           jack]|
                  | o LROUT                               |
                  +---------------------------------------+

ESP32-S3 / supply             PCM5102A TOP HEADER
GND ------------------------ SCK     (master clock tied low)
GPIO 39 -------------------- BCK     (bit clock)
GPIO 40 -------------------- DIN     (audio data into DAC)
GPIO 41 -------------------- LRCK    (left/right clock)
GND ------------------------ GND
Verified module supply ----- VIN     (see power note below)

PCM5102A LEFT EDGE           HX-A244 ANALOG AUDIO INPUT
LROUT ---------------------- L / IN-L
ROUT ----------------------- R / IN-R
AGND ----------------------- GND     (either AGND hole)

12 V supply + -------------- HX-A244 DC+ / VCC
12 V supply - -------------- HX-A244 DC- / power GND
                                  |
ESP32 GND + DAC GND ---------------+  common ground

HX-A244 SPEAKER OUTPUTS
L+ ------------------------- Left Dayton PC83-4 +
L- ------------------------- Left Dayton PC83-4 -
R+ ------------------------- Right Dayton PC83-4 +
R- ------------------------- Right Dayton PC83-4 -
```

The GPIO assignments match [include/app_audio_pins.h](../../include/app_audio_pins.h).
`SCK` is **not** `BCK`: ground `SCK`, and connect only `BCK` to GPIO 39.
The bottom label **`LROUT` means the left analog output** on this board layout;
use it together with `ROUT` for stereo. The amplifier labels above describe
function, not physical terminal order; its connector layout has not been photographed.

**DAC power still needs confirmation from the component side or its seller's
schematic.** The back photo shows `VIN` and `A3V3`, but does not establish the
regulator or VIN rating. If this is the usual regulated **5 V VIN** version,
connect the radio's regulated 5 V supply to `VIN`. Do not apply 5 V on that
assumption alone. Leave `A3V3` externally unconnected in this VIN-powered setup;
it is the analog 3.3 V rail, not a second VIN. Never apply 12 V to any DAC or ESP32
pin. Keep the ESP32 on its existing supply.

## DAC configuration pads (earlier PCM5102A board reference)

These four left-edge control pins are normally configured by the board's solder
jumpers, so they need **no additional ESP32 GPIO wires** when set correctly:

| Printed control pin | Required state | Purpose |
| --- | --- | --- |
| `FLT` | Low / GND | Normal-latency filter |
| `DEMP` | Low / GND | De-emphasis off |
| `XSMT` | High / digital 3.3 V | Unmuted output |
| `FMT` | Low / GND | Philips I2S format |

Solder is visible on the photographed pads, but the selected bridges are not
confirmed. With power off, identify which jumper center is connected to each
control pin and which outer pad is GND or digital 3.3 V using continuity and the
board schematic. If a jumper must change, remove the old bridge before joining
the center to the required side. Never bridge both sides or wire `XSMT` to 3.3 V
while its jumper shorts it to GND. After confirming VIN and powering only the
DAC/radio, check the four control voltages relative to GND before connecting the
amplifier. The chip's pin functions and three-wire clock mode are documented in
the [TI PCM5102A datasheet](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf).

Keep I2S wires short and route ground alongside them. Run the amplifier's power
return directly to its 12 V supply, not through the DAC's thin ground wire.
Connect one 4-ohm Dayton PC83-4 per channel. **Neither speaker negative is ground;
do not join the speaker negatives.** These are bridge-tied outputs on the
[TPA3110D2](https://www.ti.com/product/TPA3110D2).

The DAC/amplifier and two-transistor speaker-mute circuit are installed. The
report does not supply new playback or electrical test results. Check left/right
separation at low volume and flat bass EQ, then startup/mute behavior and noise;
see the [mute acceptance checks](speaker-mute.md).
