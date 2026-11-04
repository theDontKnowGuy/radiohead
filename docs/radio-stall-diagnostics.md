# Radio playback flight recorder

Implemented 2026-11-03 for intermittent live-radio pauses, including the supplied
Galatz stream. Enabled in the normal firmware; no serial monitor or opt-in image
is required. This records evidence; it does not change reconnect policy or prove
that a particular pause came from the broadcaster.

Current investigation handoff and preserved raw captures:
[Radio silence investigation — 2026-11-04](radio-stall-investigation/README.md).
It includes the confirmed Wi-Fi interruption and recommended next work.
The [network follow-up](radio-stall-investigation/network-followup.md) documents
the added first-incident extension, temporary roaming comparison API, legacy
archive compatibility, updated RTC/flash budget and bounded HTTP watcher.
Its current storage figures supersede the original logger-only figures below.

## Detailed logging switch

Under **Device & maintenance → Open diagnostics → Playback logging**, click
**Detailed playback logging: On/Off** to change the mode. It is on by default
for the current investigation. The change applies immediately, without restarting
playback, and is saved across restarts. A failed save leaves the old mode active.

Off stops periodic snapshots, playback events, PCM progress counters and detailed
service timing. Boot/reset metadata, the last entered service and crash archiving
remain active. Existing records and the last crash archive are kept; they can
still be downloaded. Off does not release the fixed RTC allocation or eliminate
boot-time archive writes. Changing the switch writes one Preferences value;
there are still no continuous log writes to flash during playback.

The additive `radio/radioLog` Preferences key defaults to true when absent and
is cleared with the existing radio settings during factory reset. Ordinary
settings saves preserve it. `GET /api/device` includes `radioDetailedLogging`.
`GET /api/radio/diagnostics/config` returns `enabled`; POST to the same endpoint
takes `enabled=0|1` and `expected=0|1` (the last observed value). Invalid values
return 400, stale state/maintenance conflicts return 409, and persistence failure
returns 500. The browser refreshes state after each attempt. Log export adds
`detailedEnabled`, the mode at the end of that bank's session; legacy archives
without this metadata are interpreted as detailed mode.

Switch validation: production recorder/settings/HTTP-handler tests verify default
on, immediate off, no sampling/counter progress while off, preserved basic service
location, reboot persistence, resumed sampling without false stalls, failed saves,
invalid arguments, stale clients and maintenance exclusions. Device JavaScript
tests cover state rendering, duplicate clicks, save failure and stale-state refresh.
The firmware build passes: 4,700,480 bytes, 1,853,120 bytes free in the app slot;
DIRAM remains 261,947 bytes, RTC SLOW remains 6,352 bytes. November 4 device
captures now report build `c78bbe75c4d568ad` with `detailedEnabled:true`, so the
earlier statement that the device still had the initial logger-only image is
outdated. Physical off/on interaction with the switch remains unverified.

## Collect evidence

1. Ensure a firmware with the recorder is installed using the normal
   settings-preserving upload flow. Read-only captures on November 3–4 verified
   recording on this radio; the November 4 capture also verified recovery of a
   previous-session archive after a software restart.
2. Play the affected saved station normally with the USB serial monitor closed.
3. As soon as a pause happens, note the approximate time and collect from the same
   network (replace the address if mDNS is unavailable):

   ```sh
   python3 tools/collect_radio_diagnostics.py http://radio.local --output radio-diagnostics.json
   ```

4. If the radio crashed and rebooted, collect after it becomes reachable again.
   Avoid removing power before it has restarted and archived its retained log.
   The collector downloads live, previous-session and latest-crash records and
   adds readable event/phase names. It does not change settings or start playback.
5. Keep the JSON with the approximate audible-pause time, selected station and
   matching firmware ELF/hash manifest if deeper debugging becomes necessary.

Direct browser/API access: `GET /api/radio/diagnostics?bank=live&offset=0`.
Banks are `live`, `previous`, and `crash`. `nextOffset` supplies the next page;
`-1` ends the export. Each response has at most eight records. Slots are physical
ring positions, so sort by `seq` (the collector does this). Missing/corrupt slots
are omitted. Invalid banks/offsets return 400; allocation/uninitialized failures
return 503. `available:false` means that bank could not be read/validated, not
that playback was healthy. The separate UI control is described above.

An export is a short, bounded sequence of reads, not a frozen live snapshot;
new live records can arrive while collecting. The collector detects a changed
boot/build between pages. Web export can itself appear in service timing, so
collect after the observed pause instead of polling continuously. The existing
synchronous web server's client timeout also still applies.

## Retention and durability

- The 6,320-byte RTC store contains 32 periodic snapshots (every five seconds,
  about 160 seconds) and a separate 24-record event ring. Events are coalesced
  into bit flags, at most one event record per second. Repeated events of the
  same kind inside a second are not counted individually. Both rings overwrite
  their oldest entries, so download intermittent incidents promptly.
