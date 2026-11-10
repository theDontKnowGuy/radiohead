# Network recorder and roaming comparison

2026-11-04 work added onset retention and controls; [November 7 playback-aware
roaming](../wifi-roaming.md) supersedes its unconditional scan/missing fallback.
Neither establishes the trigger of the [captured outage](README.md).

## Source finding

Arduino **3.3.11**, resolved via `.pio/spotify-components/arduino`, preserves a
configured BSSID through AUTH_EXPIRE retry: `disconnect()` defaults `eraseap=false`,
parameterless connect reuses config. A failed or later-lost pinned AP could trap
retries; the application then had no bounded unpin. Starting a join while connected
could wait up to 1 s. This was a real code path, not evidence it caused November 4.
Do not inspect the unrelated unsuffixed Arduino installation. The later roaming
change adds asynchronous disconnect and an 8 s unpinned fallback.

## First-incident record

Page zero of `/api/radio/diagnostics` adds `network` v1; outer schema stays 1.
First disconnect or first empty buffer after established radio opens an episode,
with previous/current 250 ms polls in `before/first`. Startup, stop and station
changes do not arm empty-buffer incidents. First/last times/reasons and up to
16 reason counters survive rolling-ring overwrite; excess reasons count separately.

- Callback timestamps mark disconnect; sampled empty/PCM recovery can lag 250 ms
  plus loop delay. Use uptime across wall-clock corrections; zero UTC means unsynced.
- Recovery requires GOT_IP, then Playing + nonempty input + new PCM for radio.
  Further disconnect before PCM return extends the episode. Non-radio ends at GOT_IP.
- Completed first episode freezes until reboot; GET does not clear it. Export and
  start a fresh boot for another trial. Flags Incident/Disconnected/Empty/WifiRecovered/
  PcmRecovered/Complete = 1/2/4/8/16/32; zero timestamps can be valid.
- `configuredPinned` = 0/1 or UINT32_MAX on read failure. Config/AP/channel and
  auto-reconnect are loop-time observations, not exact callback-time snapshots.
- `incidentRoam` freezes latest decision at onset; `latestRoam` keeps updating.
  Outcomes: 0 none, 1 pending, 2 GOT_IP target, 3 elsewhere/unknown, 4 no GOT_IP
  after 30 s (later recovery may update it). That recorder timeout itself does no fallback.
- AP IDs are boot-local numbers from eight RAM-only BSSID slots; 0 unavailable/full.
  Never compare IDs across boots; no SSID/BSSID/password is exported.
- A 32-event numeric SPSC queue keeps earliest events; `queueDropped` makes evidence
  incomplete. Logging off retains records but marks `loggingInterrupted=1`.

Original network extension 528 B: store/archive/RTC SLOW 6848/6852/6880 B.
Newer [panic breadcrumbs](../radio-crash-diagnostics.md) supersede those totals.
Legacy 6324 B banks export `network:null`; bad extension need not invalidate rings.
Only startup archives to flash; power loss/torn commit can lose unarchived onset.
Export before downgrade; old firmware cannot read larger archives. Page-zero JSON
reservation 12 KiB vs 8 KiB other pages; collector caps response at 16 KiB.

## Run a comparison

Network → **Background Wi-Fi roaming** applies immediately and persists
`radio/wifiRoam` (missing/factory reset defaults true; backup includes it).
GET `/api/radio/diagnostics/roaming` shows enabled/AP/channel/RSSI.
POST requires `enabled=0|1&expected=0|1`; invalid 400, stale/busy 409, save failure 500
without changing mode. No-op writes avoid flash. Disabling does not disconnect;
a running scan may finish but cannot join. Startup/browser scanning remain available.

Use a fresh boot, detailed logging on, same station/position and serial closed.
Listen for at least 40 minutes each with roaming on, then off:

```sh
python3 tools/watch_radio_network.py http://radio.local --duration 2400 --output .pio/roam-on.jsonl
python3 tools/collect_radio_diagnostics.py http://radio.local --output .pio/roam-on-final.json
curl --max-time 10 -X POST -d 'enabled=0&expected=1' http://radio.local/api/radio/diagnostics/roaming
```

Begin the off trial from a fresh boot, disable before 10 minutes, use new `roam-off`
filenames, and restore `enabled=1&expected=0` afterward. Verify current values before
POST. Turning off background roaming neither pins an AP nor disables driver recovery.
No router logs are available; isolate a known AP externally if that control is needed.
Watcher reads every minute, records timeout errors, fsyncs captures, refuses overwrite;
Ctrl-C keeps prior captures. Reads are not atomic; duration may include one final request.
Note actual silence/return times. Healthy 40-minute runs alone do not prove a fix.

## Recorded evidence

Files in [2026-11-04/](2026-11-04/): baseline boot 3/count 237, a timed-out HTTP
watch (unreachability alone proves no cause), and boot 4 intermediate image
`764f1f7df1d30ba0`, progressing PCM/empty incident, legacy previous bank readable.
An API enabled→disabled→enabled trial passed and restored roaming; no long off trial.
Host checks covered queue/reason overflow, GOT_IP, freezes, rollover, old archives,
serialization and stale/maintenance controls. Final build `cad09a2851d9f501`:
app 4,708,368 B, DIRAM 262,539 B, RTC SLOW 6880 B. It added current AP fields and
recent-PCM recovery; not installed in that pass. Listening and first-incident
retention remain unverified. Display/retry/vendor code was unchanged.
