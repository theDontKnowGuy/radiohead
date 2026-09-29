# Native Spotify validation evidence ledger

Started 2026-09-24 on branch `spotify/native-s0-s1` from `b47961c5684731e8e2d79c5c4f5999eab99aa5c8`.
The source handoff is `docs/spotify-native-validation.md`. This ledger records observed
results separately from pending device tests. Private serial captures, NVS and
firmware backups are kept outside the repository.

| Package | Code/build | Hardware/function | Resources/timing | Evidence / next action |
| --- | --- | --- | --- | --- |
| S0 | PASS | PASS (user listening report) | PASS (baseline measured; existing defects below) | 30-minute private capture; `tools/spotify_validation_summary.py`. |
| S1 | PASS (standalone and integrated builds) | PARTIAL (one integrated Wi-Fi interruption recovered with clear audio; earlier standalone run 19 aborted) | PARTIAL (short-run resource counters clean; renewal only observed on earlier run 12) | Three cold restarts and remote controls checked on earlier images; adjacent duplicate inconclusive. Two-hour run remains open. See short-run continuation below. |
| S2 | PASS (normal and integrated images build) | PASS (user marked step completed successfully on 2026-09-29) | NOT VERIFIED (formal transition counts and leak trend not supplied) | User accepted S2 completion. The earlier observed skip fallback and missing formal counts remain recorded in the [S2 audit](s2-output-handoff.md); S3 must measure current behavior. |
| S3 | PASS (normal and integrated images build; fixed endpoint verified on device) | PARTIAL (computer-controlled Spotify played clearly; one Spotify-to-radio handoff reached local `playing`; iPhone connection unresolved) | FAIL (decoder/queue stack below preset 25% reserve; full resource budget and stress run open) | Short-run passes and limits are in [S3 measurements](s3-resources.md). Full S3 acceptance remains open. |
| S4 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Outside this request. |

## Default build promotion: 2026-09-29

After a routine Arduino build temporarily removed Spotify from the radio, the
user requested Spotify in the default image. `pio run` now selects `esp32s3`,
and `pio run -e esp32s3` builds the native integrated image into the normal
`firmware.bin` path. A completed build matched the already tested integrated
binary byte for byte (SHA-256 `7b0c03c977b3d40cf674c21333413678dd67826b80aea6a3a344f23a29d0b4a7`),
with 2,141,776 bytes free in the 0x640000-byte app slot. The serial upload
command now uses the matching ESP-IDF bootloader, partition table and OTA
selection with the integrated app at `0x10000`, preserving NVS, LittleFS and
app1. This build change has not been uploaded again: the radio already runs
the same integrated app hash. Earlier paragraphs below describe the former
opt-in build at the time of those observations.

## S2 completion and S3 start: 2026-09-29

The user explicitly marked step 2 completed successfully and requested S3.
That is the S2 completion decision. It is not a new measured trace: the last
recorded short batch includes a Spotify Next fallback to the phone, and the
planned 100 directed/20 rapid transitions and equal-settling memory trend were
not captured. Keep those observations visible while testing the current image.

S3 adds an opt-in, read-only `/api/spotify/resources` endpoint and a bounded
HTTP capture tool. Both are intended to collect resource/timing data with the
USB serial reader closed, since serial writes from the output task previously
slowed Spotify PCM. Build and device results are recorded in the S3 procedure.
On the corrected S3 image, computer-controlled Spotify playback passed a
three-minute HTTP/PCM capture with clear audio reported by the owner; a further
110-second run passed 20 same-volume web control probes. A web `/next` command
released Spotify output and reached local NYOX `playing`. The owner stopped
further testing before a return-to-Spotify handoff or full S3 stress/soak run.

## S2 start: 2026-09-25

The user authorized starting S2 while treating S0/S1 as prerequisites, despite
the outstanding S1 Wi-Fi recovery and long-run acceptance. The S2 ownership
audit is in [s2-output-handoff.md](s2-output-handoff.md). The current standalone
sink and Radiohead both allocate I2S controller 0 on the same pins; the
current public audio-library API cannot release that output after `stopSong()`.
The standalone Spotify worker has no bounded stop acknowledgement. These are
implementation blockers, not S2 pass claims. The user explicitly authorized
the narrow vendored Audio handoff patch on 2026-09-25.

One independent local-source correction is implemented in `src/media.cpp`:
radio, station-test and podcast selection preserve mute and reapply the saved
volume/tone. The uninstrumented `pio run -e esp32s3` build passes at 96,372 B
static RAM and 3,493,715 B linked flash after the final vendor fix. This
binary omits the Spotify adapter.

The direct ESP-IDF experiment builds to about 4.29 MB with 86,245 B static
DIRAM headroom after placing three TFT scratch buffers in PSRAM only for that
image. Its partition table is byte-identical to the normal table. The image
is built from the pinned candidate using
`experiments/spotify-native/build-integrated.sh`.

The first device probe of the vendored Audio seam performed two idle
release/restore cycles: both reported `released=1 restored=1`, with no panic
or watchdog in the short capture. It was not an audible handoff. The first
integrated boot fault was traced to cspot's separate Civetweb worker stack;
pairing now uses Radiohead's existing port-80 server. The next boot could not
reserve cspot's 16 KiB mercury worker stack. Moving the TFT scratch buffers
to PSRAM allowed boot and authentication. A later app-only image authenticated
with the S1 device identity and no panic in a 65-second capture. It reported
50,391 B free internal RAM at discovery, 30,203 B at the AP attempt, and
only 9,471 B free / 7,168 B largest block after authentication. The earlier
image with a changed device name lost its S1 identity and failed
authentication. Another rebooted capture showed transient AP retries with
poor Wi-Fi signal. These are not stable playback results.

A further experimental image enables ESP-IDF's supported
`SPIRAM_TRY_ALLOCATE_WIFI_LWIP` option. It built and flashed app-only with
hash verification. A 100-second capture had no panic and reported 56,823 B
free internal RAM at discovery and 36,635 B at the first AP attempt, roughly
6.4 KB more than the prior image at those points. It made three AP attempts
without authenticating in that capture, so no post-authentication or playback
memory improvement is claimed. Captured radio RSSI was around −83 to −85 dBm
and the selected station repeatedly timed out.

The final normal `esp32s3` build was restored to app0 with esptool write-hash
verification. A 25-second private boot capture had no panic, abort or
watchdog and showed the TFT renderer and radio audio task starting. NVS, OTA
metadata, bootloader and LittleFS were not written during these app-only
probes. Audible radio was not rechecked by the user after this final restore.

The experimental adapter uses a 16-slot event mailbox, a single main-loop
coordinator, and bounded I2S release acknowledgement. A fresh phone Load/Play
event is distinguished from late decoded PCM events. Mute, volume, and a
three-band tone are applied to Spotify PCM. The cspot service remains resident
between transfers, so its detached worker lifecycle and S1 Wi-Fi recovery
defect still require work. No 100 directed transitions, 20 rapid transitions,
audible overlap checks, or integrated playback memory trend have passed.
**S2 remains incomplete.**

### S2 continuation: 2026-09-26

The normal and integrated builds were reproduced from the current dirty
worktree. `pio run -e esp32s3` passed at 96,372 B static RAM and 3,493,715 B
linked flash; its 3,544,752-byte application image has SHA-256
`888f1c939b170f4a3caaeffa6a9cf4adb86617a0f7e354097929dffe4056e2a8`.
`RADIOHEAD_SPOTIFY_CANDIDATE=/tmp/radiohead-s2-cspot
experiments/spotify-native/build-integrated.sh` also passed, including the
partition-table comparison. Its 4,290,448-byte application image has SHA-256
`ae26255891c9034cd9313fc192cd7cfce772fc3776690863d844bbfdd9d3914e`
and leaves 2,263,152 B (35%) in the 6,553,600-byte OTA slot. `git diff
--check` passed.

