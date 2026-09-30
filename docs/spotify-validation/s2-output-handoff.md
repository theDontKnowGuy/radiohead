# S2 output ownership and experimental integration

Date: 2026-09-25; updated 2026-09-29. Status: partial implementation.
Short-run integrated Spotify playback is verified; directed transitions and
stability acceptance remain open.

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
At the initial audit, no phone-to-radio playback handoff, radio/podcast return
handoff, 100 directed transitions, 20 rapid transitions, memory trend, or
audible overlap check had passed. The experimental image is not an S2
acceptance build.

## 2026-09-28 continuation

The first integrated listening trial exposed Spotify PCM underruns, followed
by decoder watchdogs and web timeouts after a Spotify-to-radio switch. The
decoder now backpressures while local output owns I2S. Disabling Wi-Fi modem
sleep, as in the earlier standalone receiver, restored a sustained 176.4 KB/s
PCM output in a 14-window device sample with no empty polls or I2S errors.
That image still left only about 6 KB of internal RAM when local radio and a
podcast catalog ran after Spotify; web requests subsequently timed out.

The September 28 candidate suspends the cspot decoder and waits up to one second
for it to close the current CDN stream before local I2S is restored. It
built and booted on device. At local startup, 10 radio-to-podcast and 10
podcast-to-radio selections plus 20 immediate alternating selections all
returned HTTP 200; the web player remained responsive and no watchdog or
stream recovery appeared. The device reported 35,635 B free heap after that
local run. A later Spotify-to-local command returned HTTP 303 but the owner
heard silence until selecting a station manually, so directed handoff is
still not accepted. See the
[evidence ledger](progress.md) for image hashes and trial details.

## 2026-09-29: USB serial caused a playback-rate failure

The `f7d81dcd` S2 image included the Wi-Fi sleep fix, but playback became
sluggish again after its serial capture ended. With a serial reader attached,
its steady PCM reports were 881,664–882,688 bytes per five seconds. That
image printed a diagnostic line from the I2S output task every five seconds.
The installed Arduino 3.3.11 `cores/esp32/HWCDC.cpp` sets a 100 ms TX timeout
and allows 20 consecutive timeouts when a USB host remains plugged in but
stops reading: a roughly two-second wait. Opening USB serial also resets
this test radio, changing the test state.

The first HTTP-instrumented image, `7e32a071`, kept that periodic print.
With USB serial closed, 28 steady samples each reported exactly 534,528 PCM
bytes per five seconds, despite a nearly full PCM queue and zero I2S write
failures. The owner heard sluggish/skipping audio across multiple songs.
That is about three seconds of 44.1 kHz stereo output in each five-second
window. The repeated fixed deficit strongly implicates the serial print,
rather than CDN starvation, in the continuous slowdown.

The installed follow-up image, SHA-256
`32e24232ec8a09e69892a2c0ca5831ea704c12d21f9095529d7fab6b4ae26778`,
removes **all serial writes from the Spotify output task**. It retains
five-second counters at `GET /api/spotify/diagnostics` in the opt-in image.
With USB serial closed, 19 steady HTTP samples reported 881,664–882,688
PCM bytes per five seconds, zero empty polls and zero I2S write failures;
the owner described playback as crystal clear. This confirms a short-run
fix, not the full S2 stability or source-handoff acceptance.

The same candidate expands the PSRAM PCM queue from 128 KiB to 384 KiB and
waits for 320 KiB before output or after an underrun. The earlier 128 KiB
image had one measured CDN/decode gap with a roughly 270 KiB five-second
production deficit. The larger reserve is intended to absorb such a gap,
but the serial-output change resolved the repeatable continuous slowdown.
Do not infer that queue size alone fixed playback. Keep diagnostic reporting
off the hot path; sample the read-only HTTP endpoint with USB serial closed.
Recheck controller latency and handoffs with the larger queue.

## 2026-09-29: short-run bidirectional handoff

On the installed `32e24232…` image, a web `/next` command during Spotify
playback released Spotify I2S, emptied its PCM queue, and reported station
NYOX as `playing`; seven subsequent one-second player checks remained
responsive. The owner then reported several radio-to-Spotify and
Spotify-to-radio switches with clear audio and no handoff problem. This
passes the short-run audible handoff check. The exact switch count and
timing were not captured, so the required 100 directed and 20 rapid S2
transitions, memory trend, and recovery cases remain open.

## 2026-09-29: integrated Wi-Fi recovery probe

The current opt-in app0 image is `ed4b809d…`, with a cspot decoder catch
for the previously uncaught TLS stream-open exception. App0-only flashing
and independent flash verification passed. During a user-controlled Wi-Fi
interruption, HTTP polling was unreachable for about 32 seconds; the Spotify
session and output remained active on return. Full-rate PCM resumed with
zero sampled empty polls or I2S write failures, and the owner heard clear
Spotify audio resume without selecting the device again. Device uptime was
consistent with no reboot. This is one short-run integrated recovery pass;
repeated outage, renewal, memory and long-run acceptance remain open.

## 2026-09-29: local transition acceptance attempt

On the `ed4b809d…` image with USB serial closed, 20 directed local transitions
(ten radio-to-podcast and ten podcast-to-radio) reached the expected
`playing` state and left Spotify output unowned. Podcast selection took a
median 9.51 seconds to report `playing`; radio took 2.42 seconds. This is
functional control evidence only; audible quality was not reported for the
counted batch. A rapid alternating run timed out on its third HTTP command
at five seconds, though the device later reported podcast playback and
responsive status endpoints. The 20 rapid-transition gate therefore failed
this attempt and remains open. No phone-controlled Spotify transition was
captured in this run. See the [ledger](progress.md) for counts and private
trace location.

## 2026-09-29: phone-controlled short batch

With the `ed4b809d…` image, the owner heard clear podcast-to-Spotify,
Spotify-to-radio, radio-to-Spotify and Spotify-to-podcast handoffs without
overlap. A podcast-to-radio-to-podcast listening pair and one subsequent
podcast-to-Spotify-to-radio-to-podcast cycle were also clear. The owner chose
a short batch instead of the proposed 20 phone transfers.

On the second batch transfer, a Spotify Next tap moved playback to the phone.
Radiohead released Spotify output, retained an authenticated session and
reported local playback stopped. A later controlled Next tap succeeded and
played the next song on Radiohead. This intermittent fallback fails the
current image's S2 acceptance; the HTTP trace cannot identify the upstream
event sequence that caused it. Pause/resume, mute preservation under phone
volume and encoder changes, volume endpoints while muted, and one nonflat
tone setting passed the reported listening/control checks. The exact counts,
resource limits, and private trace names are in the [ledger](progress.md).

## 2026-09-30: Spotify-to-episode DMA failure

The owner reproduced a failed transition to episode 1 of “יהיה בסדר”. The
episode fetch returned HTTP 200 with eight playable entries, and the Spotify
producer acknowledged release. Local I2S restoration then failed while
allocating a 2,048-byte DMA buffer. The resource endpoint showed about 35 KB
of DMA-capable memory free but only a 7,424-byte largest block, with allocation
failures increasing. Repeated episode selections failed at the same handoff.

The candidate reduces the integrated image's local I2S queue from eight to
six 256-frame DMA descriptors, lowering its late DMA allocation from 16 KiB
to 12 KiB. The normal image retains its existing 16-descriptor setting.
Device playback and repeated handoffs remain unverified.
