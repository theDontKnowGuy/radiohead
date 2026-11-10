# Encoder wiring and controls

The radio has **one physical button: the encoder's push switch**. Disconnect power
before wiring. These are ESP32-S3 GPIO numbers, not physical header positions.

| Encoder module pin | ESP32-S3 connection |
| --- | --- |
| `+` / `VCC` | **3.3 V**, not 5 V |
| `GND` | Common GND |
| `CLK` / `A` | GPIO 4 (`PIN_A`) |
| `DT` / `B` | GPIO 7 (`PIN_B`) |
| `SW` | GPIO 13 (`PIN_SW`) |

Encoder modules may pull their signal pins up to VCC. Use 3.3 V so these signals
stay within the ESP32-S3's input limits; its GPIOs are not 5 V tolerant. See the
[Espressif datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf).
A bare mechanical encoder needs no VCC connection: connect its rotary common and
one switch contact to GND, A/B to GPIO 4/7, and the other switch contact to GPIO 13.
The firmware enables internal pull-ups.

GPIO 35–37 are reserved for octal PSRAM on the configured N16R8 target.
The encoder avoids external JTAG GPIO 39/40 and possible RGB LED GPIO 38.
The owner's board is labelled YD-ESP32-23 2022-V1.3; the
[published YD design](https://github.com/vcc-gnd/YD-ESP32-S3#readme)
uses GPIO 48 for its RGB LED. Board metadata remains the N16R8 build target.
See [Espressif's board pin restrictions](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html).

- Turn to change volume; short-click to mute/unmute.
- Hold the switch while powering on or resetting to request Wi-Fi setup/recovery.
  Boot checks for a pressed switch, without a timed hold threshold. Release after
  setup mode starts. This does not start touch calibration.
- During normal operation, a hold of 3 seconds requests light sleep; another
  encoder press wakes the radio.

Leave GPIO 6 (`PIN_K0`) disconnected for this one-button wiring. If retaining
a separate K0 button, keep it on GPIO 6; its sleep/factory-reset handler and
light-sleep wake remain active. Do not bridge GPIO 6 and 13. Sleep consumes
the initiating and waking button releases before restarting normal startup.

Pin remap, 2026-10-13: with power disconnected, move encoder A from 39 to 4,
B from 40 to 7, SW from 38 to 13, and TFT backlight control from 3 to 1.
Touch T_CLK/T_DIN must already be on shared SPI GPIO 12/11, not the previous
separate-bus GPIO 4/7. Rewire all four connections before using this image.
Hardware acceptance of the remap remains pending.

The future [battery power plan](battery-power-architecture-plan.md)
also uses this one switch and GPIO 13, through an adapted BSS138 interface to the
IP5310 KEY input. It replaces the relay proposal. That circuit requires different
switch wiring and bench validation; its software shutdown sequence is not yet
implemented.

See the [configuration and controls specification](../ui/configuration-and-controls-plan.md)
for the current feature scope and unresolved power decisions, and the
[shared-bus/remap record](touch-spi-wiring.md) for diagnostic evidence.