No ESP32 serial device was present (`pio device list` showed only the host's
debug and Bluetooth pseudo-terminals), so the image was not flashed and none
of the required audible/directed transition cases could be rerun. The last
measured post-authentication internal heap remains 9,471 B with a 7,168 B
largest block. Code/build reproducibility therefore remains PARTIAL and S2
hardware/function and resource acceptance remain unpassed. Completion still
requires the intended radio connected over USB, the Premium phone/account,
stable Wi-Fi, and the 100 directed plus 20 rapid/interrupted transition run
specified by the handoff; no credentials should be supplied in chat.

Later on 2026-09-26 the intended ESP32-S3 appeared as
`/dev/cu.usbmodem1201` (USB VID:PID 303A:1001, revision 0.2, 8 MB embedded
PSRAM). A read-only OTA-data probe confirmed sequence 1/app0. The integrated
image above was then written app-only at `0x10000`; esptool's independent
`verify-flash` comparison passed. NVS, OTA metadata, bootloader, partition
table and LittleFS were not written.

The image joined Wi-Fi, advertised the port-80 Spotify service, found the
saved pairing, authenticated to Spotify and fetched a client-credentials
access token without a panic or watchdog during the approximately 202-second
capture. Wi-Fi initially measured about -85 dBm and suffered one beacon
timeout; after reassociation it measured about -41 dBm. Discovery reported
57,135 B free internal RAM / 31,744 B largest block, the successful AP attempt
started at 36,083 B / 15,360 B, and post-authentication reported 27,279 B /
15,360 B. This improves on the previous 9,471 B post-authentication result but
does not measure playback. Queue-task stack reserve stabilized at 23,620 B.
No phone Load/Play command arrived during the capture, so output acquisition,
audible Spotify playback and any directed source transition remain NOT
VERIFIED. The integrated image remains installed pending the interactive
phone-driven run.

### S2 device trial: 2026-09-28 — FAIL, correction pending

The normal firmware was installed at the start of this trial. The device's
app0 image and OTA metadata were backed up privately under
`~/.radiohead-recovery/spotify-s2-20260928/`; the 4 MiB app0 backup has a
valid ESP image checksum/hash. A fresh opt-in image built from `0223a0e`
plus the then-present, unrelated boot-screen worktree edits. Its SHA-256 is
`54f980a1cfe973555ef6ddf19cc66c65fdb159b49d80abdf6ac20aa52ac578ac`;
the 4,383,296-byte image left 33% of the unchanged app0 slot free. It was
flashed at `0x10000` only and independently verified with `verify-flash`.
NVS, OTA metadata, bootloader, partition table and LittleFS were not written.

The integrated image joined Wi-Fi, served `/spotify_info` (HTTP 200), reused
saved pairing and authenticated. A real phone command acquired Spotify I2S
output; PCM counters advanced with zero partial/failed I2S writes. The user
reported Spotify music was sluggish and stopped every one to two seconds.
Across 17 five-second samples, output ranged up to 880,640 bytes versus
882,000 bytes required for uninterrupted 44.1 kHz stereo, with lower steady
samples and three empty-queue snapshots. Internal free memory reached
25,811 B in those samples; the lowest sampled DMA free was 18,131 B. Twenty
simple `/api/player` requests during Spotify playback had p95 83.2 ms and
max 131.0 ms.

A web `/next` selected local radio in 775 ms. The serial log recorded one
Spotify output release and generation 2 restoration of local I2S; the web
player reported radio `playing`, and the user reported radio audio was clear.
Afterward, a podcast-page request and subsequent status requests timed out.
The device logged three IDLE1 watchdog events with `cspot_decoder` running;
the first backtrace was in TLS GCM decryption reached from the decoder's CDN
read. The user reported that Radiohead no longer appeared in Spotify. The
receiver's loss of discovery is a user observation; its exact cause is not
yet established. No podcast or reverse Spotify transition passed, and none of
the required 100 directed plus 20 rapid transitions can be counted as clean
acceptance. The private `integrated-trials-01.log`, build and flash records
preserve the failure.

### S2 playback correction trials: 2026-09-28

All corrections below used app0-only flashing followed by independent
`verify-flash`. The partition table, OTA metadata, NVS and LittleFS were
preserved. Private logs and image copies are in
`~/.radiohead-recovery/spotify-s2-20260928/`.

The first correction backpressured cspot when a local source owns output,
preventing its detached decoder from decrypting ahead without PCM pacing.
It also added a flat-tone integer gain path and five-second producer/queue
diagnostics. Its 4,383,296-byte image SHA-256 begins `935a8d98`; the user
still heard one-to-two-second stalls. PCM production was commonly only
0.72–0.80 MB per five seconds, versus 0.882 MB needed, with 85–198 empty
output polls. No I2S write failures appeared in that active playback run.

A merged cspot patch had added a one-tick sleep after every decoded 1024-byte
chunk, absent from the earlier standalone run the user heard clearly. Reducing
it to every eighth chunk (image SHA-256 `a88fb677`) and then removing it
(image SHA-256 `5df1c4fb`) did not restore continuous production. The latter
image averaged about 159,322 output bytes/s across 16 steady five-second
windows, below the 176,400 bytes/s PCM rate; it recorded 1,918 empty polls
and no watchdog in those windows. This separates decoder pacing from the
remaining source starvation.

The earlier standalone cspot image disabled Wi-Fi modem sleep, whereas the
integrated image associated with sleep enabled and a roughly 400 ms listen
interval. The current integrated candidate calls `WiFi.setSleep(false)` on
Spotify startup. Its 4,384,064-byte app image has SHA-256
`c4000ec1641b2de3e1e39eea988ce40a30b1fa8a99ca88dc5c9cb679415b5bb6`.
After the initial startup window, 14 steady output samples averaged 176,464
bytes/s (881,664–882,688 bytes per five seconds), with zero empty polls,
zero I2S write failures and zero watchdogs. This is a PCM continuity result;
the user's listening verdict for this exact image is pending.

The first Spotify-to-radio request on this image released Spotify I2S once
and restored local output (generation 2), but the selected station then
stalled and entered repeated network recovery. A podcast catalog fetch found
eight playable episodes, yet measured only 6,563 B free internal RAM at its
start and 5,895 B at completion. Subsequent web requests and ping timed out.
This is an S2 coexistence failure, not a clean transition. The Spotify
decoder was still retaining its CDN stream while local audio reopened.

The next candidate (image SHA-256 `b201d2976a6627f8792073cd0b80741393ea58e882b5b00cbe3ebf1cd61065c4`)
adds a bounded producer-suspend acknowledgement before local output is
restored. It was built, flashed app-only, and independently verified; boot
authenticated and `/api/player` and `/spotify_info` responded. At local
startup, a podcast catalog fetch found eight playable episodes, with
34,323 B free internal RAM at completion. Ten radio-to-podcast and ten
podcast-to-radio web selections then returned HTTP 200 and player `playing`;
20 more immediately alternating selections also returned HTTP 200. The
capture had no watchdog, abort or station recovery during those 40 local
selections, and the web player still responded afterward. Private numeric
CSV traces preserve request latency. This validates local command handling,
not audible handoff or all S2 transition requirements. Spotify-to-local
handoff and its memory result on this candidate remain pending a fresh
Spotify controller command.

While local radio played, `/api/player` also accepted minimum volume 0,
mute, a non-flat tone (bass +2, mid −2, treble +1), and maximum stored volume
21 while muted. Each response reported the expected state; the original
volume 1, unmuted, flat tone settings were restored. This does not establish
audible tone quality or Spotify's remote-volume behavior.

A final source-only correction clears the decoder's saved paused flag when
Spotify resumes without a new Load frame. The resulting 4,384,960-byte image
SHA-256 is `f7d81dcd97461729c1e0ca46910aa1a9d8b81885908a475d12abb4b8137dd249`;
it built, was flashed app-only and passed independent `verify-flash`. The
40-transition local run above applies to the preceding image, which differs
only in that return-play flag. The final image's phone-controlled cases
remain unverified. Its boot authenticated, both `/spotify_info` and
`/api/player` responded, one local podcast-to-radio pair completed, and
`/api/device` reported 35,927 B free heap at 117 seconds uptime. No watchdog
or abort was recorded in that short capture; no Spotify output command had
arrived yet.

