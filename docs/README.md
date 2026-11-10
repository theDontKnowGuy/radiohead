# Radiohead documentation

## Build and operation

- [Build and flash firmware](building.md): prerequisites, commands, artifacts, and recovery builds.
- [Encoder controls](hardware/encoder-controls.md): volume, mute, sleep/wake, and setup mode.
- [Settings backup and restore](settings-backup.md): private `.rhb` backups and recovery API.
- [Boot screens](boot-modes.md): persisted artwork selection and HTTP commands.
- [Wi-Fi roaming](wifi-roaming.md): saved networks and reconnection behavior.
- [Programs catalog](programs-catalog.md): recorded shows, feeds, and direct audio.

## Hardware

- [Current audio assembly](hardware/current-audio-hardware.md): installed hardware and remaining acceptance checks.
- [DAC and amplifier wiring](hardware/audio-wiring.md): board drawing, power, configuration pads, and speakers.
- [Speaker mute](hardware/speaker-mute.md): GPIO2 contract and validation.
- [TFT and touch wiring](hardware/tft-wiring.md): complete header, backlight, calibration, and sensitivity trials.
- [Encoder wiring](hardware/encoder-controls.md): pin assignments and one-button setup.
- [Shared SPI and pin-remap record](hardware/touch-spi-wiring.md): migration history and encoder diagnostics.
- [Battery power plan](hardware/battery-power-architecture-plan.md): proposed hardware and open shutdown decisions.

## Development and design

- [Firmware architecture](firmware-architecture.md): module ownership and structural review.
- [UI documentation](ui/README.md): visual references, specifications, and implementation status.
- [Configuration and controls specification](ui/configuration-and-controls-plan.md): current scope, retired features, and open decisions.
- [Web UI handoff](ui/web-configuration/README.md) and [progress ledger](ui/web-configuration/progress.md).
- [Build consolidation plan](framework-consolidation-plan.md) and [verification record](framework-consolidation-validation.md).

## Diagnostics and investigation history

These records distinguish observations, proposals, build checks, and physical-device evidence.
Older hardware references apply to the assembly tested at the time; use the current audio record above.

- [Radio crashes](radio-crash-diagnostics.md), [stream stalls](radio-stall-diagnostics.md), and [silence investigation](radio-stall-investigation/README.md).
- [Touch diagnosis and recovery](ui/touch-diagnosis.md) and [TFT white-screen investigation](hardware/tft-white-screen-diagnosis.md).
- [Standalone TFT color test](../experiments/tft-color-test/README.md).
- [Spotify research](spotify-integration-research.md), [acceptance plan](spotify-native-validation.md), and [validation ledger](spotify-validation/progress.md).
- [Spotify output handoff](spotify-validation/s2-output-handoff.md), [resource checks](spotify-validation/s3-resources.md), and [stop/disconnect investigation](spotify-validation/stop-disconnect-2026-11-09.md).
