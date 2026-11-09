# Spotify Stop / disappearing receiver investigation

The owner reports that pressing Stop in Spotify on the Mac sometimes crashes
Radiohead, and sometimes leaves it running but absent from Spotify's device list.
These are separate observations; one root cause has not been established for both.

## Captured evidence

Read-only HTTP requests to the radio on November 9 retrieved a preserved valid
core-dump summary. The exact matching archived ELF resolves this call chain:

```
loopTask -> loop -> serviceDisplayRefresh -> renderRadioUi
 -> renderSpotifyPlayer -> drawPlayerIcon (previous-track icon)
 -> LGFXBase::draw_png -> png_draw_alpha_callback
 -> readRect<bgra8888_t> -> Panel_Sprite::readRect -> copy_rgb_affine
```

The fault is StoreProhibited (exception cause 29), with fault address zero, in
`loopTask`. The captured firmware ELF hash is
`fd440829ddcc5782cc81e32557e79383e572811267b2c3f848902b2445cd7b4c`;
dump ID is `ebe52dd3`. The private report and symbol decode are saved in
`.pio/spotify-stop-crash-20261109.json`. Flight-recorder bank requests timed out;
the report records those collection failures. The dump was not cleared.

LovyanGFX 1.2.21's `png_draw_alpha_callback`, in
`src/lgfx/v1/LGFXBase.cpp`, allocates a DMA-capable BGRA scanline, then passes
the result to `readRect` without checking allocation success. The faulting
instruction and null destination match that failure path. The application
decodes alpha PNG transport icons during every Spotify player redraw, including
changes between playing and paused states.

A separate live resource sample, after reboot, reported 1,815 bytes free DMA
heap (largest block 1,344), 9,515 bytes free internal heap, and five failed
allocations. Spotify was playing at 881,664 PCM bytes per five seconds with no
sampled I2S write failures. This supports memory pressure but is not the heap
state at the captured fault. The preserved dump may predate the latest incident;
it cannot establish that every reported Stop crash follows this path.

## Receiver disappearance: still unconfirmed

The adapter's outer exception handler terminates its connection task permanently
but previously left `sessionReady` true. Local discovery is advertised separately
by `startWebMdns`, so a surviving web server or discovery advertisement does not
prove that Spotify's session still processes requests. This is an evidenced
diagnostic/lifecycle defect, not proof of what caused this owner's disappearance.

The pinned cspot code also has unsynchronized Mercury callback registries and
shared playback state across connection, decoder, receiver and output tasks.
For example, `failAllPending` clears registries while `handlePacket` may invoke
and erase a callback. These are follow-up review findings, not the decoded
display crash. A broader concurrency patch is deferred pending targeted evidence.

## Validation boundary

No firmware was flashed, USB serial opened, stored pairing changed, or crash
evidence cleared during this investigation. On-device acceptance must include
repeated remote Stop/resume, continued device-list visibility while stopped,
transfer away/back, and a fresh crash report if a failure recurs. The existing
dump occupies the no-overwrite crash slot; obtaining a new dump requires a
deliberate clear after preserving the existing evidence.

## Candidate changes

GPT-6.1 Sol implemented the display fix. Six transport icons, the back chevron
and four Wi-Fi marks now use generated RGBA runs in flash. Drawing composites
those pixels directly into the existing RGB565 canvas without a temporary heap
or DMA allocation. The same supplied artwork, alpha values, positions and sizes
are retained. Opaque backgrounds retain their existing decoder. No third-party
source or cspot patch was changed.

`/api/spotify/diagnostics` gains:

- `sessionPhase`: `starting`, `pairing`, `authenticating`, `running`, or `failed`.
- `sessionTaskAlive`, `sessionErrors`, and `lastSessionErrorAtMs`. Errors count
  terminal failures, not individual retried authentication attempts.