### S2 discovery recovery: 2026-09-29

The owner reported that the radio was absent from Spotify while the iPhone
and radio were on the same Wi-Fi. Read-only HTTP checks reached the radio at
`192.168.11.199`, but `/spotify_info` returned 404. A fresh USB serial boot
logged ordinary HTTP mDNS startup and no Spotify service. `/api/device`
reported the normal 0.1.1 firmware built Sep 27 2026 20:57:27, rather than
the S2 candidate. OTA metadata was byte-identical to the Sep 28 backup:
sequence 1 selected app0. The normal image had therefore replaced the
experimental app0 image sometime after the previous trial; the cause and
time of replacement are not established. The updater's current check said
`Up to date (0.1.1)` and did not install anything during this observation.

Automatic firmware installation was changed from enabled to ask-first through
`/api/update/auto` for this trial. A fresh 5 MiB app0 backup attempt stopped
at 29% due to a USB serial read error; the existing private Sep 28 normal
app0 backup remains available and contains the same build date. The exact
previously verified S2 image (`f7d81dcd97461729c1e0ca46910aa1a9d8b81885908a475d12abb4b8137dd249`)
was written to app0 only. esptool verified the written data hash. NVS, OTA
metadata, the partition table and LittleFS were not written. The flash and
backup attempts are in the private recovery directory. Multiple visible
boots during diagnosis were caused by serial-port opens and esptool resets;
only one image was flashed in this continuation.

After startup, `/spotify_info` returned HTTP 200 and Spotify status 101;
`/api/player` also returned HTTP 200. `/api/device` reported a 4,384,960-byte
sketch and 41,715 B free heap at 50 seconds uptime. The update endpoint
confirmed `autoInstall=false`. These checks establish that the S2 image and
pairing endpoint are active again. The Mac is on a different routed subnet,
so its mDNS browse is not valid evidence for the iPhone's subnet.

The iPhone then discovered and selected Radiohead Native Test. The owner
initially heard sluggish audio, but after a serial-induced reboot and fresh
transfer reported clear Spotify playback. During a 165-second private
diagnostic capture, cspot authenticated using saved pairing, registered the
Spotify mDNS service, acquired output once and produced sustained 881,664–
882,688 PCM bytes per five-second window after startup. One window at about
68 seconds produced only 611,328 B; the buffer reached empty and recorded
176 empty polls, followed by recovery. The other sampled steady windows
had zero empty polls, partial I2S writes and write failures. Internal free
memory during Spotify playback was usually about 26 KB, with one sample at
23,915 B. This is a clear short listening report with one measured underrun,
not a clean stability pass.

A web `/next` command during Spotify playback returned HTTP 303 in 1.23 s,
but the owner heard silence afterward. The owner subsequently selected
גלגלצ locally and heard music. `/api/player` then reported `playing` on
גלגלצ; `/api/device` reported 35,767 B free heap, and five subsequent
player requests all returned HTTP 200. Because the clear local audio followed
the owner's separate selection, this does not pass the directed automatic
Spotify-to-local handoff. No serial transition log was captured for the web
command. Reverse local-to-Spotify playback remains pending a phone command.

### S2 closed-serial PCM correction: 2026-09-29

The owner again heard sluggish Spotify playback across multiple songs on the
restored `f7d81dcd` image. `/api/device` still reported its 4,384,960-byte
sketch, `/spotify_info` responded, and the updater remained in ask-first mode.
The radio's Wi-Fi RSSI was −51 dBm; 20 routed LAN pings had no loss, and 12
player requests returned HTTP 200 quickly. Those checks do not establish CDN
continuity. A prior serial-attached capture had one real PCM underrun, but
otherwise delivered the full 44.1 kHz stereo rate while the serial reader
was attached.

A 384 KiB PSRAM PCM queue with a 320 KiB start/refill threshold and a
read-only `GET /api/spotify/diagnostics` endpoint was built first. The normal
PlatformIO build passed at 96,404 B static RAM and 3,599,207 B linked flash;
the integrated image fit its app slot with 33% free. Its image hash was
`7e32a071177660313a5e1c37564b7d82b1d5707d9308b824682463fc5a3b3b35`.
It was flashed app0-only and its write hash verified. With USB serial closed,
28 steady HTTP samples each reported **534,528 output PCM bytes per five
seconds**, while the queue stayed nearly full and there were no I2S write
failures. The owner still heard sluggish/skipping audio. This shows that
buffering alone did not fix the repeatable slowdown.

The installed Arduino 3.3.11 `HWCDC::write` can wait up to 20 consecutive
100 ms TX timeouts when the USB host stays plugged in but stops consuming
serial output. The Spotify output task had printed a diagnostic line every
five seconds. The fixed PCM count above corresponds to about three seconds
of 44.1 kHz stereo output per five-second window; a roughly two-second USB
write stall is the supported cause. Keeping a serial monitor open had
masked the defect by draining those prints. Opening USB serial also resets
this radio, so serial-based playback tests are intrusive.

All serial writes were removed from the Spotify output task; its numeric
five-second counters remain available through the HTTP endpoint. The next
normal and integrated builds passed, and the integrated partition table
matched the normal one. Final image SHA-256 is
`32e24232ec8a09e69892a2c0ca5831ea704c12d21f9095529d7fab6b4ae26778`;
its 4,397,456-byte binary leaves 33% of app0 free. App0-only flashing passed
write-hash verification without writing NVS, OTA metadata, the partition
table or LittleFS. With USB serial **closed**, 19 steady HTTP samples
reported 881,664–882,688 output PCM bytes per five seconds, zero empty
polls and zero I2S write failures. The owner reported **crystal-clear**
Spotify audio. Private numeric captures are `buffer-trial-20260929.csv` and
`nonblocking-trial-20260929.csv` in the recovery directory.

After flashing, only an explanatory source comment and documentation were
edited. A final normal/integrated rebuild passed again and generated image
SHA-256 `380887a6769a850dacf6e5e4749b3a97260c1bac4973ee069243e5fdde5181ee`.
The radio was **not** flashed again after that comment-only edit; the
hardware-tested installed image remains `32e24232…`. The two builds have
the same 4,397,456-byte image size. `git diff --check` passed.

This is a confirmed short-run playback-rate correction. The enlarged queue's
effect on rare CDN gaps, remote-control delay, source handoffs, longer-run
memory trend, token renewal and Wi-Fi interruption still need device tests.
S1 recovery and S2 acceptance remain open. The opt-in Spotify image is still
installed and automatic firmware installation is still set to ask-first.

### S2 directed handoff probe: 2026-09-29

With the installed `32e24232…` image and USB serial closed, read-only HTTP
checks found the Spotify session ready and output owned, with 882,688 output
PCM bytes in the latest five-second interval. A single web `/next` selection
completed and redirected to the player page in about 2.02 s. The next player
response, about 0.3 s later, reported local station NYOX as `playing`, while
Spotify diagnostics reported `outputOwned=false` and an empty PCM queue.
Seven further one-second samples kept reporting local `playing` with HTTP
responses; a later device query reported 36,095 B free heap. The owner then
reported switching several times between radio and Spotify in both
directions, with each transition working very well. This passes the short-run
bidirectional audible handoff check by owner report, alongside the measured
single Spotify-to-radio control/I2S transition. The exact number and timing
of the owner's switches were not recorded, so they are not counted toward
the 100 directed plus 20 rapid S2 acceptance run.

### S2 counted local transitions and rapid-command failure: 2026-09-29

With the installed `ed4b809d…` integrated image and USB serial closed, the
podcast catalog loaded eight playable episodes. A radio/podcast warm-up reached
`playing` for both sources. The player initially held volume index 0; it was
temporarily set to 5 for this trial and restored to 0 afterward.

