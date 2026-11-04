# Radio silence investigation — agent handoff

Prepared 2026-11-04. All incident times below are local Asia/Hebron, UTC+02:00.
Follow-up implementation, reconnect source findings, and comparison procedure:
[`network-followup.md`](network-followup.md). The original evidence below is
preserved; the follow-up distinguishes host/build checks from device acceptance.

This handoff supersedes earlier conversational speculation about buffer overflows
and the earlier claim that the logging switch had not yet reached the device.

## What the user needs

Investigate intermittent pauses/silence while listening to Galatz:
`http://glzwizzlv.bynetcdn.com/glz_mp3` (saved station slot 1 in these captures).
The device is reachable at `http://192.168.11.127` on the local network.
The user initially wanted a crash-surviving logger, then a persistent UI switch
for detailed logging while retaining basic crash/reset diagnostics. Both are
implemented. The user has now requested a handoff, not a speculative fix or
another deployment.

**Strongest finding:** the user-reported silence on November 4 coincided with
loss of Wi-Fi connectivity and repeated authentication/rejoin failures. Input
buffer contents fell to zero and local PCM production stopped. Wi-Fi recovered,
the player reconnected, and the user explicitly confirmed sound returned.
The trigger for the Wi-Fi failure remains unknown. Automatic AP roaming is a
candidate, not an established root cause.

## Repository/device state

- Workspace: `/Users/avner/Documents/Projects/ESP32Projects/radiohead`.
- Current HEAD when preparing this handoff: `30dd690` (`off switch to logging`).
  Previous implementation commit: `126cf8b` (`logging`).
- At handoff start the only modified file was `docs/radio-stall-diagnostics.md`,
  containing the buffer interpretation correction and silence incident notes.
  Preserve those edits. This handoff adds this folder and updates that guide.
- November 3 early captures report ELF hash prefix `aced42f2ce63fec9`.
  November 4 live and previous-session banks report `c78bbe75c4d568ad`, with
  `detailedEnabled:true`. The device has therefore moved beyond the original
  logger-only capture. Do not identify a compiled image solely by release version;
  exact ELF-to-source provenance has not been independently checked here.
- Last observed live boot was boot 3, reset reason 3 (software restart).
  Its previous-session archive is boot 2, also ending with reason 3. No crash
  archive was available. The actual UI switch's off/on behavior has not been
  exercised on the physical radio by this agent.
- No playback, Wi-Fi configuration, restart, flash or OTA actions were performed
  during the live incident investigation. Recovery occurred while using read-only
  HTTP requests. The last confirmation from the user was **“Yes, sound is back.”**

## Preserved original evidence

These are byte-for-byte copies of the original local captures, with hashes in
[`2026-11-04/SHA256SUMS`](2026-11-04/SHA256SUMS). Originals also remain in `.pio/`,
but that directory is ignored/build-cleanable. The copies below are suitable for
handoff with the repository. They contain numeric diagnostics and public station
metadata, not Wi-Fi passwords or API keys.

| File | Evidence and retained time span |
| --- | --- |
| [radio-silence-2026-11-04.json](2026-11-04/radio-silence-2026-11-04.json) | **Primary incident.** Live tail November 4, 08:09:48–08:12:25, including zero input/no PCM progress, reconnect failures and recovery; also includes the prior-session bank. |
| [radio-silence-player.json](2026-11-04/radio-silence-player.json) | `/api/player` captured during investigation: Galatz, radio source, volume 8, not muted, notch disabled, playing by the time of the read. |
| [radio-silence-resources.json](2026-11-04/radio-silence-resources.json) | `/api/spotify/resources` is a general runtime-resource endpoint despite its name; captured at uptime 1,422,451 ms. No failed allocations. |
| [radio-diagnostics-2026-11-04.json](2026-11-04/radio-diagnostics-2026-11-04.json) | Morning capture before silence: live November 4, 07:49:06–07:55:32; previous bank November 3, 14:56:21–15:00:38. Confirms retained log recovery and archive creation across a software restart. |
| [radio-diagnostics-initial.json](2026-11-04/radio-diagnostics-initial.json) | November 3, 11:30:10–11:31:31; startup and initial baseline. |
| [radio-diagnostics-followup.json](2026-11-04/radio-diagnostics-followup.json) | November 3, 11:32:14–11:51:55; recurring display delays, no captured audio fault. |
| [radio-diagnostics-latest.json](2026-11-04/radio-diagnostics-latest.json) | November 3, 11:47:00–12:06:26; repeated minute-boundary delays, stable memory, no captured audio fault. “Latest” is its original filename, not the newest capture. |

