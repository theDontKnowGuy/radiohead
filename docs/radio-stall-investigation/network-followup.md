# Network investigation follow-up — 2026-11-04

2026-11-07: [Playback-aware roaming](../wifi-roaming.md) supersedes this
snapshot's unconditional roaming and missing-fallback behavior. The historical
incident findings below remain unchanged; the new firmware is not device-verified.

The user requested the handoff recommendations be carried out. This package
implements priorities 1–2 and prepares the controlled comparison in priority 3.
The user has no router/AP logs. Display optimization is a separate task; neither
display rendering nor stream retry policy nor the vendored audio library changed.

## What the reconnect source establishes

The build resolves Arduino 3.3.11 through `.pio/spotify-components/arduino`.
Do not inspect the unsuffixed global PlatformIO Arduino directory: it is a
different installed version. The authoritative files inspected here are:

| File | SHA-256 |
| --- | --- |
| `libraries/WiFi/src/STA.cpp` | `4206afa605763873afc094fcb7a8d700207a82efcb81712ccb2b88475ee9a2a0` |
| `libraries/WiFi/src/WiFiSTA.h` | `7abc2d5af5ed26226d345460bbec8c130b2bd08c444ffeb78456c941dbdae25d` |
| `libraries/WiFi/src/WiFiSTA.cpp` | `ae665d9b0469c58017ad193a47bf2e2409acc69e955f409e156ae5896201497f` |