Ten radio-to-podcast and ten podcast-to-radio web selections then returned
HTTP 303 and reached the expected local `playing` state with Spotify output
unowned. The measured selection-to-`playing` medians were 9.51 s for podcast
and 2.42 s for radio (maxima 9.70 s and 3.26 s). These are web/player-state
observations, not first-audio or listening measurements. The private directed
trace starts at 29,743 B free heap and ends at 29,719 B; its sampled minimum
was 29,099 B. PSRAM changed from 6,373,248 B to 6,363,496 B across this one
batch, without equal post-run settling. These samples do not establish the
full S2 leak trend or resource budget.

The attempted rapid alternating batch stopped on its third command: podcast
selection exceeded the host's five-second HTTP timeout after a podcast and
radio command had returned HTTP 303. The device later reported podcast
`playing`, Spotify output remained released, and all three status endpoints
responded. This is a failed rapid-command acceptance attempt, with serialized
podcast `audio.connecttohost()` work in the web/main-loop path as a plausible
cause of the delayed response; no device trace proves the exact blocking span.
No rapid transition from that attempt is counted toward the required 20.

No phone-controlled Spotify transfer was observed during the private HTTP
capture, so the four Spotify-directed pairs, cross-source audible checks,
mute/tone/volume checks across every direction, and the 100-transition total
remain open. Evidence is under
`~/.radiohead-recovery/spotify-s2-20260929-acceptance/`; the HTTP capture
does not contain account or track identifiers.

### S2 phone-controlled short batch: 2026-09-29

The owner continued with the installed `ed4b809d…` image and USB serial
closed. An initial podcast-to-Spotify transfer sounded clear, with Spotify
owning output and full-rate PCM. A web Spotify-to-radio switch returned HTTP
303, reached local `playing` in 6.69 s, and sounded clear with no overlap.
The owner then transferred radio-to-Spotify with clear audio; a web
Spotify-to-podcast switch reached local `playing` in 7.92 s and also sounded
clear. A subsequent podcast-to-radio-to-podcast pair sounded clear in both
directions. These are individual directed listening checks, not ten counted
passes in each direction.

At the owner's request, a shorter phone-transfer batch was used. Its first
podcast-to-Spotify-to-radio-to-podcast cycle completed all device-state
checks. On the second phone transfer, the owner tapped Spotify Next. The
receiver briefly owned output, then released it; diagnostics changed to
`outputOwned=false` and `paused=true`, the local player stayed stopped, and
the owner reported that Spotify selected the phone and played there. No
local source was selected in that interval. After reselecting Radiohead, a
controlled Next tap played the next song on Radiohead with full-rate PCM.
The skip-to-phone failure is therefore intermittent on this image. The
four-cycle runner stopped on that failure; it did not manufacture a passed
four-cycle count. A later podcast-to-Spotify transfer again sounded clear.

Spotify Pause kept Radiohead selected and silent with its session and output
owned; Play resumed clear audio without reselecting the device. During
Spotify playback, local mute stayed active while the owner changed phone
volume and turned the encoder. Stored volume 0 and 21 were accepted while
muted, mute remained active, and volume 11 was restored before unmuting.
Bass +2, mid −2 and treble +1 sounded clear without clipping or pops on
Spotify and through Spotify-to-radio-to-podcast; the owner heard both local
sources clearly. Tone was restored to flat. At the end, the player reported
volume 11, unmuted and flat tone.

The 15-minute private HTTP continuation has more than 400 status samples and
uptime increased without a reset. Its lowest sampled free heap while Spotify
owned output was 17,327 B; local free heap varied with stream and catalog
work. These are `ESP.getFreeHeap()` samples, not full internal/DMA/largest-block
measurements or an equal-settling leak test. HTTP polling cannot identify
the cspot event that caused the phone fallback. Evidence is in
`~/.radiohead-recovery/spotify-s2-20260929-acceptance/` as
`http-samples-continuation.jsonl`, `events-continuation.jsonl`, and
`directed-batch.jsonl`; no account or Spotify track identifiers were saved.

**S2 cannot pass on this image.** The intermittent Next-to-phone regression
needs a cause and retest, and the required 100 directed plus 20 rapid or
interrupted transitions remain incomplete. The shorter batch ended before
the directed transition count could be completed.

### S1 stream-open exception correction: 2026-09-29

The run-19 abort was traced to an uncaught TLS exception in
`CDNAudioFile::openStream()`. A new opt-in cspot patch catches standard
exceptions at that decoder call, releases the failed stream, resets to the
current queue head, and retries after a 500 ms delay. It avoids the recorded
uncaught-exception reboot path; actual Wi-Fi reconnection and audio resumption
still require a device trial. The normal PlatformIO build passed at 96,404 B
static RAM and 3,599,207 B linked flash. The integrated build passed in a
fresh build directory; its 4,397,728-byte app image has SHA-256
`ed4b809d5b8d317f7f61039df195f5f4135109780b078cf12f02cf5c1d123092`,
leaves 33% of app0 free, and has a partition table byte-identical to the
normal build. The candidate was not yet flashed at this ledger update.

The candidate was subsequently flashed app0-only and independently passed
`verify-flash` against all 4,397,728 image bytes. OTA metadata still selected
app0 and matched the Sep 28 backup; the flash partition table matched the
normal build. NVS, OTA metadata, partition table and LittleFS were not written.
A full readback of the previously installed app image failed due to a USB
serial-stream error, so the last working comment-only rebuild is retained
privately as a rollback image. The new image booted, authenticated, and
served `/spotify_info` and `/api/spotify/diagnostics` with USB serial closed.

During a user-controlled Wi-Fi interruption, HTTP polling first failed at
113.755 seconds into the capture and resumed at 145.804 seconds. The Spotify
session and output were still active on return. Five-second output PCM
recovered to 881,664–882,688 bytes, with zero empty polls and zero I2S write
failures in subsequent samples. The device reported 229 seconds of uptime
after recovery, consistent with no reboot during this capture. The owner
confirmed that Spotify resumed clearly on its own, without phone selection.
This passes one short-run integrated Wi-Fi recovery case; repeated outages,
long-run renewal, and the formal S1/S2 acceptance gates remain open. Private
flash/verification logs and the 240-second HTTP trace are under
`~/.radiohead-recovery/spotify-s2-20260929-wifi/`.

## Baseline before changes

- `pio run -e esp32s3` **PASS** at source `b47961c` on 2026-09-24.
- PlatformIO platform 55.3.311; Arduino framework 3.3.11; ESP-IDF libraries
  5.5.5 `b774170ff46`; toolchain xtensa-esp-elf 14.2.0+20260121;
  vendored ESP32-audioI2S 4.0.0; LovyanGFX 1.2.21; ArduinoJson 7.4.3.
- Linker static RAM: 96,340 / 327,680 bytes (29.4%). Firmware: 3,491,471
  bytes reported by PlatformIO. Actual `firmware.bin`: 3,542,448 bytes.
  Each OTA application slot is 0x640000 = 6,553,600 bytes; baseline image
  headroom is 3,011,152 bytes. Static reports do not establish runtime fit.
- `git diff --check` **PASS** before changes. Existing untracked `build/`,
  `docs/spotify-integration-research.md` and `docs/spotify-native-validation.md`
  predate this branch and are preserved.
- USB serial `/dev/cu.usbmodem11201`, USB VID:PID 303A:1001, reported as ESP USB
  JTAG/serial debug unit. `esptool chip-id` identified ESP32-S3 revision 0.2,
  8 MB embedded PSRAM. User identifies the currently attached speaker module as
  MAX98357A; one channel is installed, with a second planned.