Time spans are the oldest/newest retained entries, **not continuous recording**.
Samples and event records have different retention windows. A single condition
can appear in both rings, with distinct sequence numbers but identical time/data.

## Confirmed silence timeline

The November 4 morning capture had zero Wi-Fi disconnects. The later incident
capture already begins partway through the outage:

| Local time, November 4 | Observation |
| --- | --- |
| 08:09:48 | Radio connecting; Wi-Fi not connected; compressed input 0; PCM block count 108,222; cumulative disconnect events 193. |
| 08:09:48–08:12:10 | PCM count remains 108,222 and input remains 0. Disconnect events continue approximately every few seconds. Most recorded reasons are 2, `AUTH_EXPIRE`. |
| 08:12:06 | Counter has reached 235; it subsequently reaches 237. Last recorded reason changes to 36, `STA_LEAVING`. |
| 08:12:13 | Wi-Fi connected at about −56 dBm; audio still connecting, buffer still empty. |
| 08:12:18 | `stream_ready`, recovery/timeout/state signals recorded; playback state Playing; input 24,599; PCM count 108,247 and increasing. |
| 08:12:25 | Input has grown to 381,721; PCM count 109,502. |
| Subsequent read, through about 08:12:51 | Input around 655,350; PCM count 113,837; RSSI about −57 dBm; disconnect count stays 237. This second read was inspected in tool output, not saved as a separate JSON artifact. |
| User reply during investigation | Sound is back. |

Player/resource observations: volume 8, mute false, notch disabled, no allocation
failures. Internal free heap was about 35–38 KB around recovery (lifetime minimum
20,352 bytes); free PSRAM about 6.35 MB. There is no evidence here that exhausted
memory or a broadcaster outage caused this particular silence.

