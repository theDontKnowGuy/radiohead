# S2 output ownership and experimental integration

Date: 2026-09-25. Status: partial implementation. Integrated Spotify playback
and directed transitions are **not verified**.

## Ownership audit

Radiohead's global `Audio audio` allocates I2S0 on pins 15/17/16 when
`setPinout()` runs. `stopSong()` does not release that channel. The S1
MAX98357A sink also allocates I2S0 on those pins. Running the two owners
together would conflict. The repository's vendor-patch rule required explicit
authorization; the user granted it after this need was documented.

`Audio::releaseOutputForHandoff(timeoutMs)` now cooperatively parks its
decoder task, waits for acknowledgement, stops and clears output, then deletes
the I2S channel. `restoreOutputAfterHandoff()` recreates the channel using
the saved GPIO setup before unparking the task. The shared `Audio` object is
never destroyed. A probe image completed two idle release/restore cycles on
the device, with no panic or watchdog in the short capture. Audible handoff
has not been checked.

## Experimental coordinator

The native adapter is linked only by the direct ESP-IDF experiment.
`src/media.cpp` owns a source generation and runs transitions from the
Arduino loop. The adapter has a fixed 16-entry event mailbox with sequence
numbers; a full mailbox always admits transfer-away/stop. The PCM producer
checks a separate atomic cancellation flag, so a full PCM queue cannot delay
release. Local selection waits up to one second for the Spotify writer to
acknowledge that it has deleted its sink before Radiohead restores I2S.
Spotify acquisition occurs only after Radiohead releases I2S. A timeout
leaves the coordinator in a stopped/retryable state.

The pinned cspot experiment adds an `ACTIVATE` event for a fresh phone
Load/Play command. Decoded PCM callbacks cannot themselves request output
ownership after local selection. Spotify pause retains its session and
silences output. Transfer away releases the sink and leaves local playback
stopped. The selected radio catalog slot and podcast browse cache remain
available; a local selection can explicitly restart them. Local mute is
preserved. Remote volume maps to the existing 0–21 volume index without
clearing mute, and the Spotify sink applies a three-band tone with headroom.
The tone response is an approximation of the local Audio library's EQ and
needs listening validation.

The adapter uses Radiohead's Wi-Fi connection and private `spotify` NVS
keys. Pairing is served by the existing port-80 web server, avoiding a second
Civetweb worker stack. Spotify mDNS advertises that endpoint. The S1 device
name is retained because cspot hashes it into the device ID; changing it
caused the saved pairing to fail. The normal PlatformIO build omits the
adapter, pairing route and Spotify mDNS service.

## Evidence and remaining risks

Both the normal PlatformIO image and the direct IDF integrated image build.
The integrated partition table matches the normal one byte for byte. Moving
three existing TFT scratch buffers to PSRAM in the integrated image raised
static internal DIRAM headroom from 46,069 B to 86,245 B. The device booted,
served `/spotify_info`, and authenticated to Spotify using the saved S1
pairing. One 65-second capture had no panic or watchdog. Internal free RAM
after authentication was only 9,471 B with a 7,168 B largest block, so
playback fit is a serious open issue. A subsequent rebooted capture showed
AP retries with poor Wi-Fi signal.

An ESP-IDF option that prefers PSRAM for eligible Wi-Fi/lwIP allocations
increased free internal RAM by about 6.4 KB at discovery and the first AP
attempt. Its first 100-second capture did not complete authentication, so
the post-authentication effect remains unknown.

The cspot session, decoder and queue tasks remain resident between source
transfers. Their detached lifetime does not yet provide a general bounded
stop/join after a session failure; S1's Wi-Fi/CDN failure also remains.
No phone-to-radio playback handoff, radio/podcast return handoff, 100 directed
transitions, 20 rapid transitions, memory trend, or audible overlap check
has passed. The experimental image is not an S2 acceptance build.