- Private recovery directory: `~/.radiohead-recovery/spotify-s0-s1-20260924/`
  (mode 0700). It contains source baseline firmware (SHA-256 prefix `b76d6567`),
  a successful 20,480-byte NVS read, and an 8,192-byte OTA metadata read, all
  mode 0600. OTA metadata reports app0 as active. A full-flash read at 460800
  and app0 read at 115200 both failed with `Serial data stream stopped` after
  partial transfer; no full-flash backup is claimed. Neither failed read
  modified flash. Recovery of the known source build, preserving NVS/LittleFS:
  `pio run -e esp32s3 -t upload --upload-port /dev/cu.usbmodem11201`.
  A full original binary restore is unavailable; preserve the private NVS
  backup and do not erase the chip.

## S0 runs

- Instrumentation is gated by `esp32s3-validation`. It samples every five
  seconds, uses fixed event and gap buffers, records internal/DMA/PSRAM heap
  capabilities separately, allocation failures, task stack reserves, audio
  input fill and main-loop/audio/web call gaps. IDF 5.5.5 documents task high
  water marks in **bytes**. Source 0=stopped, 1=radio, 2=podcast.
- `pio run -e esp32s3-validation` **PASS**: 96,612 bytes static RAM versus
  96,340 normal baseline (+272); 3,494,831 bytes linked flash versus
  3,491,471 (+3,360). Diagnostic firmware was uploaded to the identified
  ESP32-S3 with esptool write verification **PASS**. NVS and LittleFS partitions
  were preserved. This measures static overhead only; runtime cost is pending.
- Private capture `s0-serial.log` completed after 30 minutes. It contains 358
  five-second samples spanning 1,787.3 seconds (the capture began before the
  first sample and ended after the last). Source counts: 171 radio samples
  (~14m15s) and 187 podcast samples (~15m35s); the boot and transition gaps
  complete the 30-minute wall-clock capture. The user reports radio and
  recorded show playback, volume, mute, station selection, screen and web
  actions all worked and sounded good. Exact action times, station/show IDs
  and HTTP response latency were not recorded. Log summary command:
  `python3 tools/spotify_validation_summary.py ~/.radiohead-recovery/spotify-s0-s1-20260924/s0-serial.log`.

  | Source | Samples | Least free internal / DMA / PSRAM | Least audio-task stack reserve | Largest loop gap / audio call |
  | --- | ---: | ---: | ---: | ---: |
  | Radio | 171 | 39,804 / 32,140 / 6,656,700 B | 404 B | 1,596,289 / 217,156 us |
  | Podcast | 187 | 43,756 / 38,524 / 6,912,928 B | 404 B | 1,348,937 / 9,177 us |

  The three heap capabilities overlap and cannot be summed. Across all
  samples, the smallest largest allocatable block was 26,612 B for internal
  and DMA, and 6,553,588 B for PSRAM. The boot-to-date minimum free was
  17,744 B internal, 10,080 B DMA, and 6,656,700 B PSRAM. The maximum
  five-second-window p95 loop-gap bucket upper bound was 20,000 us; max
  `audio.loop()` call 217,156 us; max `server.handleClient()` call 6,688 us.
  Maximum task count was 16. Lowest stack reserves were 10,228 B loop,
  3,276 B controls, 404 B audio and 7,464 B updater (ESP-IDF units: bytes).
  Input fill minimum was 10,634 B. These are S0 baseline observations, not
  S3 acceptance measurements: web end-to-end latency, actual PCM underruns,
  and I2S errors were not directly measured.
- The uninstrumented normal build after source changes also **PASS** with
  unchanged 96,340-byte static RAM. Its linked flash report is 3,491,499 bytes.

## S1 runs

The standalone candidate built reproducibly from
`experiments/spotify-native/prepare.sh` plus the pinned patches on ESP-IDF
5.5.5. The current run-14 image is 1,769,888 B, leaving 4,783,712 B of a
6,553,600 B app slot. Its partition-table binary is byte-identical to the
Radiohead table. Flashing only app0 at `0x10000` completed with hash
verification; NVS, otadata, bootloader and LittleFS were left in place.
The run-6 flashed image SHA-256 begins `7046dd096890485f`; run 7's flashed
image begins `f2f61bd9573a7cff`.
Run 8's flashed image begins `93d70c409bb2610f` and retains the same
1,763,040-byte image size; its TLS memory allocation mode differs.
Run 9 uses a 1,768,304-byte image with software mbedTLS AES, SHA-256 prefix
`cde7ae5dabc97faf`. Run 10 modifies the I2S write loop and telemetry,
producing a 1,768,560-byte image with SHA-256 prefix `42dda8052a8ab31b`;
its measured runtime failure is below. Run 11 fixes the I2S timeout units and
was flashed and hash verified (SHA-256 prefix `6e9777557ffc8003`). A fresh
clone, pinned submodules and run-11 patches built independently to a
1,768,656-byte image.
Its generated configuration confirms external mbedTLS allocation and software
AES, and its partition table matches the Radiohead binary byte for byte.
An independent fresh clone plus patches of the earlier run-6 candidate built
successfully on the same IDF 5.5.5 toolchain. Its size report showed
237,435 / 341,760 B DIRAM used (69.47%), with 104,325 B static headroom.
The earlier independent binary was 1,779,296 B; the 96-byte difference in
the current fresh build was not investigated. Static headroom is separate
from runtime heap.
The flashed run-7 build reports 221,059 / 341,760 B DIRAM used (64.68%),
120,701 B static headroom. Its runtime limits are reported with run-7 logs,
not inferred from the linker report.
Wi-Fi association and DNS succeeded on device. The first provisioning attempt
failed because `fgets(stdin)` returned EOF with the default USB serial/JTAG
console. The second candidate installs the USB serial/JTAG driver and reads
bounded lines directly. It stored the two privately supplied app credentials
in the `spotify` NVS namespace, started HTTP port 8080 and advertised the
pairing service. A redacted host GET of `/spotify_info` returned HTTP 200,
status 101, with no active user before phone pairing. The phone then supplied
a login blob; it persisted across reboot. The native AP connection and
authentication succeeded, and the device fetched one CDN access token. A
track file selection, key exchange and CDN URL fetch reached the device in
failed runs. Run 10 produced audible but unintelligible sound, so S1 is
**FAIL**. Run 11 fixed the I2S write errors but exhausted internal memory
during playback. The user later confirmed that its audio sounded good before
the memory-related failure, including after song changes.

Run-6 standalone diagnostic samples at paired, authenticated and output
idle states show 103,983 / 74,931 / 42,903 B least free internal RAM, with
96,195 / 67,143 / 42,903 B DMA free and 8,343,952 / 8,343,952 /
8,111,984 B PSRAM free, respectively. These are **not** track-decoding minima.
The boot-to-date DMA minimum is 4,648 B and internal minimum 12,436 B in
the output-idle sample; largest internal/DMA block is 30,720 B. Main task
stack reserve at authentication is 24,856 B, output task minimum 14,268 B,
queue task minimum 23,616 B so far. Decoder stack has not been measured
during active decode. Capability totals overlap; do not add them.

- A private file containing the developer app Client ID and Secret was
  provisioned by the user outside the repository with mode 0600. A host-side
  HTTPS `client_credentials` probe on 2026-09-24 returned HTTP 200, an access
  token and `expires_in=3600`; neither credential nor token was logged. This
  verifies only app credentials and token issuance. It does **not** verify CDN
  authorization, Premium Connect pairing, native renewal or ESP32 playback.

## Failures and unresolved decisions

- The Waveshare native example is pinned at `9c51b087`, cspot `37b650a5`,
  Bell `ead27050`, mdnssvc `44e78c14` and nlohmann/json `07182ebc`.
  It uses an ESP-IDF 6.0 build, a board-specific ES8311 I2S codec and a
  compiled Spotify client secret for CDN tokens. Its audio and authentication
  paths cannot be used unchanged on this Arduino board. Compatibility and
  secret-free native authorization remain open.
- The user identified MAX98357A; the firmware uses BCK 15, WS 17 and DOUT 16.
  Wiring has not been electrically measured; audibility is still pending.