- `lastEvent`, `lastEventAtMs`, `pauseEvents`, and `disconnectEvents`. Pause
  counts true PLAY_PAUSE events; disconnect counts DISC events. These counters
  identify which upstream event arrives when the owner presses Stop.

A terminal session exception now clears `sessionReady` and marks playback
paused. It still requires restart to recreate the session; no unsafe task
reconstruction or new recovery policy is introduced. `ready` continues to mean
that local pairing/discovery data is available. `running` reports the adapter's
packet-processing phase, not independently verified cloud connectivity.

After installing the candidate, capture a Stop/resume trial with USB serial
closed using the existing read-only sampler:

```sh
python3 tools/spotify_s3_capture.py --base-url http://192.168.11.111 \
  --output .pio/spotify-stop-trial.jsonl --duration 120 --interval 5
```

If the device disappears while HTTP remains accessible, the session fields and
event counters distinguish a terminated adapter from a still-running one. An
HTTP timeout alone does not distinguish a radio failure from a network failure.

The crash collector's pre-existing `main()3` syntax typo was also corrected.

## Verification results

- `pio run -e esp32s3`: PASS. Application image 4,742,912 bytes (11,120 bytes
  larger than the previous export), 1,810,688 bytes free in its partition.
  Static DIRAM 267,351 / 341,760 bytes (78.23%), 74,409 bytes remaining.
  Matching ELF archive:
  `a52e7375e267adc387619ba6be8aa34c9366e618170fae4d7e5c331474df65b6`.
- `python3 tools/render_ui_fonts.py --alpha-icons-only`: PASS. All 11 icons
  match the original PNG renderer pixel-for-pixel on four RGB565 backgrounds.
  Ten repeated playing/paused redraws and a fallback render complete. Native
  320 x 240 paused/playing evidence is in `.pio/ui_native/alpha-icons-*`;
  the paused fixture was visually inspected and retains the existing artwork.
- `python3 tools/check_ui_alpha_icons.py`: PASS under ASan/UBSan. Each icon
  is exercised 100 times with allocation forbidden, for readable and fallback
  drawing and clipping at screen edges.
- `python3 tools/check_crash_tools.py`: PASS against the built artifact,
  including immutable archive and exact-ELF decoding checks.
- `git diff --check`: PASS. No generated binaries, secrets or vendored source
  changes are included.

A subsequent read-only two-minute capture of the **installed, unchanged**
firmware returned all 12 samples from each of resources, Spotify diagnostics
and player endpoints. There was no reset within that window and no active
Spotify output. The initial uptime was 77 seconds with reset reason 1, distinct
from the earlier running sample; the agent did not trigger a restart. This is
idle responsiveness evidence only, not a Stop/resume or discovery test.
Private trace: `.pio/spotify-stop-observation-20261109.jsonl`.

**Hardware acceptance remained NOT VERIFIED at the end of the initial pass.**
The agent had not installed the candidate. The subsequent device investigation
below identifies that build running on the radio and reproduces a separate
failure; it does not close acceptance.

## Subsequent disappearance before playback started

Later on November 9, the owner reported that Radiohead disappeared after a
playback attempt which did not start. All four diagnostic/player HTTP probes
to its last known address timed out. The Mac was then on `192.168.44.136`,
using gateway `192.168.44.1`, while the radio's recorded address was
`192.168.11.111`. The gateway returned Destination Host Unreachable. A bounded
Spotify mDNS browse returned no service, and `radio.local` did not resolve.

This establishes a network reachability obstacle during the attempted capture;
it does not establish a new firmware crash or session failure. The radio's
current address and screen responsiveness were requested, without rebooting,
flashing or opening serial. Evidence is saved privately in
`.pio/spotify-disappearance-20261109-175002.json`.

The owner clarified that the two networks normally route to one another and
confirmed the radio still showed `192.168.11.111`. After moving the Mac to
`192.168.11.195`, HTTP still failed, ping received no replies and ARP resolution
for the radio remained incomplete. The subnet difference therefore does not
explain the full failure. The owner reports that touch still responds, but
selecting a radio station returns to Home showing Spotify rather than starting
the station. This matches an unsuccessful local-output handoff, but its precise
failure branch could not be observed.

