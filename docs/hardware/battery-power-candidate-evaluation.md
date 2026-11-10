# Earlier battery research

Historical 2026-10-05 review, superseded by the [current proposal](battery-power-architecture-plan.md).
The owner later accepted double-click off and waived off while charging.
The relay fallback is rejected; its old requirements must not return as build tasks.
Neither design was electrically validated. Old references to two MAX amplifiers
are obsolete; current hardware is PCM5100/HX-A244 with a separate 12 V amplifier path.

## Findings worth keeping

From the [IP5310 V1.37 datasheet](https://wiki.geekworm.com/images/c/ce/IP5310-datasheet-en.pdf):

- p. 9: double-press standby transition requires charger removal; insertion wakes it.
- p. 13: short-press wake/double-press boost shutdown; KEY-to-BAT 10 kΩ disables
  double press. Boost-off does not prove charger-to-load isolation.
- p. 18: **IP5310_I2C** is a distinct variant; L1/L2 become the bus, with illustrated
  BAT-referenced pull-ups. Do not connect those directly to ESP32 GPIOs.
- p. 7: VIN-to-VOUT PMOS path and battery standby figure do not establish whole-radio
  off current while charging.

The mirrored V1.28 register notes described SYS_CTL0 `0x00` bit 5 boost and bit 4
charging; disabling boost alone does not enter standby. SYS_CTL1 `0x01` includes
button/VIN-removal behavior. Revision-specific read/modify/write is needed;
no register probes/writes occurred. Do not transplant IP5306 code.
The AliExpress board's exact variant/schematic/protection/current were unverified.

## Lessons for any later power circuit

Measure supply rails under load with charger absent/present/inserted/removed,
button held/released, low battery and quiet playback. Check startup/OTA/reset
continuity, GPIO backfeed, off current and thermal behavior. Dark LEDs are not proof
of cutoff. A relay part number does not identify a module's trigger circuit.

Old relay START/HOLD proposals had unresolved wake timing, shutdown while held,
and GPIO loss during reset. Early setup cannot bridge a pre-setup loss of HOLD.
Those were reasons to reject a premature wiring diagram, not a reason to revive it.

For sizing, `I_pack = total_load_power / (V_pack × efficiency)`; 5 V×2 A at 3 V
and assumed 85% needs about 3.92 A. This was illustrative, not measured demand or
a rating of the board. Parallel cells need matching/protection and do not guarantee
equal sharing. Qualify the current amplifier's supply before claiming a whole-radio solution.