- Every header/record has a version/checksum. The checksum is invalidated before
  replacing a slot and committed last. An interrupted write loses that slot,
  leaving independently valid records readable.
- RTC `NOINIT` storage is intended to survive software restart and the normal
  panic/watchdog reboot path. Actual retention depends on the reset mechanism;
  external reset, severe watchdog recovery and brownout are not guaranteed.
  Power-on/brownout boots explicitly reject RTC contents. This is **not** a
  power-loss journal and does not contain a CPU backtrace/core dump.
- Before boot audio starts, recovered contents are archived as
  `/radio-previous.bin` in the already-mounted LittleFS partition. Panic/watchdog
  resets also replace `/radio-crash.bin`; later ordinary restarts leave that
  crash archive intact. The archive includes the reset reason that ended the
  recorded session. Each archive is 6,324 bytes.
- Archive replacement writes and verifies a temporary file, then renames it over
  the destination. Failed writes/replacements preserve the old destination.
  `rtcRecovered`, `filesystemReady` and `archiveOk` report current-boot recovery
  status; an unsuccessful archive can leave an older bank available. The new
  boot starts a fresh RTC recorder even if archiving failed. Flash exhaustion
  therefore prevents preserving the just-ended session beyond that boot.
- No recorder flash writes occur during playback. Archived banks survive later
  power loss, but erasing/formatting the filesystem removes them. Existing artwork
  storage recovery may format an unmountable filesystem; this logger does not
  add any new formatting or alter settings/partition layout.
- Record contents are numeric plus a 16-hex-character firmware ELF hash prefix.
  They exclude URLs, stream metadata, network names, passwords and API keys.
  Saved station identity is its zero-based slot (`-1` for test/no saved station).

Espressif documents RTC `NOINIT` retention after software restart in
[Memory Types](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-guides/memory-types.html).
The pinned IDF linker places `.rtc_noinit` outside startup initialization;
LittleFS's pinned `lfs_rename` contract permits replacement of an existing file.
Device reset tests remain necessary to establish this board's actual retention.

## Reading the evidence

`ms` is uptime at capture; `utc` is epoch seconds, or zero before clock sync.
`bootResetReason` describes the start of the recorded boot; `endingResetReason`
is meaningful for archived banks. Values follow `esp_reset_reason_t` (software
3, panic 4, interrupt watchdog 5, task watchdog 6, other watchdog 7, brownout 9).
`lastPhase`/`lastPhaseMs` describe the last entered loop service, including a
service that never returned. `UINT32_MAX` indicates an invalid phase breadcrumb.
A stage identifies where progress stopped, not necessarily the cause.

| Evidence | Meaning / interpretation |
| --- | --- |
| `input`, `inputMin`, `capacity` | Compressed input buffer bytes; minima observed at 250 ms polling intervals since the last saved record. `capacity` is the library's nominal main-buffer size, not its full physical allocation; see the reserve-area explanation below. Very short dips can be missed. |
| `pcmBlocks`, `pcmIdleMs` | Cumulative blocks reaching the local pre-I2S hook and time since observed progress. This proves decoder/output preparation progress, not successful DMA delivery or audible sound. |
| `wifi`, `rssi`, `disconnects`, `disconnectReason` | Connection status, connected RSSI in dBm (zero when disconnected), cumulative disconnect count and last ESP Wi-Fi reason. Captures disconnect/reconnect between polls. |
| `heapFree`, `heapMin`, `heapLargest`, `psramFree` | Internal 8-bit heap free/lifetime minimum/largest block and current free PSRAM bytes. |
| `allocationFailures`, `failedBytes` | Allocation-failure count and last requested size. A fatal failure before the next saved record can be absent. |
| `loopGapUs`, `audioUs`, `webUs`, `slowPhase`, `phaseUs` | Maximum top-level audio-loop entry gap, audio/web phase time, and slowest phase since the last saved record. Other existing calls to `audio.loop()` are not measured separately. |
| `state` | Radio playback state: stopped 0, connecting 1, playing 2, failed 3; `-1` when no radio source is selected. |
| `flags` | Bitmask: local audio running 1, user mute 2, volume zero 4, radio selected 8. Silence while muted is not evidence of starvation. |

`events` is a bitmask:

| Bit | Event |
| --- | --- |
| 1 / 2 | Library `slow stream` / `Stream lost` |
| 4 / 8 | EOF / decoder stream ready |
| 16 / 32 / 64 | Recovery scheduled / connection timeout / unexpected player stop |
| 128 / 256 | Playback/station change / Wi-Fi state or disconnect-count change |
| 512 / 1024 | No local PCM progress for at least one second while radio is playing / progress resumed |
| 2048 / 4096 / 8192 | Top-level loop gap at least 100 ms / allocation failed / library I2S write timeout |

