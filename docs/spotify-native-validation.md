# Spotify: tests still worth running

Native ESP32-only Connect is included in the ordinary [build](building.md).
Use the Spotify phone/computer app for music selection; no runtime companion.
Early “wait for all tests before UI/default build” rules were explicitly superseded
in September. [Current results](spotify-validation/progress.md) remain partial.
Current PCM5100/HX-A244/GPIO2 hardware needs its own checks; old MAX listening
results do not validate the replacement assembly.

## Before a bench run

Identify the radio/IP/port and exact running ELF. Keep a matching recovery build
and private settings/pairing backups. Never erase NVS/LittleFS or compile/log
credentials. Build and focused checks establish code; listening/device evidence
establish actual behavior. Keep real TFT/web/weather loads enabled.

Use numeric counters and bounded captures; no network, allocation or Serial
printing in PCM service. **Opening USB serial resets this radio; a connected
USB host without a reader can block writes for about 2 s.** Use HTTP diagnostics
for normal playback. Private dumps/NVS/logs stay outside Git.

## Test groups

| Group | Required run | Current standing |
| --- | --- | --- |
| S0 baseline | 30 min radio/podcast, normal web/display/controls; resource table and recovery path | Recorded with existing defects |
| S1 native playback | 2 h, actual token renewal, 20 track changes, pause/seek/remote volume/transfers, 3 cold boots and Wi-Fi interruption | Short runs/earlier renewal observed; full final-image run open |
| S2 coexistence | 100 transitions, ≥10 each directed radio/podcast/Spotify pair; 20 rapid/interrupted; tone/mute/min/max volume | Owner accepted S2; formal counts/memory trend incomplete |
| S3 resources | Same transitions with TFT/web/weather, TLS/renewal/recovery and bounded web stress | Earlier decoder/queue stacks failed 25% reserve |
| S4 resilience | 8 h total, ≥4 h Spotify; 10 Wi-Fi outages, 5 transfers, 5 cold boots, 5 sleep/wake, one supported OTA/restart | Not verified |

Also test adjacent identical songs, full-queue pause, rapid seek/skip, DNS/CDN failure,
local selection while reconnecting, and reboot persistence of catalogs/favorites/
Wi-Fi/calibration/tone. Failed runs do not count as clean soak time. Do not intentionally
interrupt flash writes. Counters alone cannot prove every minute sounded correct.

## Source handoff rules

Serialize: invalidate old generation/cancel recovery → request producer stop →
wait bounded acknowledgment → flush PCM → release I2S → configure new owner →
apply volume/mute/tone → start. Timeout leaves a usable stopped/failed state;
never free resources still used by a worker. PCM callbacks cannot reacquire output.
Pause keeps session; local radio/podcast selection overrides late Spotify events;
transfer away never auto-starts radio. Mute never pauses/restarts and remote volume
preserves mute. Preserve routes/settings/partitions and the approved vendor-patch boundary.

## Numerical checks fixed before judging a run

- Zero allocation/stack corruption/watchdog/I2S-ownership failures; no unexplained
  steady-state PCM underrun or audible dropout on stable network.
- Each owned task: **≥1 KiB and ≥25% stack free**, in bytes for installed IDF.
  Unmeasured tasks remain unknown; zero lookup result can mean absent, not exhausted.
- For each memory capability: free covers remaining worst-case allocation burst
  plus 25%; largest block ≥1.25× largest pending contiguous request. Internal/DMA/
  PSRAM figures overlap: never sum them or use PSRAM as proof of DMA reserve.
- Compare warm, equally settled ten-transition endpoints. Investigate loss >4 KiB
  internal, >32 KiB PSRAM or >10% largest internal block; any continuing reproducible
  decline fails even below those thresholds. Account for bounded caches, not reboot recovery.
- Same-LAN control acknowledgment p95 ≤250 ms; status p95 ≤500 ms; healthy-service
  source-to-first-audio p95 ≤5 s. Record p99/max and separate network/handoff delays.
  Bound audio-service gaps against measured buffer duration, not an arbitrary loop target.

Change a provisional threshold only with allocation/timing evidence **before** rerunning.
Record free/min/largest internal 8-bit/DMA/PSRAM, failure sizes/capabilities, owned
stacks/tasks, PCM production/consumption/queue/underruns, source generations,
transition/control latency, reset reason, app size and OTA headroom. Keep runtime
measurements separate from static linker totals. Heap-integrity walks belong at safe checkpoints.

## Reproduce and report

Use [S3 capture commands](spotify-validation/s3-resources.md) and [S2 diagnostics](spotify-validation/s2-output-handoff.md).
Report exact build/hardware, duration/counts, listening observations, worst resources,
failures/recovery and remaining tests. Preserve failed evidence and link the rerun.
Follow [current stop/disappearance findings](spotify-validation/stop-disconnect-2026-11-09.md)
before experimenting; do not lower failed limits to declare success.
