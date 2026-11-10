# Build your Radiohead

1. Check the [parts and audio hardware](hardware/current-audio-hardware.md).
2. Wire the [display](hardware/tft-wiring.md), [encoder](hardware/encoder-controls.md),
   [DAC and amplifier](hardware/audio-wiring.md), and [mute signal](hardware/speaker-mute.md).
3. [Build and flash](building.md), then follow the Wi-Fi setup shown on the screen.
4. Open `http://radio.local` or the displayed IP to add stations and [podcasts](programs-catalog.md).
   Spotify setup is in the web interface's Spotify section.
5. [Back up your settings](settings-backup.md) before experimenting.

## Hardware

The wiring above describes the current PCM5100/HX-A244 build. [Battery power](hardware/battery-power-architecture-plan.md)
is still a proposal. Disconnect power before changing wires.

## If something goes wrong

See [touch recovery](ui/touch-diagnosis.md), [blank display](hardware/tft-white-screen-diagnosis.md),
[audio stalls](radio-stall-diagnostics.md), or [crash recovery](radio-crash-diagnostics.md).
For code changes, start with [architecture](firmware-architecture.md) and [UI notes](ui/README.md).