- The S0 run recorded one failed internal 16,717-byte allocation
  (`MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT`), a 404-byte low audio-task stack
  reserve and three station recovery scheduling events (two observed station
  connection timeout messages in the log). User listening remained good.
  These are existing radio findings, not Spotify regressions. One reported
  input fill exceeded the library's nominal capacity; this could be a
  sampling/interpretation issue and needs investigation before claiming a
  buffer defect.
- The Waveshare example uses a distinct SPIFFS partition with
  `format_if_mount_failed=true`, while this radio's data partition holds
  LittleFS artwork. Flashing it unchanged could destroy saved artwork. Its
  partition layout and credentials header are therefore unsuitable.
- A standalone candidate build under `/tmp` with the current pioarduino
  PlatformIO platform selected ESP-IDF 5.5.5, not the example's IDF 6.0. After
  selecting the example's `main` source directory, PlatformIO failed before
  compilation with `ModuleNotFoundError: SCons.Tool.FortranCommon`. This is a
  host build-system failure, not a cspot compatibility result.
- Failed S1 boot 1: default `fgets(stdin)` returned EOF instead of waiting
  for provisioning. Using the ESP32-S3 USB serial/JTAG driver for bounded
  private input fixed this; credentials are now in NVS, not compiled in.
- Failed S1 boot 2: mbedTLS 3.6 under ESP-IDF 5.5.5 returned `-0x7400`
  (`MBEDTLS_ERR_SSL_NO_RNG`) because the upstream candidate assumes mbedTLS 4
  supplies the RNG. Adding `mbedtls_ssl_conf_rng` with ESP32 hardware random
  input allowed a real AP TLS connection and authentication.
- Failed S1 boots 3–5: `cspot_player` needed a contiguous 32 KiB internal
  stack after dynamic TLS/task allocations and aborted, despite >100 KiB
  total internal free. The candidate had also statically reserved 131 KiB
  for a nominal 32 KiB main-task stack because `StackType_t` is four bytes.
  Correcting the declaration freed 98,304 B of static DRAM, but fragmentation
  still prevented the later 32 KiB decoder allocation. A 16 KiB decoder
  stack then overflowed when opening the CDN stream. The 24 KiB queue stack
  also overflowed during CDN URL fetch. Both are now reserved as 32 KiB
  static internal stacks; the current rerun has no early abort, but has not
  yet been driven through a track. These failures are S1 candidate failures,
  not S0 regressions. The working native configuration must retain measured
  stack and heap headroom before S1 can pass.
- Failed S1 run 6: a real track selected Ogg Vorbis format 1, retrieved its
  audio key and CDN URL and opened a CDN stream. A concurrent metadata/CDN
  request drove free internal RAM to 2,063 B (largest block 576 B), DMA free
  to 1,207 B (largest block 16 B), and boot-to-date minima to 64 B internal
  and 12 B DMA. `esp-aes` then failed to allocate memory; the queue task
  triggered repeated watchdog reports. No audible playback or clean track
  completion was established. The latest candidate reduces the main stack
  from 32 to 16 KiB and the output stack from 16 to 8 KiB based on observed
  reserves, and adds an allocation-failure counter. Run 7 is currently
  collecting evidence. Any further allocation failure, overflow, watchdog or
  inaudible output blocks S1.
- Failed S1 run 7: reducing the measured overlarge main/output stacks raised
  free memory at authentication, but a real track still selected format 1,
  opened one CDN stream, and started a concurrent CDN request. Free internal
  RAM fell to 2,247 B (largest block 448 B) and DMA to 1,387 B (largest
  block 184 B). There were at least nine allocation failures, two explicit
  AES allocation errors and repeated queue-task watchdog reports. The
  candidate did not deliver accepted audio. ESP-IDF 5.5.5's supported
  `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` was enabled for run 8 to move
  transient TLS allocations to the abundant PSRAM. ESP-IDF recommends
  internal TLS allocation for physical security unless external RAM is
  encrypted. Run 8 is provisional and needs both functional and resource
  evidence; this choice is not an integrated firmware decision.
- Failed S1 run 8: external mbedTLS allocation raised free internal memory
  during output idle, but concurrent CDN stream requests still exhausted the
  internal DMA path used by hardware AES. At the first error, output samples
  showed 39,231 B internal free and 33,207 B DMA free, falling to 15,239 B
  internal and 9,347 B DMA with a 6,144-byte largest block. One allocation
  failure, an explicit `esp-aes` failure and repeated decoder watchdog
  reports followed. Run 9 disables ESP-IDF's hardware AES acceleration so
  TLS uses software AES without the extra internal DMA buffer. Its CPU/audio
  cost must be measured; no S1 acceptance is inferred from the build.
- Failed S1 run 9: software AES avoided the earlier hardware-AES DMA error
  long enough to open three CDN streams and advance through a track, but the
  sink discarded thousands of partial I2S writes (3,458 in the first 63
  seconds), then internal free RAM declined to 159 B with at least 75 failed
  allocations. This is neither acceptable sound nor memory stability.
  Run 10 loops over partial I2S writes with a 200 ms deadline, records PCM
  bytes/short writes/failures, and suppresses per-write log flooding. The
  remaining heap decline must be measured again; its cause is not yet proven.
- Failed S1 run 10: over 356.7 s of capture, two tracks started and two CDN
  streams opened. The sink accepted 1,454,336 PCM bytes but recorded 14,867
  partial writes and 12,804 failed writes; internal free fell to 6,191 B,
  DMA free to 2,319 B and at least two allocations failed. The user heard
  audio but described it as very bad and unintelligible. The installed IDF
  5.5 I2S API expects a timeout in milliseconds; the sink passed
  `pdMS_TO_TICKS(50)`, giving a roughly 5 ms wait with this 100 Hz tick.
  Run 11 passes 50 ms directly. The timing error is proven in source, while
  its contribution to the heap decline and audible fault still needs a
  device measurement.
- Failed S1 run 11: app-only flash and image hash verification passed. The
  saved pairing authenticated after reboot and fetched one authorization
  token. Across 848.2 s of capture, three tracks started, one ended, and
  three CDN streams opened. The sink accepted 35,566,932 PCM bytes with zero
  partial writes or I2S write failures, including about 176,400 B/s during
  steady 44.1 kHz stereo output. Internal free RAM fell to 147 B (largest
  block 104 B), DMA free to 143 B (largest block 104 B), and at least 125
  allocations failed. PCM output stopped after the next track change. This
  disproves the idea that the I2S timeout was the sole cause of the earlier
  memory decline. The user reported clear audio and clear song changes before
  that failure.
- S1 run 12: the IDF 5.5.5 `SPIRAM_MALLOC_ALWAYSINTERNAL` threshold is
  changed from 16,384 to 1,024 B so ordinary larger allocations prefer
  PSRAM, while explicit internal/DMA allocations retain their capability.
  The allocation-failure callback now records the last failed capability
  mask for diagnosis. The candidate compiled, its 1,768,576-byte app-only
  image was flashed and hash verified, and device capture is running. A fresh
  clone with pinned submodules and current patches built independently to a
  1,768,688-byte image. Its linker size report is 221,119 / 341,760 B
  DIRAM used (64.7%, 120,641 B static headroom). Its generated configuration
  selects the 1,024 B threshold, external mbedTLS allocation and software AES,
  and its partition
  table matches the original candidate. Early device capture has five
  tracks started, two finished and five CDN streams opened; at 166.5 s it
  reports zero allocation and I2S write failures, at least 70,387 B free
  internal RAM, 62,599 B DMA RAM and 7,992,680 B PSRAM during output.
  The user reports clear audio and clear song changes. This is provisional;
  sustained headroom, controls, renewal and restart remain to be measured.