With the owner's explicit authorization, the WCH UART bridge was opened at
115200 baud with DTR/RTS deasserted before open, no reset pulse and no transmitted
commands. A 45-second capture returned zero bytes. This build sends Arduino and
IDF diagnostics through native USB, which was not enumerated; silence on the
UART bridge is not evidence that the device stopped executing. The HTTP retry
is saved in `.pio/spotify-stuck-20261109-175412.json`; the private serial capture
is under `.pio/private-spotify-stuck/`. After these attempts, the owner was asked
to perform the offered hard reboot so retained diagnostics could be collected.

## Reproduction and live stack: confirmed decoder stop failure

After the owner's hard reboot, HTTP recovered. Live diagnostics identified
build prefix `a52e7375e267adc3`, matching the newly built image; it had therefore
been installed outside this agent's flashing actions. The retained previous
and crash banks still describe the older `fd440829ddcc5782` image, not the
latest powered-off stuck state. All banks and the preserved dump were saved in
`.pio/spotify-after-reboot-20261109.json` without clearing anything.

A controlled playback attempt was captured at two-second intervals in
`.pio/spotify-failed-start-live-20261109.jsonl`. ACTIVATE arrived at device
uptime 73,330 ms. At 75,723 ms, output was owned, buffering and paused, with
zero PCM; phase was running and no terminal session error was recorded.
Internal free heap was 27,179 bytes and DMA free heap 19,479 (largest block
9,728). The next sample timed out, followed by sustained HTTP loss. These last
heap values do not prove exhaustion at the later failure.

The owner connected native USB. Opening capture produced an explicitly logged
USB_UART_CHIP_RESET, so this is a fresh reproduction, not uninterrupted evidence
of the previous attempt. The owner heard playback briefly, then a song change
stopped it and removed the receiver from Spotify. Selecting a local station
twice produced:

```
producer-stop released=1 stopped=0
generation=2 restore=station spotify-stop-timeout
producer-stop released=1 stopped=0
generation=3 restore=station spotify-stop-timeout
```

The I2S sink released successfully, but the decoder failed to acknowledge
suspension. Local I2S restoration was never attempted. Decoder heartbeats stop
at 45,252 ms, while queue heartbeats continue. Later Mercury receive and
reconnect failures are logged; no panic or watchdog appears in this capture.
Private serial evidence: `.pio/private-spotify-stuck/native-20261109-180003.log`.

Using native USB JTAG with flash support disabled, the agent briefly halted,
read task stacks with arguments suppressed, then detached/resumed. No flash
or reset command was issued. The matching ELF resolves the blocked decoder:

```
TrackPlayer::runTask -> ov_read -> TrackPlayer::_vorbisRead
 -> CDNAudioFile::readBytes -> std::istream::read -> SocketBuffer::xsgetn
 -> TLSSocket::read -> mbedtls_ssl_read -> mbedtls_net_recv
 -> lwIP receive -> queue wait
```