Phases: boot 0, audio 1, web 2, network 3, weather 4, controls/settings 5,
media/power 6, display 7, idle 8, diagnostics 9. Phase/timing measurements run in
normal operation; callbacks only publish atomic numeric signals. The PCM hook
publishes a single 32-bit counter, without allocation, formatting, flash or Serial.
The old stream diagnostic Serial print was replaced by these signals.

Buffer interpretation correction (2026-11-03): the pinned `AudioBuffer` allocates
655,350 main bytes plus a 65,535-byte reserve, totaling 720,885 bytes.
`getInBufferSize()` returns only `m_mainSize`, while `writeSpace()` permits the
writer into the reserve and `bufferFilled()` can include those bytes in its
pointer-distance calculation. A fill above the exported `capacity` therefore
does not by itself indicate memory corruption or an overrun. The three initial
device captures peaked at 697,170 bytes, within that allocation. The earlier
description of these values as inconsistent/questionable was too strong; the
misleading part is treating nominal main capacity as a physical upper bound.
This explains those readings, without establishing that all library buffer
operations are correct or identifying the cause of the reported audible pauses.

A low input buffer plus slow/lost-stream events with healthy service timing
suggests incoming-data starvation. Wi-Fi disconnects/weak RSSI strengthen a local
network hypothesis. Normal Wi-Fi does **not** distinguish an upstream broadcaster
stall from ISP/router/TCP delays. Large service gaps or heap failures provide
separate evidence of device pressure. Healthy PCM progress during audible silence
can also mean silence encoded by the station, mute/filter settings, or an output
problem; the logger alone cannot separate those.

## Verification

- `pio run -e esp32s3`: passes. Firmware 4,695,232 bytes; app-slot headroom
  1,858,368 bytes. DIRAM 261,947 / 341,760 bytes (76.65%). RTC SLOW 6,352 /
  8,192 bytes (including 6,320 bytes of retained log); 1,840 bytes remain.
- `python3 tools/check_radio_diagnostics.py`: production sampler/archive code
  under ASan/UBSan, including PCM stall/recovery, transient disconnects,
  allocation failures, loop timing, ring overwrite isolation, every-byte record
  corruption, paging bounds, reset archive separation and failed replacement.
- `python3 tools/check_audio_filter.py`, `python3 tools/check_audio_tone.py --firmware`
  and `python3 tools/check_startup_sequence.py`: pass. Startup test expectations
  were corrected from an obsolete ten-second QR hold to the existing seven-second
  firmware behavior; the transient reconnect fixture now finishes within that hold.
- `git diff --check`: passes. No vendored sources or generated binaries added.

Hardware acceptance is open: listen to the affected stream with serial closed;
compare buffer/PCM timing during an observed pause, a temporary AP outage and a
normal reconnect. Software-restart archival is now observed in the November 4
capture; panic/watchdog retention still needs a dedicated bench test image.
Verify crash-archive preservation
after subsequent normal boots and a power cycle **after** archive completion.
No intentional-crash endpoint was added to production firmware.

## Captured audible interruption — 2026-11-04

The user reported silence and then confirmed sound returned during investigation.
Saved capture: `.pio/radio-silence-2026-11-04.json`; separate player/resource
responses are `.pio/radio-silence-player.json` and `.pio/radio-silence-resources.json`.
All times below are Asia/Hebron (UTC+02:00).

- The retained tail already shows disconnected/connecting playback at 08:09:48:
  compressed input zero, local PCM block counter fixed at 108,222, and 193
  cumulative Wi-Fi disconnect events. The beginning of the outage was overwritten.
- Through 08:12:06, repeated Wi-Fi disconnection events increased the counter to
  235, eventually reaching 237. Most retained reasons are 2 (`AUTH_EXPIRE`);
  the last is 36 (`STA_LEAVING`). Espressif documents reason 2 as an authentication
  timeout or an authentication-expiry reason received from the AP. It does not
  by itself establish a wrong password, defective router or firmware fault.
- Wi-Fi was connected again by 08:12:13 at approximately -56 dBm. The decoder
  reported stream ready at 08:12:18 and PCM blocks began increasing; the input
  buffer subsequently refilled. A second read still showed 237 disconnects and
  continued PCM progress, and the user confirmed audible recovery.
- Player state: saved station 1 (Galatz), volume 8, mute false, notch disabled.
  No allocation failures were recorded. The immediate cause of this silence was
  Wi-Fi loss/rejoin failures starving playback; this capture does not support a
  broadcaster outage or memory exhaustion as the immediate cause.
- Source inspection shows automatic stronger-AP roaming every ten minutes in
  `serviceWifiRoaming()`, using `WiFi.begin(..., channel, bssid)`. This is an
  investigation candidate, not a proven trigger: the retained event tail lacks
  the original disconnect, AP identity and roaming-attempt evidence. The 237
  events include repeated reconnect failures, not necessarily 237 successful
  connections followed by drops. No firmware/network settings or playback actions
  were changed during capture.

Reference: [Espressif Wi-Fi reason codes](https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32s3/api-guides/wifi.html#wi-fi-reason-code).
