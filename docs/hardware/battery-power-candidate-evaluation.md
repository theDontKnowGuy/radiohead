# Battery power candidate evaluation

**Historical evaluation, superseded on 2026-10-06.** The user waived off while
charging and accepted double-click shutdown. Use the
[simple battery power plan](battery-power-architecture-plan.md) for the current
two-module proposal. The charging-off gates and relay recommendation below no
longer govern the design. Chip/source observations remain as research history.

Date: 2026-10-05. Documentation and source review only. No electrical results.

## Decision

The [linked IP5310 board](https://www.aliexpress.com/item/1005008291016827.html)
is **not qualified as a complete radio power solution**. The user confirmed it
has not been bought. Do not order it on the assumption that firmware can turn
the radio off while USB charging continues.

Continue evaluating the existing relay as the fallback. This is a direction for
bench investigation, not approval of a circuit. Exact board identity and relay
measurements are needed before selecting the small interface. The
[current handoff](battery-power-architecture-plan.md) remains authoritative;
the rejected UPS and expanded relay designs remain superseded.

## Evidence and its limits

The manufacturer-authored [IP5310 V1.37 datasheet](https://wiki.geekworm.com/images/c/ce/IP5310-datasheet-en.pdf)
establishes these chip-level facts:

- Page 9 makes charger removal a condition of the double-press transition to
  standby; charger insertion enters the working state.
- Page 13 describes short-press wake and double-press boost shutdown. A 10 kohm
  KEY-to-BAT configuration disables double-press handling.
- Page 18 specifies the **IP5310_I2C** variant for I2C, with L1/L2 used for the
  bus. Its example pull-ups connect to BAT.
- Page 7 specifies a VIN-to-VOUT PMOS path, and battery standby current with
  VIN absent. That number is not whole-system off current during charging.

Inference: button emulation alone cannot be accepted for off-while-charging.
Disabling a boost converter does not establish isolation of the charger supply.
The KEY option that protects runtime double clicks also removes that shutdown
gesture. Battery-referenced control wiring needs an interface suitable for
powered and unpowered ESP32 states.

A mirrored manufacturer-authored [IP5310 register document V1.28](https://www.scribd.com/document/894318668/ip5310-i2c)
adds the following evidence, subject to confirming the purchased chip revision:

- Its application notes distinguish the ordinary part from IP5310_I2C.
- SYS_CTL0 (0x00), bit 5 controls boost enable; bit 4 controls charging.
  Disabling boost is explicitly not sufficient to enter standby automatically.
- SYS_CTL1 (0x01) includes button behavior and boost behavior after VIN removal.
- Register changes require read/modify/write, preserving other bits; defaults
  can differ between batches.

This does not establish a command that isolates the radio rail while preserving
charging. No register writes or bus probes were performed. Do not transplant
IP5306 register code or connect the illustrated BAT pull-ups to ESP32 GPIOs.

The AliExpress page could not be fetched in this pass. Exact variant, schematic,
pad access, protection, current rating, price, stock and delivery are unverified.
A product title or a chip datasheet cannot substitute for those board facts.

## What would qualify direct power

All of the following must be demonstrated on the exact board:

1. A documented control switches the **radio supply** off under load with USB
   absent, present, newly inserted, and removed again, while charging remains
   available. Check the rail electrically; dark LEDs are insufficient evidence.
2. Encoder wake works from off; normal clicks, double clicks and long holds do
   not cause unintended PMIC shutdown or an immediate restart after turn-off.
3. The interface neither exceeds ESP32 input limits nor powers its 3.3 V rail
   through a control wire while the radio supply is disconnected.
4. The module supports measured startup and sustained radio demand at low
   battery, including the TFT and both amplifiers.

Until these pass, direct power stays unqualified. No claim is made that every
custom IP5310 board is incapable of meeting the requirements.

## Minimal relay fallback to evaluate

The proposed power boundary is the relay's normally-open contact between the
module's 5 V output and the entire radio supply. Charging remains upstream.
All radio loads, including both amplifiers and the TFT, belong downstream.
Contact terminal order must be measured. This describes a power boundary, not a
terminal-by-terminal wiring plan.

The small control interface must implement these states:

| State | Required behavior |
| --- | --- |
| Off, button released | Relay released; no radio power or GPIO back-feed, including with the charger connected. |
| Starting | One encoder press requests PMIC wake and relay START through electrically separated paths. |
| Firmware takeover | Assert HOLD before display, Wi-Fi, calibration or boot animation; then allow button release. |
| Running | HOLD retains power; encoder continues volume/mute/hold behavior without triggering PMIC shutdown. |
| Shutdown requested | Complete safe shutdown work; ensure START is inactive before releasing HOLD, or provide an independently verified START inhibit. |
| Off again | A previous held press cannot immediately restart the radio; require release and a fresh press. |

Three unresolved details determine whether this stays simple:

- **Wake timing:** the board must produce 5 V while START is still requested.
  If wake occurs only after release, a bare momentary START path cannot perform
  takeover. Measure before adding pulse storage or other components.
- **Shutdown while held:** the current runtime hold event fires before release.
  With START still active, releasing HOLD alone cannot open the relay. A
  firmware transition that waits for debounced release is a candidate solution,
  and must keep servicing the application while waiting.
- **Reset continuity:** a bare GPIO HOLD can disappear during software reset,
  OTA reboot, watchdog reset or brownout. Early setup cannot bridge time before
  setup executes. Preserve power through intended reboots with a measured
  hardware solution, or document a product decision requiring manual restart.
  Do not silently change existing Restart behavior.

Determine the S/+/- module's trigger polarity, input current, open-input state,
3.3 V compatibility, release behavior and supply current with a current-limited
bench setup, initially disconnected from the ESP32. Its Songle relay marking
does not identify the input circuit. Neither a direct GPIO connection nor a
diode-OR of unknown voltage domains is approved by this report.

## Bench sequence and result record

Record board photos/revision, chip marking, port used, instrument, cell identity,
load, voltages, current and temperature for each run. Every result below is
**not measured**. Do not use a normal bench supply as a charging battery sink
unless it is explicitly designed to absorb charge current.

| Test | Evidence required to pass |
| --- | --- |
| Board and battery identity | Exact module variant and connections; correct charge-voltage configuration for the identified cells; charge/discharge ratings, protection and temperature sensing established. |
| Radio supply compatibility | Confirm actual devboard/regulator and TFT supply inputs; identify speaker impedance and amplifier wiring. |
| Wake / takeover | Capture button, module 5 V, relay state and radio rails; establish minimum reliable hold duration with margin, including low battery. |
| USB / shutdown | Test off before USB insertion, off during active charging and after charge termination, USB removal while off, and restart in each case. Radio remains disconnected until intended wake. |
| Ordinary input | Repeated clicks, fast double clicks, long holds, release after shutdown and a continuously held button produce only intended actions. |
| Low and high load | Muted/dim radio remains operating; startup, Wi-Fi activity and loud stereo playback do not reset it or cause rail collapse or excessive temperatures. Repeat near low-battery cutoff. |
| Reset and OTA | Intended software/OTA reboots recover; watchdog/brownout behavior is known; shutdown is deferred throughout firmware writes. |
| Off current / isolation | Measure battery-side total off current and radio-side leakage separately; measure radio 5 V and 3.3 V with all controls connected. Repeat with charger present. No powered programming USB cable. |

Select numerical electrical limits from the actual component specifications
before testing. Define an acceptable whole-system standby-current budget;
complete radio disconnection does not imply zero charger/protection current.

For initial sizing only, use `I_pack = (V_radio * I_radio) / (V_pack * efficiency)`.
For example, an assumed 5 V, 2 A load at 3 V and 85% efficiency needs about
3.92 A from the pack. These are illustrative assumptions, not measured radio
demand or confirmed board efficiency. Two parallel cells do not guarantee equal
current sharing; capacity markings alone do not qualify the pack. Establish
cell matching, voltage equalization and suitable pack protection before assembly.

## Firmware inspection and later implementation

The existing worktree has the startup coordinator in `src/main.cpp`; the user's
deletion of `application.cpp/.h` was preserved. Source observations:

- `setup()` samples `PIN_SW` for Wi-Fi setup, and
  `configureTouchCalibration()` samples it again for calibration. A normal
  power-on hold would enter both recovery paths.
- `pollEncoderButton()` has 25 ms debounce and a 700 ms runtime hold, but no
  startup-press suppression. A press surviving startup can emit a runtime hold.
- `startDeviceControl()` occurs after interactive/network/boot work. It is too
  late to own initial HOLD assertion.
- `goToSleep()` saves settings, stops audio, waits five seconds and enters deep
  sleep with K0 wake. The `/off` route and UI standby command reach this path.
  It does not disconnect power.
- Restart is used by local UI, Wi-Fi changes, web handling and firmware updater.
  No battery-source detection or relay HOLD implementation was found in the
  inspected power paths.

Once the electrical gates pass, put power sequencing in `device_control` with
explicit hardware constants in `app_state` and only early orchestration in
`main`. Consume the startup press through its debounced release. Preserve
deliberate boot recovery using a distinguishable gesture, such as a fresh hold
after releasing the start press; settle the exact gesture before implementing.
Handle external, battery and unknown power-source states per the controls spec.
Review every restart path and both upload/background update guards. Do not use
the current five-second blocking sleep display as a new shutdown state machine.

## Procurement and completion status

No finalized shopping list is justified yet. The linked module is the sole
current AliExpress candidate; the existing relay is retained for investigation.
No extra controller, interface parts or GPIOs have been selected. No basket
addition or purchase occurred, and the previous AliExpress browser restriction
was not bypassed.

To advance: obtain exact listing-variant details and board documentation or
clear board photographs, then perform the electrical sequence above. Hardware
is not present for that validation. A replacement wiring diagram and firmware
remain dependent on those results, as required by the handoff's work order.

This pass changed documentation only. No firmware build, flash or hardware
verification was performed; existing source, artwork and vendor files were
left untouched.
