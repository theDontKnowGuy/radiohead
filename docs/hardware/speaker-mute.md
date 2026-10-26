# GPIO2 speaker mute

GPIO2 is a 3.3 V logic output for the external speaker mute/relay driver:
**HIGH means silent; LOW enables sound.** It is initialized HIGH as the first
operation in Arduino setup and cannot go LOW until four seconds have elapsed.
This is a nonblocking minimum hold; stopped playback keeps it HIGH longer.

After the hold it follows radio, podcast and Spotify playback. Mute, volume
zero, pause, stop, connecting and failed playback keep it HIGH. It is also
asserted before source handoffs and held HIGH through light sleep. Unmute keeps
the selected volume and does not restart playback. The firmware's embedded
boot sound can become audible after the four-second hold if it is still playing;
its own fixed volume remains independent of the saved radio volume.

GPIO2 is unused by the other configured peripherals and is not a strapping pin
on ESP32-S3; see the [Espressif GPIO reference](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-reference/peripherals/gpio.html).

The external circuit must default to muted while the ESP32 is resetting or
unpowered: firmware cannot suppress a speaker announcement that occurs before
setup runs. Use a compatible logic input/relay driver with common ground, not
a relay coil or speaker load connected directly to GPIO2. The circuit and its
active-HIGH behavior still require physical verification; this change does not
establish a wiring connection to a particular amplifier board.

Validation (2026-10-26): `python3 tools/check_speaker_mute.py` passed for both
local-only and Spotify builds of the production gate, including the 4000 ms
boundary and rollover. Encoder power and boot-mode host checks, `git diff --check`,
and `pio run -e esp32s3` passed. Firmware: 4,688,224 bytes, with 1,865,376 bytes
free in the app slot; DIRAM: 261,835 bytes (76.61%), with 79,925 bytes remaining.
The firmware has not been flashed. On-device acceptance remains pending: cold boot
with the speakers powered, mute/unmute, volume zero, station reconnect, podcast
pause/resume, Spotify pause/transfer, and sleep/wake. Measure GPIO2 relative to
GND and confirm the external circuit actually silences the speakers.