- The user then reported completing at least 20 deliberate song changes,
  pause/resume, seek, remote volume changes and transfer away from and back
  to the native device. They heard clear playback with no gaps, distortion or
  stale audio. At 392.4 s, the device capture recorded 12 actual track starts,
  32 file selections, nine completed tracks, ten producer cancellations
  during changes, 40,699,360 accepted PCM bytes and zero allocation, partial
  I2S write or I2S failure counts. The capture does not count every phone
  button press, so the 20 deliberate changes are a user report. Two-hour
  renewal and physical recovery tests remain pending.
- Around 18 minutes into run 12, PCM stopped after a song ended. The user
  reported that Spotify already showed “This phone” as the selected output
  before the radio song ended, with no intentional transfer at that time.
  The radio stayed paired, did not reboot, and retained over 90 KiB free
  internal RAM while idle. Playback resumed when the user selected the radio
  and restarted the playlist from the phone. The current capture has no
  positive cause for the spontaneous Connect device switch, so this is an
  unresolved S1 functional failure even if later playback stays healthy.
- Source audit after the stop found that the standalone MAX98357A adapter
  ignored cspot's `DEPLETED` event. The pinned cspot CLI reference holds this
  event until its PCM buffer drains, then calls `notifyAudioEnded()` so
  Spotify receives the end-of-audio state. The candidate currently had no
  equivalent call. Run 13 adds the same deferred handoff with two low-rate
  event logs; this is a plausible cause of the stale Connect state, not yet
  established by device evidence.
- Run 12 ended after 1,883.5 s of private capture: 17 track starts, 14
  completed tracks, 17 CDN opens, 277,571,660 accepted PCM bytes, zero
  allocation failures, zero partial I2S writes and zero I2S failures. The
  least sampled output free was 67,091 B internal, 59,303 B DMA and
  7,960,680 B PSRAM; capabilities overlap. A second authorization token was
  fetched successfully 1,800.856 s after the first, and PCM continued
  afterward. This passes the observed renewal subcheck, but the spontaneous
  phone transfer and run duration under two hours still fail full S1.
- Run 13 compiled from both the working tree and an independently prepared
  pinned checkout, and its 1,769,152-byte app-only image flashed with hash
  verification (SHA-256 prefix `e957d04fb1272da8`). It implements the
  deferred `DEPLETED` handoff and is now
  collecting a fresh device capture. The phone must replay a playlist across
  natural song boundaries to test whether the Connect state remains correct.
- On run 13 the user observed one natural song transition: the next song
  started and the radio remained selected. At 383 s, the device had four
  track starts, one finished track, 48,532,480 accepted PCM bytes and zero
  allocation or I2S failures. The `DEPLETED` handoff had not fired yet, so
  this ordinary transition does not prove the earlier queue-boundary issue
  is fixed. A repeated skip-to-boundary test is in progress.
- Failed S1 run 13: after 831.3 s of capture, the user again observed the
  iPhone revert to itself as output without touching its selector or leaving
  Wi-Fi. The radio continued its buffered song, then became silent. Five
  tracks had started, three ended, and five CDN streams opened; 109,506,304
  PCM bytes were accepted with zero allocation or I2S write failures. Least
  sampled output free was 69,715 B internal and 61,927 B DMA. Two incoming
  “another player took control” notifications appeared about 47 seconds
  after the final song ended, but neither `DEPLETED` nor the new end-of-audio
  notification fired. The user confirmed more songs remained in the phone's
  playlist. The missing `DEPLETED` handoff is therefore not the cause of this
  reproduction. Run 14 adds bounded, identifier-free queue-state and
  Zeroconf request diagnostics to identify why the native queue did not
  advance; it does not claim a functional fix.
- Run 14 built from the candidate working tree and from an independently
  prepared pinned checkout. The fresh binary is 1,770,000 B; the flashed
  app-only binary is 1,769,888 B (SHA-256 prefix `1b72948e961102be`), with
  hash verification. Its private capture lasted 1,185.9 s. Four tracks
  started, three finished, 158,912,876 PCM bytes were accepted and zero
  allocation/I2S write failures were observed. Least sampled output free was
  71,667 B internal and 63,879 B DMA. The user reports that opening Spotify
  on the iPhone caused the app to revert to “This phone” without touching the
  output selector; the radio remained listed as an available device and kept
  playing until its current song ended. No competing-device notification or
  Zeroconf request preceded that change. After the third natural EOF,
  `queue_finished=0` but the decoder repeatedly waited with
  `index=0 refs=50 preloaded=3 offset=3 prev_found=1`. The candidate had
  exhausted its three preloaded tracks while leaving the playlist index at
  zero. Instrumentation logs
  queue index, reference count, preloaded count and offset when the decoder
  waits, plus `queue_finished` at EOF and counts of Zeroconf GET/POST
  requests. It never logs their bodies, track identifiers or account data.
- Source inspection identified a missing call to cspot's
  `notifyAudioReachedPlayback()` in the candidate output task. The cspot
  CLI reference calls it when a new track's PCM reaches output; that call
  advances the playlist index, refills the preload queue and sends a Connect
  state notification. Run 15 records bounded track-boundary positions in
  the PCM queue and calls the handler when output consumes each boundary.
  It was built and flashed app-only with hash verification (1,772,688 B,
  SHA-256 prefix `e0d726b8e576bddb`). A fresh pinned checkout and patch
  application also built successfully to 1,772,800 B, with a byte-identical
  partition table. The private run-15 capture lasted 917.0 s: five track
  starts, three natural endings with more tracks queued, four output-boundary
  notifications, five CDN opens and 151,387,796 accepted PCM bytes. No
  allocation, partial I2S write or I2S failure was recorded. Least sampled
  output free was 68,671 B internal, 61,239 B DMA and 7,974,772 B PSRAM;
  stack reserve minima were 4,820 B decoder, 5,580 B output and 7,808 B
  queue. The fourth song started naturally, beyond the old three-track
  preload limit. The user opened Spotify on the iPhone and reported that
  the radio stayed selected and sound stayed clear. This verifies the
  reproduced failure for this run, but not the two-hour S1 gate.
- Run 16 changes the standalone PCM producer callback to return actual
  accepted bytes. Cspot's existing retry loop now waits for output buffer
  room during a long pause rather than discarding PCM after a two-second
  timeout. The candidate builds to 1,772,400 B; a fresh pinned checkout
  plus the same patches built to 1,772,496 B with a byte-identical partition
  table. App-only flashing completed with hash verification (SHA-256 prefix
  `fa83ae77ddc94eb5`). The user paused and resumed playback and reported
  clear, correct audio afterward. The private run-16 capture was stopped
  at the user's request after 1,547.4 s (25m47s): one boot and paired
  session, nine track starts, seven finishes, eight output-boundary
  notifications, five natural EOF events with more songs queued, nine CDN
  opens and 241,385,412 accepted PCM bytes. There were zero queue-wait
  samples, PCM cancellations, allocation failures, partial I2S writes,
  I2S write failures, watchdogs, panics or aborts in the captured log.
  The pause state was sampled 24 times, followed by resumed output;
  the user confirmed the audio resumed clearly on the correct song.
  Least sampled output free was 69,467 B internal, 61,679 B DMA and
  7,989,236 B PSRAM; during pause it was 69,195 / 61,407 /
  7,958,468 B. Minimum stack reserves were 4,884 B decoder, 5,580 B
  output and 7,488 B queue. One authorization token was fetched in this
  shorter run; actual renewal was demonstrated on run 12, whose native
  authentication path is unchanged in run 16. The iPhone was not
  rechecked on every minute of this run, so the log alone establishes
  output activity, not continuous listening quality.
- Short-run continuation on 2026-09-24: a pinned, independently prepared
  run-16 checkout rebuilt successfully with ESP-IDF 5.5.5. The 1,772,496-byte
  image (SHA-256 prefix `3dabee13ceb8e0db`) fits the 6,553,600-byte app slot.
  Before flashing, a fresh read of the device partition table matched both
  the Radiohead and candidate binaries byte for byte. Fresh OTA metadata had
  valid sequence 1 in copy 0 (CRC `0x4743989a`), selecting app0 at `0x10000`;
  copy 1 was invalid. App0-only flashing and the write hash check passed.
  NVS, OTA metadata, bootloader and LittleFS were not written. The normal
  Radiohead build also passed (96,340 B static RAM; 3,491,499 B linked flash).
