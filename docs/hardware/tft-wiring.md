# Wire the display and touch

Use a **2.8-inch ILI9341 SPI TFT + XPT2046 resistive touch**, 320×240 landscape.
Disconnect power. GPIO numbers below match `include/app_state.h` and `src/app_state.cpp`.

## Complete header wiring, in pin order

| Header label | Connection |
| --- | --- |
| VCC | Owner's panel uses 5 V; verify a replacement panel's rating |
| GND | Common GND |
| CS / RESET / DC | GPIO8 / GPIO10 / GPIO9 |
| SDI(MOSI) | GPIO11, joined to T_DIN |
| SCK | GPIO12, joined to T_CLK |
| LED | GPIO1, subject to backlight note below |
| SDO(MISO) | Unconnected |
| T_CLK / T_CS / T_DIN / T_DO | GPIO12 / GPIO14 / GPIO11 / GPIO5 |
| T_IRQ | Unconnected; touch is polled |

Match printed labels; the opposite-side view is mirrored. Signals are **3.3 V logic**
even with 5 V module power. SPI2 is shared: clock 12, MOSI 11, touch-only MISO 5.
Keep CS and T_CS separate. Touch runs at 1 MHz. Optional microSD pins are unused.
Raw X+/X−/Y+/Y− terminals cannot replace an XPT2046 controller.

GPIO1 supplies active-high 5 kHz brightness/dimming/sleep PWM. Connect directly
only to a 3.3 V-compatible control input. If LED is a backlight power terminal,
use a suitable current-limited driver with active-high PWM; a GPIO is not a lamp supply.
Tying LED to supply bypasses dimming and sleep shutdown.

Encoder A/B/SW are 4/7/13, replacing the old touch 4/7 connections; backlight moved 3→1.
GPIO39–41 now carry audio. See [the remap record](touch-spi-wiring.md).

## First checks

1. Verify power/ground and the two clock/data joins with power off.
2. For backlight but no picture, try the [TFT color test](../../experiments/tft-color-test/README.md).
   It temporarily replaces the radio; restore the normal firmware afterward.
3. Open **Settings → Device → Touch calibration** and tap/release four corners.
   Calibration is saved in NVS; missing data uses defaults. Recalibrate after changing panels/orientation.
4. Compare ten comfortable finger-pad taps with ten gentle nail taps at center/edges,
   lifting at least 0.5 s. Count attempts and actual actions separately.
5. If nail works but light pad fails, inspect shipping film, enclosure pressure and
   connections; change one thing at a time. Do not peel the bonded touch layer.

Use [browser touch diagnostics/recovery](../ui/touch-diagnosis.md); opening USB
serial resets this radio. SDO and IRQ connections have no established sensitivity benefit.
Build/host checks passed for shared SPI/remapping on 2026-10-13; comfortable finger
operation and complete physical acceptance remain open.