The application loop was running, Spotify output was parked, and the network
and Wi-Fi tasks were waiting for messages. The default netif retained the
expected IP/gateway and up/link flags. This does not establish why packets
stopped arriving. Task evidence is in `.pio/private-spotify-stuck/tasks-deep-20261109.txt`.
The no-reset debug setup follows [Espressif's flash-disable guidance](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/jtag-debugging/tips-and-quirks.html).

The confirmed fix target is cancellation of the unbounded decoder network read,
so a source change can close the stream and acknowledge suspension. Increasing
the handoff timeout or changing local DMA allocation does not address this
observed branch. The initial network-loss trigger and any relationship to the
earlier changes remain unproven; no A/B firmware comparison has been performed.

A later attempt to catch the reconnect error under a hardware breakpoint did
not yield an error value. That debugger session encountered the panic/semihosting
path and a target reset; its state was perturbed and is not evidence for the
original failure. OpenOCD was shut down. The earlier detached stack captures
above identify the blocked read before this later probe.

## Cancellable-stream candidate

GPT-6.1 Sol implemented `stream-cancel-bell.patch` and
`stream-cancel-cspot.patch`, applied reproducibly by normal build preparation.
They keep the existing sink-release plus decoder-acknowledgement gate. No local
audio vendor code, pins, settings, or partition layout changes are included.

- TLS read, write and handshake operations use nonblocking sockets, 100 ms
  select slices, a five-second per-operation deadline and a cancellation
  predicate. Negative TLS results cannot become huge unsigned byte counts.
- Track reset generations keep old streams cancelled even after `pendingReset`
  is cleared. Metadata waits check suspension in 100 ms slices.
- Cancellation bypasses CDN range retries, closes the connection, and unwinds
  Vorbis before suspension acknowledgement. Cancelled tracks do not emit EOF
  or depleted events. No C++ exception escapes the codec read callback.
- Closing a stream discards pending bytes rather than flushing an interrupted
  request. Reopening clears buffered data and stream error flags.

**Limit:** DNS resolution and initial TCP connection establishment remain
blocking. The bounded cancellation result applies to the captured established
stream and metadata-wait paths, not every possible connection phase. The
initiating network failure remains unproven.

`python3 tools/check_spotify_stream_cancel.py` passes under ASan/UBSan. It
reconstructs the pinned dependencies and ordered patches in a fresh checkout,
then runs production TLS policy, socket-stream, HTTP, CDN and decoder lifecycle
code. The TLS record layer is a deterministic shim over real socket pairs;
this is not cryptographic interoperability or physical-radio validation.
Checks cover a five-second silent-peer timeout, read/write/handshake
cancellation, a partial stalled CDN body, unchanged position on cancellation,
fresh-track data, truncated-range retry, stream reopening, metadata suspension,
reset generations and codec cleanup. Cancellation assertions complete in
under 400–500 ms. Patch idempotency and conflict refusal also pass.

The existing token-refresh checks pass, and root review reran the cancellation
suite and `git diff --check`. Firmware artifact and device-test results follow
when available; this candidate has not yet been installed by the agent.

## Owner's cancellation-build reproduction

The owner flashed the final cancellation build and reproduced playback followed
by Wi-Fi loss and refusal to return to radio. Native USB JTAG verified the live
ELF SHA256 as `d7c8c74d83aa347294e936ac87c38a645d7a1b24938983444a316d9a45000ebf`,
matching the validated export (4,747,312-byte application). The previous root
build was stopped when a concurrent owner upload was detected; only the root
process group was terminated, leaving that upload untouched.

The first stopped-state capture shows the decoder in its local-suspension loop,
with `localSuspended=true`, `localSuspendAck=true` and `outputOwned=false`.
The media owner was nevertheless Failed, generation 2; local audio remained
parked with no I2S handle. This does not distinguish a late acknowledgement from
a failed local I2S restoration because the existing logs were unavailable.
Networking and application tasks were not stuck in a busy loop at that snapshot.
The Mercury reconnect task was waiting inside USB CDC error logging.

Retained records show four allocation failures by uptime 54,108 ms, internal
free heap 6,967 bytes, historical minimum 6,456 bytes, largest internal block
6,144 bytes. The latest failed request was 444 bytes with capabilities `0x80c`
(INTERNAL | DMA | 8BIT). DMA-capable allocation failure is established; neither
the caller nor its causal relationship to Wi-Fi loss is yet established. Wi-Fi
continued reporting connected with strong RSSI and zero disconnect events.
Later general internal heap recovery did not establish network recovery.

**Debugger limitation:** GDB detach left both processors halted in this setup.
Subsequent captures therefore describe the same paused state, and the owner's
later unresponsive-touch report occurred while it was paused. Root explicitly
resumed via OpenOCD and verified running targets; a subsequent panic/semihosting
stop was debugger-perturbed evidence, not the original failure. OpenOCD was shut
down. Do not treat detach alone as proof of resumption. Private evidence:
`.pio/private-spotify-stuck/post-cancel-*.txt` and `post-cancel-rtc.bin`.

The next diagnostic candidate removes USB writes from `RedactedLogger`, retains
numeric upstream error sites for HTTP/JTAG, captures the first eight allocation
failures with capability masks and eight caller PCs without printing/allocating,
and records Spotify restore result -4 (release/acknowledgement timeout) versus
-5 (local I2S restoration failure) in existing RTC media breadcrumbs (operation 5).
GPT-6.1 Sol also fixed independently verified AP reconnect defects: failed DNS
used an uninitialized address pointer, failed connects leaked resolver results,
and EINTR/EOF handling could incorrectly advance byte counts. Production fault
fixtures pass under ASan/UBSan; these defects are not proven to initiate Wi-Fi loss.

No Wi-Fi buffer policy was changed. Inspection found Arduino overrides the
compiled IDF buffer settings unless `WiFi.useStaticBuffers(true)` is called.
The current dynamic policy uses static RX 4, dynamic RX 32, dynamic TX 32 and
cache TX 4; claiming savings from compiled RX16/TX16 values would be incorrect.
The next allocation trace is intended to identify the actual failing consumer
before changing this resource policy.

Diagnostic build: `pio run -e esp32s3` passed; application 4,748,608 bytes,
1,804,992 bytes free in its slot, DIRAM 267,767 / 341,760 bytes (78.35%).
ELF SHA256 `11d3f3149e651ea1f0318d0f72c7c3358aef0b40350f0356dd26a4c6762ea5f6`.
Radio diagnostic sampler/collector/network tests and `git diff --check` pass.
The first-eight trace is ordinary RAM and must be collected before reset;
raw Xtensa caller PCs require `esp_cpu_process_stack_pc` normalization before
addr2line (clear return-window bits if bit31 is set, map into 0x40000000,
then subtract 3). The trace captures no credential strings or packet contents.

Root installed this diagnostic image via `flash_built_firmware.py`; esptool
verified written hashes and reset the device. HTTP recovered and Spotify reached
the running session phase with zero allocation failures before playback.
The serial monitor and debugger were closed for this reproduction. Live capture:
`.pio/spotify-dma-trace-live-20261109.jsonl`.

The owner selected Radiohead while Spotify was playing on the Mac; nothing
played on Radiohead and Wi-Fi became unreachable before any song change. HTTP
was last healthy around uptime 46 seconds. The first four allocation traces
occur at 48,079–48,137 ms: each requests 194 bytes with capabilities `0x80c`.
Exact-ELF decoding identifies:

```
ppTask -> ieee80211_output_process -> ROM -> esf_buf_alloc
 -> esf_buf_alloc_dynamic -> esp_coex_common_malloc_internal_wrapper
 -> heap_caps_malloc -> heap_caps_alloc_failed
```

This establishes failure of dynamic Wi-Fi transmit memory allocation at the
network-loss onset. It does not establish which other allocation consumed the
remaining DMA heap. A single bounded JTAG capture saved the trace, explicitly
resumed both CPUs (OpenOCD reported running), and shut down OpenOCD. Nevertheless
the owner then reported a restart; HTTP confirmed reset reason 7 (other watchdog)
and the previous bank ended with reason 7. This reset followed debugger use and
cannot be classified as spontaneous Spotify behavior. The preserved crash ID
remains the earlier `ebe52dd3`; it contains no new reset evidence.

Sol is implementing a targeted fixed Wi-Fi transmit-buffer pool, selected before
Arduino initializes the driver, to avoid the observed dynamic-TX allocation
contention. Subsequent playback acceptance must run with no debugger attached.

The fixed-pool candidate selects `WiFi.useStaticBuffers(true)` before any
Wi-Fi initialization on both station and setup-AP startup paths. Its explicit
budget is four static RX buffers and four static TX buffers, RX/TX block-ack
windows four; dynamic RX remains 32 and cache TX remains four. This reserves
roughly 6.4 KiB for TX at startup instead of allocating packet DMA buffers during
Spotify activity. Management/control allocations can still fail under pressure;
the change does not promise that every Wi-Fi allocation becomes static.
Cached and fresh builds enforce the policy, and export checks reject mismatched
generated configuration. HTTP resources report the selected Arduino policy and
configured counts. Startup order, repeated startup, cached-config enforcement,
and 19 roaming scenarios pass; root reran those tests. Physical playback,
discovery, stop and local-return verification is still required for this image.

## Fixed-pool playback and Home crash

Build `9f052d525af3547564529f2e44b08ad3cd30a6a8e62402f8043503868338bb67`
passed (4,749,120-byte app; DIRAM 267,767 bytes). The owner installed it while
root was building/checking; live HTTP confirmed this exact build, static-buffer
policy true, and RX/TX/BA counts four. The owner reported Spotify playing and
song changes working, then a panic when returning to the main menu. Root's
initial statement that the diagnostic image was still running was immediately
corrected using that device evidence.

The earlier crash report was durably saved and decoded, then the slot explicitly
cleared/re-armed. The owner reproduced the Home transition without a debugger
connected. New crash `19fb8805`, on the exact `9f052...` ELF, is StoreProhibited
at address zero in loopTask: `renderHome -> drawHomeTile -> drawHomeAsset ->
LovyanGFX png_draw_alpha_callback -> readRect`. Thus the original allocation-free
icon fix had not covered Home's translucent tiles.

Root extended the existing compositor to all 18 Home RGBA assets and the 11
list surfaces reached through Home. This is the same small rendering fix, with
unchanged source artwork/layout. Forty assets pass exact PNG comparison on four
RGB565 backgrounds and 100 allocation-forbidden readable/fallback/clipping
repetitions under ASan/UBSan. Full native UI checks pass; root inspected the
320×240 Home and Spotify fixtures. No vendored-library edits were needed.

Final Home/list build `fcc2828a57733e5445c8d6baecf41df6c36f454e0b36adfb4165a7b6d85798c5`
passes `pio run -e esp32s3`: 4,811,936-byte app, 1,741,664-byte slot headroom,
unchanged DIRAM 267,767 / 341,760 bytes. Root installed it; esptool verified
the image. With no debugger/serial monitor attached, the owner reported that
playback, Mac pause/resume, returning to Home and selecting a radio station
all work. Final HTTP independently confirms build `fcc2828a57733e54`, radio
source playing, continuous boot 10, and successful connect/await-playback
breadcrumbs. This is a short functional pass, not a long resource/soak acceptance.

The capture also exposed a separate failure storm: `updateWeatherData` retries
an unavailable 8 KiB internal task-stack allocation every loop. The first eight
caller traces identify `xTaskCreatePinnedToCore -> updateWeatherData`; the count
reached 47,384 while the main requested operations still succeeded. Root added
a 30-second retry delay only after weather-task creation failure, retaining
explicit refresh and normal scheduling behavior. Production-function tests pass
for 10,000 intervening loop calls, repeated failure, eventual success, refresh,
and clock rollover. Build `e61638bab2cb1fa85bb0f03c864381688718a76e8fee72f39165ca1959c0971e`
passes (4,812,000 bytes); **this additional guard is not flashed**. The radio is
left on the owner-verified `fcc2828...` build. Internal memory headroom remains
tight; the exact historical change that exposed these latent failures has not
been isolated with an A/B comparison. Frequent HTTP polling may also affect the
resource profile; the final recording was stopped after 172 seconds.