- The user reported 20 deliberate Next taps plus pause/resume, seek, volume
  changes and transfer to the phone and back, with clear audio afterward on
  this short-run image. These button presses are a user report; serial records
  decoder starts and output boundaries, not each remote command. The running
  private capture had 15 decoder starts, two natural EOF events, 11 output
  boundaries and 48 CDN opens as of the initial ledger update. Across its
  samples to that point, minimum free internal/DMA/PSRAM was
  67,207 / 59,419 / 7,961,124 B, with zero recorded allocation failures,
  partial I2S writes, I2S write failures, queue waits, watchdogs, panics or
  aborts. These counts span setup and controls and are not a two-hour test.
- The requested adjacent duplicate-song test is **inconclusive**. The phone
  playlist UI would not accept a duplicate entry. The user tried queueing a
  song twice, but an unexpected song played and the phone queue order was not
  confirmed. Repeat One was unavailable. The decoder log cannot prove that
  two adjacent instances were actually sent to the candidate. The source
  risk from file-identifier-based boundary detection remains open.
- Cold restart 1: the user removed power and reconnected the device. The
  serial connection dropped for about 14 seconds and a new boot reached Wi-Fi
  and native authentication with the saved pairing. A read-only host GET of
  `/spotify_info` returned HTTP 200 and Spotify status 101. The radio appeared
  on the phone, but selection failed twice while the phone had no active song,
  including after reopening Spotify. Starting a fresh song on the phone and
  transferring it to the radio then worked with clear audio. This is a
  reproducible selection precondition/limitation, not a failed cold boot.
  A brief USB reconnect between cold restarts 1 and 2 was not confirmed as a
  power-off restart and is excluded from the three-restart count.
- Cold restarts 2 and 3: the user removed all power for at least five seconds
  each time, started fresh phone playback, and transferred it to the radio.
  Both transfers worked and audio was clear. Thus three physical cold boots
  were reported and playback succeeded after all three once phone playback
  was active. These tests used the original short-run image, before the
  Wi-Fi recovery patches; they do not validate those later images.
- Wi-Fi interruption on the original short-run image: the user disabled the
  router Wi-Fi for about 15 seconds. The device did not reassociate or resume
  ping/playback. The final 3,193-second capture had 23 decoder starts, two
  natural EOF events, 16 output boundaries, 63 CDN opens and 84 task-watchdog
  events in `cspot_decoder`. Symbolization placed the decoder in Bell's HTTP
  response-header parser after a dead socket. No allocation, partial-I2S or
  I2S-write failures were recorded. The previous interim zero-watchdog
  observation applied only before this interruption.
- Recovery patch iteration (run 18): four standalone-only patches added a
  Wi-Fi station-disconnect handler, CDN range retry, HTTP response EOF check,
  and then connection-lifetime synchronization. Run 18 tested the first three
  patches. The user toggled router Wi-Fi multiple times. The radio regained
  IP and resumed PCM after two outages (`cdn_range_recovered` twice), but
  Spotify moved playback to the phone. A later outage caused a LoadProhibited
  panic in `ShannonConnection::sendPacket` while `MercurySession::reconnect`
  replaced the shared connection. Its 10m24s private capture recorded five
  reconnect and got-IP events, four CDN range retry logs, one range timeout,
  one panic and one subsequent abort; no task watchdog or sampled allocation/
  I2S failures. Minimum sampled free internal/DMA/PSRAM was
  68,087 / 60,299 / 7,956,708 B. A follow-up patch uses atomic shared-pointer
  snapshots for the session connection and removes recursive reconnect calls.
- Run 19 rebuilt the pinned standalone candidate with all four recovery
  patches. Before the app-only upload, a fresh partition read matched both
  candidate and Radiohead layouts and valid OTA sequence 1 still selected
  app0 at `0x10000`; the flashed 1,775,072-byte image passed write-hash
  verification. The user transferred a fresh song and heard clear audio.
  They then disabled router Wi-Fi for about 15 seconds. The radio rejoined
  after an abort/reboot, but Spotify moved output to the phone. The private
  3m54s capture recorded one `wifi_reconnect`, one post-reboot `wifi_got_ip`,
  two CDN retry logs, a 30-second range timeout, one abort and no watchdog.
  Addr2line traced the uncaught exception to `bell::TLSSocket::open` from
  `CDNAudioFile::openStream`, following range timeout. There was no recovered
  CDN range in this run. The crash prevents claiming Wi-Fi recovery; the
  run-18 connection-lifetime fix does not address this separate exception.
  Minimum sampled free internal/DMA/PSRAM was
  65,283 / 57,495 / 7,991,612 B; sampled allocation failures, partial I2S
  writes and I2S write failures were zero. PCM counters reset on reboot, so
  their maxima are not a whole-run output total.
- After run 19, a fresh partition read still matched the Radiohead layout;
  valid OTA sequence 1 selected app0. The normal 3,542,480-byte Radiohead
  image was flashed to app0 only and its write hash verified. NVS, OTA data,
  bootloader, app1 and LittleFS were not written. A 30-second private serial
  capture after restoration had no panic, abort or watchdog. The user then
  confirmed that normal radio playback and the screen both work.

## Handoff after short-run continuation

The user chose to skip the continuous two-hour test and preserve the
diagnostics for later work. **S0 is complete; S1 fails Wi-Fi interruption
recovery and is not a formal pass.** Run 15 fixed the reproduced
phone-state/three-song stall:
run 14 had zero output-boundary notifications and 32 queue-wait samples,
while runs 15 and 16 had four and eight notifications respectively and
zero queue-wait samples. The user reported that Spotify kept the radio
selected when the iPhone app opened on run 15, and that run 16's longer
pause resumed clearly. The latest recovery image has not had a two-hour
continuous test or its own observed token renewal. Earlier run 12 did renew a
native authorization token after 1,800.856 s with PCM continuing.

Private evidence and recovery material are in
`~/.radiohead-recovery/spotify-s0-s1-20260924/`. The directory is
mode 0700, its files mode 0600. `MANIFEST.md` and
`SHA256SUMS.private` describe/checksum the captures and backups without
publishing credentials. The provisioning file and NVS backup may contain
secrets; keep this directory private and out of git. The reproducible
source is in `experiments/spotify-native/`; the summary scripts are in
`tools/`. The earlier run-16 restore used PlatformIO upload and a private
45-second boot capture. The latest restore after run 19 used a verified
app0-only flash; the user confirmed normal radio playback and screen.
Normal web routes and controls were not rechecked after this restore.

Before declaring S1 complete, fix the Wi-Fi/stream-open failure and repeat
on the corrected final candidate image:

1. Two hours of continuous playlist output with the phone and Mac doing
   no playback work, including a token renewal in that same run and
   periodic listening checks. The user explicitly skipped this long test
   in the current continuation; it remains a formal S1 gate.
2. Three genuine power-off/on restarts and one Wi-Fi interruption with
   reconnection and resumption, while preserving NVS/LittleFS. Three restarts
   passed with active phone playback on the earlier image; Wi-Fi recovery
   failed on all tested images and the latest image aborted after timeout.
3. Remote pause, seek, volume, transfer away/back and at least 20 track
   changes on the corrected image. The user reported these working on the
   earlier short-run candidate, but the later recovery image needs its own
   checks after the Wi-Fi failure is fixed.
4. A playlist with two adjacent instances of the same song. The current
   output marker detects track changes by file identifier, so adjacent
   identical files may suppress the second boundary notification. This
   is a source-audit risk, not an observed device failure.

No Spotify UI or integration into the normal Radiohead services was
started. The standalone IDF candidate remains a protocol/audio spike;
S2 coexistence and resource validation are separate future work.