Reason 2 means authentication timed out or an authentication-expiry reason was
received from the AP. Reason 36 means the station is leaving the BSS/resetting.
Neither alone identifies which side caused the failure or proves a wrong
password. The count of 237 includes failed rejoin attempts; it does **not** prove
237 successful connections followed by drops. See the pinned IDF definitions in
`esp_wifi_types_generic.h` and
[Espressif's reason-code reference](https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32s3/api-guides/wifi.html#wi-fi-reason-code).

The beginning of the outage was overwritten, so exact onset and the initiating
AP/roam event cannot be recovered from these logs.

## Other findings and corrected interpretations

**Display delays:** early healthy captures show about 550–635 ms top-level loop
gaps, usually at minute boundaries. In one capture 19 of 24 retained event
records landed exactly on a minute boundary. `serviceDisplayRefresh()` in
`src/display.cpp` calls `renderRadioUi()` for the whole screen when the formatted
clock changes. The later November 3 archive shows additional roughly 550–690 ms
display delays, sometimes every five seconds; do not attribute every redraw to
the minute clock. Wi-Fi indicator changes and other dirty-state triggers also
exist. November 4 startup included larger display delays. No captured PCM stall
or user-confirmed pause was correlated with these healthy-period redraws.
This is a performance concern separate from the confirmed Wi-Fi outage.

**Buffer fill above “capacity” is explained:** the pinned library allocates
655,350 main bytes plus a 65,535-byte reserve = 720,885 physical bytes.
`getInBufferSize()` reports only the main size. `writeSpace()` allows writes into
the reserve and `bufferFilled()` can count those bytes. The first three captures
peaked at 697,170 bytes, below the allocation. Earlier conversational statements
calling these readings invalid or suggesting an overflow were too strong.
Do not clamp away valid reserve usage or patch the audio library on that basis.
The exported `capacity` label is misleading as a physical upper bound.

**Earlier coverage:** November 3 saved captures contain no slow/lost-stream,
PCM-stall or allocation-failure events. The archived 14:56–15:00 tail shows two
cumulative Wi-Fi disconnects from earlier in that session; their event details
were already overwritten. This does not establish an incident-free whole day.

## Diagnostics implementation and interpretation limits

Full contract: [`../radio-stall-diagnostics.md`](../radio-stall-diagnostics.md).

- `src/radio_diagnostics.cpp` / `include/radio_diagnostics*.h`: bounded RTC store,
  sampling, atomic callback signals, per-service breadcrumbs and paged export.
  `src/settings_radio_diagnostics.cpp` owns persistence/archive operations.
- RTC store: 6,320 bytes, 32 sample slots every 5 seconds (~160 seconds), plus 24
  event slots, coalesced to at most one record per second. Measurements poll every
  250 ms. Event storms can overwrite their own onset within a short period.
- Flags cover library slow/lost/ready/EOF signals, recovery, Wi-Fi, allocation
  failures, ≥100 ms loop gaps and ≥1 s lack of PCM progress while state is Playing.
- **Crucial:** `pcmIdleMs` is zero when the player is Connecting, even if silent.
  No `pcm_stall` flag during connection recovery does not mean sound continued.
  Compare PCM block counters, input, Wi-Fi, playback state and flags together.
- PCM counts measure blocks reaching the pre-I2S hook, not DMA completion or
  audible speaker output. Zero encoded audio/mute/output hardware needs separate
  evidence if PCM keeps progressing during a future silence.
- `slowPhase`/`phaseUs` and `loopGapUs` can describe different completed work at
  record-window boundaries. A 600 ms loop gap paired with a 1.7 ms web phase is
  not evidence that the web handler itself consumed 600 ms. Additional internal
  calls to `audio.loop()` are not individually timed.
- Firmware does not log arbitrary callback text, URLs, credentials or SSIDs.
  Event flags are coalesced; repeated signals within a window are not counted.
- RTC intended retention: software and ordinary panic/watchdog resets. Physical
  software-restart recovery is now observed; panic/watchdog retention is still
  untested. Power loss/brownout can erase unarchived evidence. No backtrace/core
  dump is captured by this recorder.
- Startup archives recovered RTC data to `/radio-previous.bin`; panic/watchdog
  boots also update `/radio-crash.bin`. Normal later boots preserve the last crash
  file. Each file is 6,324 bytes; replacement uses a temporary verified file.
  Peak file payload is about 19 KB, with no unbounded growth.
- No continuous log writes to flash during playback. Archive writes happen at
  startup. Changing the UI mode performs a Preferences write.
- UI: Device & maintenance → Open diagnostics → Playback logging; on by default,
  immediate and persisted as `radio/radioLog`. Off stops detailed recording but
  keeps basic reset/last-service information and old evidence. It does not free
  the RTC allocation or remove boot-time archival. Normal radio-settings reset
  clears the preference to its default.
- GET `/api/radio/diagnostics?bank=live|previous|crash&offset=0` returns eight
  physical slots/page; follow `nextOffset` and sort by `seq`. Live pagination is
  not an atomic snapshot. GET/POST `/api/radio/diagnostics/config` manages mode;
  POST requires `enabled=0|1` and `expected=0|1` with maintenance/stale-state checks.

Read-only collection (do this promptly when symptoms occur):

```sh
python3 tools/collect_radio_diagnostics.py http://192.168.11.127 --output .pio/radio-next-incident.json
```

Also capture `/api/player` and `/api/spotify/resources`. Do not open USB Serial:
opening the port resets this radio, and USB Serial writes without a reader have
previously caused playback delays. Do not accidentally overwrite the preserved
incident with a healthy later collection.

## Recommended next work, in order

1. **Preserve the beginning of network failures.** Before a long new reproduction,
   add a bounded first-disconnect/first-empty-buffer incident record independent
   of repetitive display/connection events, plus first/last timestamps, counts
   by reason, recovery time and outage duration. Consider retaining a small
   pre-incident snapshot and freezing the first failure until retrieved. Use RAM/
   RTC, not periodic flash writes. Only 1,840 RTC SLOW bytes remain; budget the
   layout and version/migration behavior explicitly rather than enlarging rings
   without checking. Prefer distinguishing pending events from the previous
   completed service window when refining timing attribution.
2. **Instrument roaming and retry decisions.** Read `src/wifi_network.cpp` and
   the pinned Arduino STA reconnect implementation. `serviceWifiRoaming()` scans
   every ten minutes and can call `WiFi.begin(ssid, password, channel, bssid)` when
   another same-SSID AP is at least 10 dB stronger. Record a numeric roam-start,
   target/channel/signal comparison, connect result and fallback outcome; keep
   credentials out. If AP identity is necessary, prefer a local opaque identifier
   or restrict its capture rather than logging network names indiscriminately.
   Check whether a failed BSSID-pinned roam keeps retries stuck on that target.
   That behavior is a hypothesis to verify, not a finding established here.
3. **Controlled roaming comparison and AP evidence.** Compare normal roaming with
   a temporary known-AP/no-background-roaming test over multiple ten-minute
   intervals. Coordinate router/AP logs around the November 4 failure, especially
   authentication responses, deauthentication and any mesh/AP handoff. Do not
   conclude that weak signal alone was responsible: RSSI before the incident was
   around −68/−69 dBm, versus roughly −56/−57 dBm after recovery; the original
   failure transition is missing. Apply a bounded fallback/retry fix only once
   the failing path is demonstrated, preserving startup's strongest-AP selection
   and existing saved credentials.
4. **Treat display optimization as a separate change.** Replace clock-only full
   redraws with bounded partial updates while respecting the existing visual
   contract. Measure actual audio service gaps and PCM/output progress before
   and after. Do not claim this fixes the captured Wi-Fi outage.
5. **Improve misleading buffer labels.** Preserve schema compatibility for
   `capacity` or explicitly version it; label it main capacity and document the
   reserve. Any physical-allocation field must match the pinned library, not an
   unexplained hard-coded assumption. No vendor patch is authorized by this
   handoff.

Media reconnect behavior to account for: `src/media.cpp` uses a 12-second
connection timeout, retries at 2/5/15/30 seconds, and polls every 5 seconds when
Wi-Fi is absent. These are separate from Wi-Fi authentication retries. Do not
create a second competing stream-reconnect loop or change them as an unexplained
side effect of instrumentation.

## Verification, constraints and remaining acceptance

- Follow root `AGENTS.md` and the `radiohead-firmware` skill. Do not spawn agents
  unless the current user/instructions explicitly authorize delegation.
- Current hardware is PCM5100 → HX-A244 → speakers, with installed two-transistor
  mute circuit. GPIO2 HIGH = silent, LOW = sound. MAX98357A boards are retired;
  their legacy filenames in software do not make them current hardware.
- No vendor edits: `lib/ESP32-audioI2S-master/` requires an explicit user-requested
  vendor patch. Keep network/storage work out of hot audio callbacks and retain
  task-safe numeric publication. No Serial output in the Spotify PCM task.
- Read configuration/web handoff docs for related changes and update their
  ledger. For TFT changes, inspect `docs/ui/uiconcept.png`, visual contract and
  implementation guide; preserve sunset/blue/artwork/Hebrew presentation.
- Previous logging-switch build passed `pio run -e esp32s3`: image 4,700,480 bytes,
  slot headroom 1,853,120 bytes; DIRAM 261,947/341,760 bytes (76.65%); RTC SLOW
  6,352/8,192 bytes. Output: `.pio/build/esp32s3/firmware.bin` for web OTA.
- Passed focused checks: `python3 tools/check_radio_diagnostics.py` (production
  sampler/archive/settings/HTTP seams and collector),
  `node tools/check_touch_diagnostics_web.cjs` (Device scripts including logging
  mode), `git diff --check`. Earlier logger work also passed audio filter/tone,
  link-map and startup checks; rerun the checks relevant to the next change.
- Existing additional check `tools/check_touch_diagnostics.py` is blocked by a
  stale fixture missing `UiRenderState.volumeOverlay`; no production display fix
  was made for that test failure. Do not misreport it as passing.
- Isolated logging-control markup/CSS was visually inspected at desktop and 320
  px; full-page interactive visual validation was blocked by browser-host access/
  headless fixture issues. See `docs/ui/web-configuration/progress.md` for exact
  evidence. Physical mode-toggle behavior and panic/watchdog retention remain
  open. Normal restart archival and the observed interruption/recovery are real
  device evidence, not host-test claims.

No firmware fixes or new deployment are part of this handoff. The next actionable
priority is preserving/instrumenting the first Wi-Fi failure and testing the
roaming/reconnect path, with the captured incident as the baseline.
