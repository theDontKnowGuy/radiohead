# Diagnose radio pauses

Normal firmware includes a flight recorder. During a pause, note local time and
save promptly, with USB serial closed:

```sh
python3 tools/collect_radio_diagnostics.py http://radio.local --output .pio/radio-incident.json
```

Use a new path and substitute the displayed IP if needed. Keep `/api/player` and
`/api/spotify/resources` responses too. After a crash, let reboot/archive finish
before removing power; also [collect the panic report](radio-crash-diagnostics.md).
The [November 4 incident](radio-stall-investigation/README.md) proved Wi-Fi loss
starved that playback; it did not establish why Wi-Fi failed.

## Automatic recovery

Saved radio marked Playing with **15 s no PCM progress** changes to Connecting.
Retries: **2/5/15/30 s**, checking every **5 s** while Wi-Fi is absent;
connection timeout **12 s**. Backoff resets after **30 s PCM progress**, or explicit
selection/stop. Mute/zero volume/encoded silence still count as progress.
Podcasts, Spotify, previews and stopped audio are excluded. Recovery runs on the
loop from an atomic PCM counter, independently of detailed logging. See newer
[playback-aware roaming](wifi-roaming.md) for AP-handover recovery.
The installed recovery image showed resumed PCM; deliberate outage/listening tests remain open.

## Logging control and export

**Device & maintenance → Open diagnostics → Playback logging** toggles detailed
logging immediately and persistently (`radio/radioLog`, default true).
Off retains basic reset/last-service breadcrumbs, existing banks and fixed RTC
allocation; it stops detailed sampling/events/timing. Boot-time archival still
writes flash. Failed saves retain the old mode. Factory reset restores default.

GET `/api/radio/diagnostics/config` returns `enabled`; POST accepts
`enabled=0|1&expected=0|1`. Invalid 400, stale/busy 409, save failure 500.
`/api/device` includes `radioDetailedLogging`; banks export `detailedEnabled`.

GET `/api/radio/diagnostics?bank=live|previous|crash&offset=0` returns up to eight
physical slots; follow `nextOffset` until −1 and sort by `seq` (collector does this).
400 means bad bank/offset; 503 allocation/uninitialized. `available:false` means
unreadable/unvalidated, not healthy playback. Live pages are not one frozen snapshot;
collector detects boot/build changes. Polling can itself affect timing.

## Retention

32 samples every 5 s (~160 s), plus 24 events coalesced at most once/s; 250 ms
measurement polls can miss short dips. Rings overwrite; the separate first-network
incident is described in [network notes](radio-stall-investigation/network-followup.md).
Records are checksummed individually. Numeric data excludes credentials/SSID/URLs.
Station slot is zero-based, −1 for unsaved/no station.

RTC NOINIT can survive software and ordinary panic/watchdog resets, but power-on/
brownout reject it and other reset paths are not guaranteed. Before boot audio,
LittleFS archives to `/radio-previous.bin`; panic/watchdog also replaces
`/radio-crash.bin`. Normal later boots preserve that crash file. Verified temporary
write/rename preserves the old file on failure; `rtcRecovered/filesystemReady/archiveOk`
report current boot success. Filesystem formatting removes archives.
No continuous flash writes during playback. Current sizes/legacy compatibility are
in [panic diagnostics](radio-crash-diagnostics.md); original logger-only figures are obsolete.

## Read the fields

| Fields | Meaning |
| --- | --- |
| `ms`, `utc` | Uptime; epoch seconds (0 before sync) |
| `bootResetReason`, `endingResetReason` | Start/end reset reason: software 3, panic 4, watchdog 5/6/7, brownout 9 |
| `lastPhase`, `lastPhaseMs` | Last entered service, not proof of cause; UINT32_MAX invalid |
| `input`, `inputMin`, `capacity` | Compressed fill/minimum; capacity is nominal main size |
| `pcmBlocks`, `pcmIdleMs` | Blocks reaching pre-I2S hook / idle while Playing; not DMA/speaker evidence |
| `wifi`, `rssi`, `disconnects`, `disconnectReason` | Connected state, dBm, total events, last reason |
| `heapFree/Min/Largest`, `psramFree` | Internal heap/free minimum/largest block and PSRAM |
| `allocationFailures`, `failedBytes` | Failed allocation count/last size |
| `loopGapUs/audioUs/webUs/slowPhase/phaseUs` | Window maxima; different fields can cover different completed work |
| `state`, `flags` | stopped/connecting/playing/failed = 0/1/2/3 (−1 no radio); flags running/mute/zero/radio = 1/2/4/8 |

Phases: boot/audio/web/network/weather/controls/media/display/idle/diagnostics = 0–9.
Events: slow/lost/EOF/ready = 1/2/4/8; recovery/timeout/unexpected-stop = 16/32/64;
station/Wi-Fi = 128/256; PCM stall/resume = 512/1024; ≥100 ms loop gap/allocation
failure/I2S timeout = 2048/4096/8192. A stall event starts at 1 s, distinct from recovery's 15 s.
`pcmIdleMs=0` while Connecting is not evidence sound continued; compare counters/input/state.

The pinned buffer has **655,350 main + 65,535 reserve = 720,885 B**. Observed fill
697,170 B is within allocation; exceeding nominal capacity alone is not corruption.
Do not clamp or vendor-patch it on that basis. Low input/stream events with healthy
services suggest incoming starvation; Wi-Fi loss strengthens that explanation.
Healthy Wi-Fi cannot distinguish broadcaster from ISP/router/TCP delays. Progressing
PCM during silence requires mute/filter/encoded-silence/output checks.

Recorded host checks: radio diagnostics, recovery, filter/tone, startup, build and
diff passed. Software-restart archival and a real Wi-Fi-loss/recovery were observed;
physical logging-toggle, deliberate outage/retention and listening coverage remain
separate. Controlled panic acceptance is recorded in the newer panic guide.
