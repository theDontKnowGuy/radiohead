# Battery power: bench proposal

**Not a finished build.** The 2026-10-06 proposal supplies 5 V, but the installed
[HX-A244 amplifier](current-audio-hardware.md) uses a separate 12 V supply.
Its battery conversion and coordinated shutdown remain undecided. GPIO2 mute
is not power isolation. Firmware currently provides light sleep, not electrical off.

## Parts and intended behavior

Proposed: an ordinary **IP5310 USB-C charger/fixed-5 V power-bank board**, a
protected **1S2P** pack, and one adapted **BSS138** level-shifter channel.
No relay, HOLD pin or extra controller. IP5210 is not an interchangeable identity.
The owner accepts double-click hardware off and power staying on while charging.

- Press the encoder switch while off: wake the PMIC; no firmware HOLD takeover.
- Turn/single-click while running: volume/mute as today.
- Two short clicks within one second on battery: immediate PMIC cutoff, bypassing
  saves and OTA guards. Do not use this gesture during firmware updates.
- Planned software shutdown: save, silence, wait for release, imitate double-click.
  If output stays on, use awake silent/dim standby with web/input; do not call it off
  or claim to identify USB versus a failed shutdown.

[IP5310 datasheet](https://wiki.geekworm.com/images/c/ce/IP5310-datasheet-en.pdf),
pp. 9/13: short-press wake, double-press boost-off; a 10 kΩ KEY-to-BAT option
**disables double-click**. Use a board with the ordinary enabled configuration.
Long hold may control a flashlight, not shutdown.

## Power wiring to qualify first

USB charger → PMIC charging port; protected 1S2P pack → identified battery pads;
PMIC 5 V → verified ESP32 5 V input and suitable TFT supply path. Grounds common.
Confirm DAC VIN rating. Never connect a 3.3 V-only input to 5 V.
Amplifier supply/shutdown still needs a separate decision.

Use matched conventional 4.2 V Li-ion cells in **parallel, never series**.
Confirm model/current/charge limits, equal voltage before paralleling, protection,
fuse, holder, wire and connector ratings. Capacity alone does not qualify cells.
Size for measured peak current, including low battery; advertised 3.1 A is unverified.
Do not use a bench supply as a charging sink unless it supports that operation.

## Proposed shared-button interface

Power off. Remove the existing direct switch-to-GPIO13 connection. Isolate any
encoder-board SW pull-up/LED from KEY; A/B remain GPIO4/7. Identify tactile-switch
pairs by continuity, not position, and replace the PMIC's KEY-to-GND switch with
the encoder's two switch contacts.

Use a discrete BSS138 MOSFET board, not TXB/TXS or a resistor-divider board.
Verify [this topology](https://cdn.sparkfun.com/datasheets/BreakoutBoards/Logic_Level_Bidirectional.pdf)
on the actual module.

| Connection | Destination |
| --- | --- |
| Encoder switch contacts | PMIC KEY and GND |
| Shifter HV1 | KEY |
| Shifter LV1 | ESP32 GPIO13 |
| Shifter LV / GND | Switched ESP32 3.3 V / common GND |
| Shifter HV supply / unused channels | Unconnected after modification |

**Remove only the HV1-to-HV pull-up**, usually 10 kΩ, identifying it by continuity.
Keep the LV1-to-LV pull-up and MOSFET. Do not tie HV to BAT/5 V: the PMIC supplies
KEY bias and a BAT pull-up can change its mode. BSS138 gate uses switched 3.3 V,
source LV1, drain KEY. GPIO13 normally stays input; generated press pulls LOW,
release returns high impedance. **Never drive HIGH push-pull.**

This is not manufacturer-validated KEY wiring. Measure KEY/GPIO at full and low
battery, including KEY below 3.3 V; check body-diode/pull-up injection, valid levels,
mode changes and the unpowered ESP32 rail. No powered programming USB during off tests.

## Firmware work after electrical checks

Consume the startup press through release; preserve deliberate Wi-Fi recovery
with a distinguishable fresh gesture. Keep the current **3 s** hold contract
(the earlier 700 ms suggestion is superseded). Defer software shutdown during writes,
complete saves/audio stop, and suppress generated input events.

Initial unvalidated pulse proposal: after **1.2 s released quiet time**, two
**350 ms LOW** pulses separated by **150 ms released**, then high impedance.
Restart the quiet interval if the user presses. Centralize GPIO13 ownership in
`device_control` so its input task cannot race pin changes. Review K0 and `/off`.
If cutoff fails, stay responsive in silent/dim standby; never retry indefinitely.

## Bench sequence

1. PMIC alone under load, USB absent: verify single-click on, double-click off,
   rail collapse and staying off with load attached; establish battery/charge setup.
2. Add modified shifter: check KEY classification, full/low-battery GPIO levels,
   off-state 3.3 V backfeed and total off current.
3. Test reset, held startup/release, software pulses, charger insert/remove and quiet
   operation. Before charging-powered OTA, verify this exact board cannot cut output
   from the physical double-click while USB is present.
4. Once amplifier power is designed, test the complete radio at startup, loud and
   muted/dim playback, low battery: rails, temperature, peak current and shutdown.

Old buying leads (variants/ratings not verified): IP5310
[item 1005008291016827](https://www.aliexpress.com/item/1005008291016827.html),
alternative [1005008721553379](https://www.aliexpress.com/item/1005008721553379.html);
BSS138 [1005012826711321](https://www.aliexpress.com/item/1005012826711321.html).
Confirm the actual PCB, accessible resistor and protection before buying/building.
No purchase, electrical test, firmware implementation or final diagram is established.
