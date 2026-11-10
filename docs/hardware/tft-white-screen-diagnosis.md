# Screen turns white when moved

**Confirmed 2026-10-16:** the 2.8-inch ILI9341 can turn white after tiny movement;
audio continues and web stays responsive. Restart usually restores it. Wiring/
other hardware were replaced (jumpers twice), but no spare TFT exists. Moving
individual wires has not isolated a cause. Encoder trouble may be separate.
The later PCM5100/HX-A244 installation does not establish a TFT fix.

## Try next

Use the Instrustar ISDS210A with Multi VirAnalyzer 3.13.12.0, one measurement at
a time. Scope ground clips go only to common GND, **never either speaker terminal**.
Keep the ground short and probe the display end without bridging adjacent pins.

1. Validate probe scaling on a known DC rail first. X10 probe/software readings
   disagreed by roughly 10×; that is unresolved. X1 is unsuitable for judging fast SPI edges.
2. Watch TFT VCC during the movement failure: X1/DC, 2 V/div, 1 ms,
   Normal/falling trigger at 4.5 V. Briefly move the tip to GND to verify triggering.
   “Wait” in Normal mode means armed. Capture at failure, not only afterward.
3. Repeat on actual RESET/GPIO10, falling trigger 2 V; verify idle level and trigger
   self-test. RESET is between CS and DC. The earlier navigation trace was **CS**,
   not RESET, and is not evidence of a reset fault.
4. If measuring internal panel rails, identify them first. Clean input VCC alone
   cannot exclude internal power/controller/SPI problems.

## Results so far

- X1 VCC displayed 4.785–5.066 V. Failure produced no new 4.5 V trigger;
  limited negative evidence, not proof of supply integrity.
- Actual RESET also failed to trigger during white-screen reproduction, but idle
  level/self-check were not separately recorded. No captured reset event.
- Rare negative-looking colours occurred; ILI9341 supports inversion commands
  0x20/0x21, but no explicit application inversion call was found. The photo does
  not prove command corruption. Probe effects, SPI, local rails or panel remain possible.

## 4 MHz experiment

A temporary `busConfig.freq_write = 4000000` replaced LovyanGFX's inherited 16 MHz;
touch stayed 1 MHz, with no pin/persistence changes. Build/diff and artifact checks
passed: app 4,514,288 B, DIRAM 260,523 B. Test export `.pio/build/esp32s3/`;
previous local export `.pio/build/before-tft-4mhz-test/` (not a dump of the device).
No flash/test was established in this record; USB was unavailable and HTTP timed out.

After installing a known image, remove probes and repeat the same movement/navigation
with serial closed. Note picture and audio continuity. Improvement supports a
communication-margin hypothesis; either result alone does not identify the fault.
See [wiring](tft-wiring.md) and [color test](../../experiments/tft-color-test/README.md).
