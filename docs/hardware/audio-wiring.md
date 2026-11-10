# Wire the DAC and amplifier

Disconnect power. [Current assembly](current-audio-hardware.md): PCM5100/HX-A244.
The red DAC was previously called PCM5102A; verify chip/VIN rating. Match printed labels.

Back-view top holes: **SCK, BCK, DIN, LRCK, GND, VIN**;
left edge: **FLT, DEMP, XSMT, FMT, A3V3, AGND, ROUT, AGND, LROUT**;
audio jack at right. `LROUT` is the left analog output.

| From | To |
| --- | --- |
| ESP32 GPIO39 / GPIO40 / GPIO41 | DAC BCK / DIN / LRCK |
| Common GND | DAC SCK and GND |
| Verified module supply | DAC VIN |
| DAC LROUT / ROUT / AGND | HX-A244 L input / R input / signal GND |
| 12 V supply + / − | HX-A244 DC+ / DC− |
| Common ESP32/DAC GND | Amplifier power GND |
| HX-A244 L+ / L− | Left 4 Ω Dayton PC83-4 + / − |
| HX-A244 R+ / R− | Right 4 Ω Dayton PC83-4 + / − |

`SCK` is not `BCK`: ground SCK. Amplifier labels describe function, not terminal order. Keep I2S short with a ground alongside.
Run amplifier power return directly to its supply, not through a thin DAC ground wire.
**Neither speaker negative is ground; never join them.**

## Power and solder pads

Confirm the module regulator/VIN rating from its component side or schematic.
Use 5 V VIN only if this is the regulated 5 V version. Leave A3V3 unconnected in
that arrangement; never feed 12 V into the DAC or ESP32.

| Control | Required state |
| --- | --- |
| FLT / DEMP / FMT | GND: normal filter / de-emphasis off / Philips I2S |
| XSMT | Digital 3.3 V: DAC unmuted |

With power off, trace each jumper center to its control pin and identify the GND/
3.3 V side. Remove an old bridge before changing it; never bridge both sides or
feed XSMT high against a GND jumper. Power DAC/radio alone and check control voltages
before connecting the amplifier. [TI pin reference](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf).

Start at low volume with flat bass. Check left/right separation, noise, startup
and [GPIO2 speaker mute](speaker-mute.md). Installation does not verify these tests.