`STA.cpp:373` accepts a BSSID and sets `conf.sta.bssid_set=1` at line 410.
The event handler at lines 140–175 retries `AUTH_EXPIRE` by calling the default
`disconnect()` and parameterless `connect()`. `WiFiSTA.h:78` defaults `eraseap`
to false. `STA.cpp:333` reads the existing configuration, writes it back, and
calls `esp_wifi_connect()`. This path does **not** clear the BSSID pin.
The application has no bounded unpin/fallback after its stronger-AP roam.
The matching [upstream Arduino source](https://github.com/espressif/arduino-esp32/blob/3.3.11/libraries/WiFi/src/STA.cpp)
also documents this path; the local pinned package and hashes above determine
the build's behavior.

Therefore a failed roam can keep automatic retries aimed at its target. This
is an established code path, **not proof that roaming initiated the November 4
outage**. Its first transition was lost. A successful roam also leaves the pin
configured for later reconnects. Startup still selects the strongest matching
AP across channels. No speculative unpin/retry fix was added.

The same implementation may wait up to one second for disconnection when
calling `connect(ssid, ..., bssid)` while connected. This existing blocking
behavior was not changed. The diagnostic `beginStatus` is the immediate
Arduino return value, not confirmation of completed association/DHCP.

## First-incident evidence

`GET /api/radio/diagnostics?bank=live&offset=0` adds `network` (version 1).
Other page offsets omit it. The outer schema remains 1 and existing fields,
including nominal main-buffer `capacity`, retain their meanings. The collector
keeps page-zero metadata and now refuses to overwrite an existing output file.

- The first disconnect or first empty compressed buffer after established radio
  playback opens the incident. Startup empty buffers, stopped playback and
  station changes do not arm an empty-buffer incident.
- The previous 250-ms poll and the first poll consuming the event are saved in
  `before` and `first`. These are numeric snapshots, not audio or packet captures.
- Disconnect timestamps come from the Arduino callback. Empty-buffer and PCM
  recovery timestamps are sampled at 250 ms plus any loop delay. UTC is derived
  from the poll's valid wall clock; zero means unavailable. Use elapsed `ms`
  across a clock correction. All duration subtraction handles 32-bit rollover.
- The first/last disconnect reason and times, up to 16 distinct reason counters,
  and an overflow count are retained independently of the rolling rings.
  Counts include failed rejoins, not just previously connected sessions.
- `configuredPinned` is 0/1, or 4294967295 when reading driver configuration
  failed. `configuredAp`, `configuredChannel`, and `autoReconnect` are the most
  recent values observed while processing a disconnect, not callback-time
  driver snapshots. Only numeric fields leave the local driver-config object.
- Wi-Fi recovery requires a delivered `GOT_IP` event. This avoids mistaking
  Arduino's potentially stale connected status during `AUTH_EXPIRE` for
  recovery. Audio recovery separately requires Playing, nonempty input and
  PCM progress since the preceding poll. PCM still does not prove speaker output.
- `outageMs` runs from the first disconnect to the most recent recovery within
  this episode; another disconnect before audio returns clears recovery and
  continues the same episode. For a network failure outside radio playback,
  GOT_IP completes the episode without requiring radio PCM.
- The completed first episode freezes until reboot; GET does not acknowledge or
  erase it. This intentionally retains the first episode, not every future
  failure. A completed empty-only incident also occupies this slot. Collect it
  and start a fresh boot before a new experiment. Normal rings continue.
- Flags: Incident=1, Disconnected=2, Empty=4, WifiRecovered=8,
  PcmRecovered=16, Complete=32. Test presence bits rather than treating a zero
  millisecond timestamp as absence.

The callback publishes only numeric event/time data into a 32-entry SPSC queue.
The loop writes/checksums RTC storage. Queue pressure retains the earliest queued
events; `queueDropped` reports lost events and makes the episode incomplete as
evidence. Reason-table overflow is separate. A checksum interrupted by reset
can lose this extension; surviving old ring records remain independently valid.

`incidentRoam` freezes the most recent roam decision at incident onset, and
`latestRoam` continues tracking the latest attempt. Compare timestamps before
inferring a causal connection. Each includes source/target AP IDs, target
channel, both RSSIs, immediate begin status and eventual observed AP/outcome.
Outcomes: 0=no attempt; 1=pending; 2=GOT_IP on target; 3=GOT_IP elsewhere or
identity unavailable; 4=no observed GOT_IP after 30 seconds. A later GOT_IP can
update 4 to 2/3. This timeout does not trigger a fallback. AP identities are
boot-local numbers backed by eight RAM-only BSSID slots; zero means unavailable
or table full. Never correlate IDs across boots. BSSIDs, SSIDs and passwords are
not exported. Recovered identity is read when the loop consumes GOT_IP and can
be unavailable if another disconnect has already happened.

Detailed logging off suppresses callbacks and recording, retains evidence, and
marks an existing episode `loggingInterrupted=1` when the mode changes. Treat
that episode's duration/counts as incomplete; toggling is not a recorder reset.

## Storage and compatibility

The legacy Store prefix remains exactly 6,320 bytes. A 528-byte checksummed
network extension brings it to 6,848 bytes. RTC SLOW uses 6,880/8,192 bytes,
leaving 1,312 bytes. Static layout assertions guard the budget and old offsets.
Archive files are now 6,852 bytes; the reader accepts both those and the old
6,324-byte archives. Old archives export `network:null`. Corrupt/unknown
extensions are unavailable independently of valid ring records. Startup writes
only; no periodic flash writes were introduced. Peak three-file payload is
20,556 bytes. Old firmware cannot read the larger new archives on downgrade.

Export before installing: physical cross-version RTC retention is not verified.
Power loss/brownout still discards unarchived RTC evidence. A reset/torn commit
can discard the network extension. Panic/watchdog retention remains untested on
hardware. The first-page JSON reservation increases from 8 KB to 12 KB; other
pages retain 8 KB, and the collector caps responses at 16 KB.

## Controlled comparison

After installing this image, keep USB serial closed and play Galatz normally.
The **Network → Background Wi-Fi roaming** switch applies immediately and is
saved across restarts. The same control is available through the API:

```sh
curl --max-time 10 http://192.168.11.127/api/radio/diagnostics/roaming
```

GET reports `enabled`, connected status, current boot-local `apId`, channel and
RSSI. No raw AP identifier is exported. POST uses exactly `enabled=0|1` and
`expected=0|1`, with stale-state and maintenance checks like the logging switch.
It saves `radio/wifiRoam` before changing the active state. Missing settings
default to enabled; both normal startup and setup mode reload the saved value.
A failed save returns HTTP 500 and keeps the previous state. Repeating the
already-active value does not write flash. Changing the setting does not
disconnect the radio. Factory reset restores the enabled default. The generic
settings backup includes this key with the rest of the `radio` namespace.
A running roaming scan may finish after disabling, but cannot initiate a join.
Browser scans and startup's strongest-AP selection remain available.

1. Start from a fresh boot with detailed logging enabled. Check the saved
   roaming setting and enable it in Network if necessary.
   Record current AP/channel and listen through at least four ten-minute periods:

   ```sh
   python3 tools/watch_radio_network.py http://192.168.11.127 \
     --duration 2400 --output .pio/network-roaming-on.jsonl
   python3 tools/collect_radio_diagnostics.py http://192.168.11.127 \
     --output .pio/network-roaming-on-final.json
   ```

2. Preserve those files. Start a fresh boot, note the connected AP, then disable
   background roaming before ten minutes of uptime:

   ```sh
   curl --max-time 10 -X POST -d 'enabled=0&expected=1' \
     http://192.168.11.127/api/radio/diagnostics/roaming
   python3 tools/watch_radio_network.py http://192.168.11.127 \
     --duration 2400 --output .pio/network-roaming-off.jsonl
   python3 tools/collect_radio_diagnostics.py http://192.168.11.127 \
     --output .pio/network-roaming-off-final.json
   ```

   This suppresses application background roaming; it does not pin an AP or
   disable driver recovery. Verify AP/channel stability within each run. Across
   boots the numeric AP IDs cannot prove use of the same physical AP; if that
   control is necessary, isolate a known AP externally. Without router access,
   this comparison has that limitation.

3. Restore with `enabled=1&expected=0` or the Network switch. Reboot preserves
   the selected value. Repeat comparable runs if
   needed. Keep station, radio position and listening conditions consistent.
   Note local times of actual silence and return of sound.

The watcher makes bounded read-only HTTP requests every minute, includes player,
runtime resources and roaming state, and flushes each JSONL capture immediately.
HTTP timeouts are saved as errors and do not terminate the experiment. Its
duration limit may be exceeded by one in-flight capture. Ctrl-C retains prior
complete captures. All output paths must be new; nothing is overwritten.
Live pagination and supplementary reads are not atomic snapshots.

A failure beginning immediately after `incidentRoam.startedMs`, followed by
repeated reason 2 and a matching `configuredAp`/target with the pin still set,
would support the pinned-target hypothesis. A failure without a recent roam,
or a similar failure in the no-roaming condition, requires a broader network
investigation. A healthy 40-minute run alone does not establish a fix.

## Verification and live observations

- The initial read-only baseline reports boot 3/build `c78bbe75c4d568ad`,
  237 disconnects, input 655,350 and PCM 242,999 advancing, RSSI −57 dBm.
  Saved as `2026-11-04/radio-network-baseline-2026-11-04.json`.
- A later watcher smoke capture at 09:05 UTC (11:05 Asia/Hebron) timed out.
  Saved as `2026-11-04/radio-network-watch-baseline-2026-11-04.jsonl`.
  This is HTTP unreachability, not proof of another Wi-Fi failure.
- Host checks cover storm retention, reason and queue overflow, frozen onset,
  GOT_IP versus stale status, separate PCM recovery, logging interruptions,
  rollover, legacy archives, page serialization, roaming comparison controls
  and maintenance/stale-state rejection. They are not AP or hardware tests.
- The user's first build caught an ambiguous `Network` reference colliding with
  Arduino's global instance; it was fixed by qualifying `RadioLog::Network`.
- Later capture at 09:09 UTC (11:09 local) found boot 4 running intermediate
  image `764f1f7df1d30ba0`, with the extension enabled, zero incident flags,
  progressing PCM, input 655,350 and RSSI −58 dBm. The previous boot-3 archive
  remains readable and exports `network:null`. Preserved in the boot4 JSON and
  watch-followup JSONL in the evidence folder.
- A brief physical API test changed enabled → disabled → enabled and verified
  each response, on boot 4. Roaming was restored. This verifies the control
  endpoint, not a ten-minute disabled-scan interval or audible continuity.
  Preserved as `radio-network-mode-check-2026-11-04.json`.
- Final firmware build passes: 4,708,368 bytes, slot free 1,845,232 bytes, DIRAM
  262,539 bytes, RTC SLOW 6,880 bytes. Final ELF hash prefix `cad09a2851d9f501`.
  The final image adds current AP fields on the mode endpoint, sanitizes invalid
  archived extensions, and requires recent PCM progress for recovery versus
  the intermediate image currently observed on the device.
- Final checks and image details are also recorded in the web progress ledger.
  The agent has not installed the final image. Listening, multi-interval
  comparison and physical first-incident retention remain unverified.
  No root-cause fix is claimed.
