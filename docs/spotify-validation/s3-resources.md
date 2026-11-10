# Spotify resource measurements

Full S3 remains open. These September measurements used earlier hardware;
current [PCM5100/HX-A244](../hardware/current-audio-hardware.md) and later firmware
need a fresh run. Keep TFT/web/weather active and USB serial closed.

## Capture

```sh
python3 tools/spotify_s3_capture.py --base-url http://radio.local \
  --output ~/.radiohead-recovery/spotify-s3-run01.jsonl \
  --duration 3600 --interval 5 --weather --control-probes 20
```

Use a new private path and exact running-image/ELF identity. The tool sequentially
reads resources, PCM diagnostics, player and optional weather each interval;
keeps timings/status and diagnostics, not track/weather bodies or signed URLs.
Optional 20 same-volume revisioned POSTs measure acknowledgments and queue normal
saves; they are bounded and off by default. Status GET latency is not command or
source-to-first-audio latency. Annotate actual listening/transfers/renewal and use
equal settling periods for ten-transition memory batches.

GET `/api/spotify/resources` reports `[free, lifetime minimum, largest block]` bytes
for internal 8-bit, internal DMA and PSRAM. Capabilities overlap—do not add them.
`stackBytes` is minimum unused bytes; zero may mean absent/unfound task. Loop buckets
have upper bounds 1/2/5/10/20/50/100 ms, then unbounded; counters are since boot.
No heap walk or serial output occurs in this endpoint.

## September 29 results

First probe `13b86441…` rebooted on `xTaskGetHandle` name-length assertion:
`mercury_dispatcher` exceeded 15 characters. Fixed lookup guards/truncated name
`mercury_dispatc`; corrected ELF `e2025288ae40a059afb13869d93f083c1e706089ffbafc5f60bee994335f363e`
was app0-only flashed/hash-verified with partitions/NVS/LittleFS preserved.
Private evidence: `~/.radiohead-recovery/spotify-s3-20260929/`.

| Corrected-image test | Measured result |
| --- | --- |
| iPhone attempt, 180 s | Saw device but did not connect; no output/PCM. Player 36/36, p95 30.42 ms; resource 1 timeout, diagnostics one 2.16 s response; no allocation failures |
| Computer Spotify, 180 s | Owner heard clear audio; 36/36 each endpoint, all output active; no empty polls/I2S errors, lowest 5 s PCM 863,232 B |
| Playing status latency | Player p95 38.05 ms, max 348.43; weather p95 34.22, max 57.54; resource p95 142.87 |
| Playing loop/resource | p95 ≤5 ms/p99 ≤20 ms over 80,355 gaps; min free internal/DMA/PSRAM 24,915/17,235/6,315,356 B; min largest internal 14,336 B |
| 110 s control run | 20/20 POSTs, p95 32.69/max 160.58 ms; 22/22 endpoint samples, no reset/allocation/I2S failures |
| One Spotify→radio | Local NYOX Playing, Spotify released; 6.15 s HTTP included redirect/page and is not first-audio timing; no reverse/listening result |

Worst unused decoder stack **4744/32768 B**, queue **7812/32768 B**: both fail
25% reserve (8192 B), despite meeting 1 KiB floor. iPhone connection remained unresolved.
Known queue: 384 KiB PSRAM; output/connect stacks 8/16 KiB. Future TLS/decoder/web/UI
allocation burst was not budgeted, so sampled free memory cannot pass that check.

Use [the fixed S3 limits](../spotify-native-validation.md): zero failures, ≥1 KiB
and 25% owned stack reserve, free/biggest-block burst +25%, settled batch leak
checks, control/status/source p95 ≤250 ms/500 ms/5 s and no stable-network dropouts.
Two-minute/three-minute runs do not establish renewal, network recovery or stress/soak.
Do not enlarge stacks blindly at the expense of scarce DMA/internal memory, or
change failed thresholds after the run. Later fixes/results are in [progress](progress.md).
