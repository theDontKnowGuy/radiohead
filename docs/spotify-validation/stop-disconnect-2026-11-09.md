# November 9: Stop, lost Wi-Fi and Home crashes

The owner reported crashes after Mac Spotify Stop and a receiver disappearing
while still running. Several distinct failures were found; no single regression
commit or common initiating cause has been established.

## Current known device state

Last observed image: **`fcc2828a57733e5445c8d6baecf41df6c36f454e0b36adfb4165a7b6d85798c5`**,
installed/hash-verified. Without debugger or serial monitor, owner confirmed Spotify
playback/song changes, Mac pause/resume, Home return and selection of radio worked.
Final HTTP: same build, radio Playing, continuous boot 10 and successful connect/
await-playback breadcrumbs. This is a short functional pass; memory/long soak remains open.

A weather allocation retry guard is built/tested in **`e61638bab2cb1fa85bb0f03c864381688718a76e8fee72f39165ca1959c0971e`**
(app 4,812,000 B), **not flashed in this record**. Don't confuse built with installed.

## What the evidence established

| Build / capture | Finding |
| --- | --- |
| `fd440829ddcc5782…`, crash `ebe52dd3` | Exact ELF: loopTask→Spotify previous icon→LovyanGFX alpha PNG→readRect null write, StoreProhibited cause 29/address 0 |
| `a52e7375e267adc3…` | First 11-icon fix installed later; activation/song-change still lost network. Live decoder stack blocked in established CDN TLS body read |
| `d7c8c74d83aa3472…` | Cancellable-read fix installed by owner; failure reproduced, but decoder now acknowledged suspension. Local Audio stayed parked; restore failure stage then unknown; latest failed allocation 444 B DMA/internal |
| `11d3f3149e651ea1f…` | Numeric allocation trace identified four 194 B failures at uptime 48,079–48,137 ms: ppTask→Wi-Fi output→esf_buf_alloc_dynamic→internal DMA malloc, at loss of network |
| `9f052d525af35475…` | Fixed TX pool installed; Spotify/song changes worked. Home return then crashed: `19fb8805`, Home tile→same unchecked alpha-PNG allocator |
| `fcc2828a57733e54…` | Extended compositor covers Home/lists; short owner/HTTP playback/pause/Home/radio test passed |

Live post-reboot DMA free was once 1815 B/largest 1344 B, with five failures;
that supports pressure, not the exact heap state at the preserved icon crash.
The dynamic-TX trace identifies a failing consumer, not every allocation that
consumed the heap. Wi-Fi could report connected/strong RSSI with no disconnect
counter even while packets stopped.

## Fixes and boundaries

**Alpha assets:** LovyanGFX 1.2.21 allocates a DMA BGRA scanline without checking
success. Prepared flash RGBA runs replace that path for 40 assets: 11 transport/
Back/Wi-Fi, 18 Home and 11 list surfaces. Same pixels, geometry and alpha, no vendor
edit; opaque photos keep PNG. All match PNG on four RGB565 backgrounds and pass
100 heap-forbidden readable/fallback/clipping repetitions under ASan/UBSan.
Full native renders/build passed; 320×240 Home/Spotify fixtures inspected.
Final installed app: 4,811,936 B, slot free 1,741,664 B, DIRAM 267,767/341,760 B.

**Cancellable stream:** `stream-cancel-bell.patch` / `stream-cancel-cspot.patch`
use nonblocking TLS read/write/handshake, 100 ms select slices, 5 s operation limit
and reset-generation cancellation. Metadata wait checks every 100 ms. Cancellation
skips CDN retry, closes/discards pending bytes, unwinds Vorbis without EOF/depleted,
then acknowledges suspension. Negative results cannot become unsigned lengths;
codec callback leaks no exception. DNS and initial TCP establishment remain blocking.
Fresh-patch production policy/socket/HTTP/CDN/lifecycle sanitizer tests passed;
shim tests are not TLS interoperability/device proof. Existing handoff ack gate stays.

**Network memory:** Arduino otherwise overrides compiled Wi-Fi buffers. Select
`WiFi.useStaticBuffers(true)` before station or setup-AP initialization; static
RX/TX and RX/TX BA windows **4**, dynamic RX **32**, cache TX **4**. TX reserves
roughly 6.4 KiB at startup instead of per-packet DMA. Management/control allocation
can still fail. Cached/fresh config enforcement, export validation, startup and
19 roaming cases passed; HTTP resources report the actual policy/counts.

**Session/reconnect diagnostics:** terminal outer exception now clears
`sessionReady`, marks paused and reports failed task; restart recreates it.
Pairing readiness/advertised mDNS is not proof of a running cloud session.
HTTP fields: `sessionPhase`, `sessionTaskAlive`, `sessionErrors`,
`lastSessionErrorAtMs`, `lastEvent/AtMs`, `pauseEvents`, `disconnectEvents`.
No USB writes from the redacted logger. First eight allocation failures record
capabilities/eight caller PCs in ordinary RAM; collect before reset. Media breadcrumb
operation 5 distinguishes −4 release/ack timeout from −5 local I2S restore failure.
Xtensa return PCs need `esp_cpu_process_stack_pc` normalization before addr2line.

AP reconnect fixes also removed uninitialized DNS pointer use, resolver leaks and
incorrect EINTR/EOF byte-count handling; sanitizer fixtures passed. They were real
source defects, not demonstrated initiators of the observed outage. Mercury
registry/playback concurrency concerns remain separate, unproved follow-up risks.

**Weather:** final capture counted 47,384 failed 8 KiB internal task-stack
allocations from `updateWeatherData`. A 30 s retry delay after creation failure
passes 10,000-loop/repeated-failure/eventual-success/explicit-refresh/rollover tests.
That extra build is uninstalled; it does not establish more memory headroom.

## Preserve evidence and repeat

```sh
python3 tools/spotify_s3_capture.py --base-url http://radio.local \
  --output .pio/spotify-stop-trial.jsonl --duration 120 --interval 5
```

Keep serial/debugger closed. Repeat Stop/resume, device-list visibility while
stopped, transfers, source returns and Home/list navigation. If it fails, collect
[crash](../radio-crash-diagnostics.md), [recorder](../radio-stall-diagnostics.md) and
session/resources before reboot. A stored no-overwrite dump can be older than the
current failure; preserve/decode before explicit clear/rearm. HTTP timeout alone
does not distinguish radio, session and network failure.

Private evidence under `.pio/`: `spotify-stop-crash-20261109.json`,
`spotify-after-reboot-20261109.json`, `spotify-failed-start-live-20261109.jsonl`,
`spotify-dma-trace-live-20261109.jsonl`, and `private-spotify-stuck/` task/RTC/serial captures.
Early cross-subnet failure also persisted after moving Mac to the radio subnet;
a 45 s WCH UART capture was empty because diagnostics used native USB, not proof of a hang.
Native USB opening reset one reproduction. JTAG detach left CPUs halted in this
setup; the later touch freeze and watchdog/reset after debugger use are perturbed
evidence, not spontaneous failures. Explicitly resume/verify both CPUs and close
OpenOCD after any authorized capture. No-debugger reproduction established the Home crash.
Frequent HTTP polling may affect the resource profile; final recording stopped at 172 s.
