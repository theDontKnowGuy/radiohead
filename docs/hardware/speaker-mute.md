# GPIO2 speaker mute

The [installed assembly](current-audio-hardware.md) has the owner's two-transistor
mute circuit. GPIO2 is a **3.3 V logic output: HIGH = silent, LOW = sound**.
It cannot drive speaker or relay-coil current. Circuit values/schematic are undocumented.

Firmware sets HIGH first in setup and holds it for at least **4 s**, without blocking.
Afterward it stays HIGH for mute, zero volume, pause/stop, connecting/failure,
source handoffs and light sleep. Playback on radio/podcasts/Spotify can release it.
Unmute keeps volume and playback position. Boot audio may become audible after 4 s;
its fixed volume is independent of saved radio volume.

The external circuit must default muted during reset/unpowered time; firmware
cannot silence anything before setup. Check GPIO2 to GND and actual speaker silence
on cold boot, mute/unmute, zero volume, reconnect, podcast/Spotify pause and sleep/wake.

Recorded 2026-10-26: `check_speaker_mute.py`, encoder/boot checks, build and diff
check passed, including the 4000 ms boundary and rollover. End-to-end electrical/
listening tests remain pending; installation alone does not establish them.
