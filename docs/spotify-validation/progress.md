# Spotify: what has actually been tested

Default firmware includes native Spotify. Use current [PCM5100/HX-A244 hardware](../hardware/current-audio-hardware.md),
not the permanently retired MAX boards. September measurements belong to that older
assembly. [Remaining test requirements](../spotify-native-validation.md).

| Stage | Recorded standing |
| --- | --- |
| S0 baseline | 30 min radio/podcasts with owner reporting good audio; existing allocation/stack findings retained below |
| S1 native playback | Real clear playback, controls and earlier token renewal; corrected-final-image two-hour/outage run incomplete |
| S2 coexistence | Owner explicitly accepted completion 2026-09-29; formal 100 directed/20 rapid counts and memory trend not established |
| S3 resources | Build/short computer playback/control checks passed; decoder/queue failed 25% stack reserve; full budget open |
| S4 eight-hour resilience | Not verified |

## Latest, November 9

See [Stop/disappearance evidence](stop-disconnect-2026-11-09.md) for exact builds.
Captured alpha-PNG DMA allocation crashes (Spotify icon, then Home tile), decoder
blocked CDN read and dynamic Wi-Fi TX allocation failure led to cancellable streams,
fixed TX pool and 40 allocation-free alpha assets. Installed `fcc2828a57733e54…`:
owner confirmed playback/song changes, Mac pause/resume, Home and radio return;
HTTP confirmed radio Playing without restart. Short functional success, not a soak.
Weather retry guard `e61638…` is built/tested but was not installed in that record.
Tight memory and the historical regression's initiating change remain unresolved.

November 7 token-refresh fix releases the pending gate on every exit, catches
standard queue exceptions, retries after 5 s and checks shutdown every 100 ms;
no exception-text logging. `check_spotify_token_refresh.py` sanitizer/idempotency/
conflict tests, build/artifact checks passed. No dedicated device renewal-recovery
trial was recorded for that change.

## September baseline and recovery

Started at `b47961c5684731e8e2d79c5c4f5999eab99aa5c8` on `spotify/native-s0-s1`.
Framework ESP-IDF 5.5.5/Arduino 3.3.11, LovyanGFX 1.2.21, ArduinoJson 7.4.3.
Baseline app 3,542,448 B in 6,553,600 B slot, reported RAM 96,340 B.

Private recovery `~/.radiohead-recovery/spotify-s0-s1-20260924/` is mode 0700,
files 0600 (`MANIFEST.md`, `SHA256SUMS.private`). Includes source image, NVS and
OTA metadata; attempted full-flash/original-app reads failed, so no full-original
backup was claimed. Preserve these secrets and NVS/LittleFS; never commit or full-erase.
Standalone tests wrote only verified app0 after partition/OTA selection checks.

S0: 358 samples/1787.3 s within a 30 min capture; owner heard good radio, podcast,
volume/mute, screen and web behavior. Instrumented static overhead +272 B RAM/
3360 B linked flash. Summarize with `tools/spotify_validation_summary.py`.

| Source | Samples | Min free internal / DMA / PSRAM B | Max loop gap / audio call µs |
| --- | ---: | --- | --- |
| Radio | 171 | 39804 / 32140 / 6656700 | 1596289 / 217156 |
| Podcast | 187 | 43756 / 38524 / 6912928 | 1348937 / 9177 |

Lifetime minima internal/DMA 17744/10080 B; smallest largest block 26612 B.
Owned stack minima: loop 10228, controls 3276, audio **404**, updater 7464 B.
One failed 16717 B internal allocation and three recovery events were pre-existing.
Input above nominal capacity was later explained by the library's reserve buffer,
not proven overflow. Capabilities overlap; no PCM/I2S/end-to-end web latency baseline
was measured by that logger. Keep baseline defects separate from new regressions.

## Native candidate: useful failure history

Pinned Waveshare/cspot/Bell example needed adaptation to this board/IDF 5.5.5:
private NVS provisioning, explicit mbedTLS RNG, software AES/external TLS allocations,
matching partition/LittleFS preservation and actual I2S sink. Upstream credential
headers and autoformat were unsuitable. Exact pinned revisions: [build record](../framework-consolidation-validation.md).

| Runs | Finding / later result |
| --- | --- |
| Early provisioning/TLS | Default stdin EOF, missing RNG and crypto/heap failures required native fixes; token issuance alone was not playback proof |
| 10→11 | I2S timeout passed ticks to an API expecting milliseconds; direct 50 ms fixed writes, but internal heap still fell to 147 B with ≥125 allocation failures |
| 12 | Ordinary-allocation internal threshold 16384→1024 B favoured PSRAM; clear playback/control checks, zero sampled allocation/I2S failures over 1883.5 s; token renewed after 1800.856 s with PCM continuing |
| 12→14 | Phone spontaneously selected itself; radio drained three preloaded songs. Adding DEPLETED alone did not fix it: run 13 reproduced failure before that event |
| 15 | Output-boundary `notifyAudioReachedPlayback()` advanced index/refilled queue; fourth natural song played and phone stayed on radio |
| 16 | Producer returned actual accepted PCM bytes for backpressure; long pause resumed the correct song, 1547.4 s/9 starts, zero sampled queue/allocation/I2S/watchdog failures |
| Short continuation | Owner reported 20 Next taps, pause/seek/volume/transfers and three cold boots (start phone music before transfer); adjacent duplicate-song test inconclusive |
| Original outage trial | ~15 s router outage failed recovery; final 3193 s capture had 84 decoder watchdog events in dead-socket HTTP parsing |
| 18 | Two range recoveries, then reconnect shared-connection race caused panic; atomic shared-pointer snapshots/nonrecursive reconnect added |
| 19 | TLS stream-open exception after range timeout aborted/rebooted; normal radio app0 restored/hash-verified and owner confirmed screen/audio |

Short runs proved useful fixes, not full S1. Run 12 had a spontaneous transfer and
was under two hours; later images did not inherit its renewal acceptance automatically.
Two adjacent identical tracks could suppress file-ID boundary detection; no confirmed
playlist test established a failure or a fix. Private logs retain detailed run hashes.

## Integrated handoffs and promotion

Owner authorized the narrow Audio vendor handoff patch on September 25.
[S2 audit](s2-output-handoff.md): park/release/restore without destroying Audio,
source generations, bounded stop/PCM cancellation, shared mute/volume/tone.
Early low-memory/watchdog/silent-return failures were retained in that record.
Removing PCM-task USB prints fixed repeatable slowdown: 534528→~882000 B/5 s.
Larger 384 KiB queue was a separate reserve change.

On `ed4b809d…`, one Wi-Fi outage recovered with clear audio/no reboot. Twenty local
transitions (10 each direction) reached Playing; podcast median 9.51 s, radio 2.42 s.
Rapid run timed out on command three. Owner heard several phone-controlled directed
handoffs but one Next moved playback to phone. Formal counts/leak trend remained
open when owner accepted S2 and asked for S3.

September 29 default promotion exported integrated hash
`7b0c03c977b3d40cf674c21333413678dd67826b80aea6a3a344f23a29d0b4a7`
to ordinary `firmware.bin`; matched the already-running image. Later [single build](../building.md)
replaced the two-build process. This did not close S3/S4.

## Resource results and next run

[S3 measured run](s3-resources.md): corrected task-name endpoint, 3 min clear
computer-controlled playback; player p95 38.05 ms, 20 same-volume POSTs p95 32.69 ms;
no sampled allocation/I2S failures. Minimum internal/DMA/PSRAM 24915/17235/6315356 B,
largest internal 14336 B. Decoder 4744/32768 B and queue 7812/32768 B unused fail
25% stack reserve. iPhone connection unresolved then. One return to radio reached
Playing but reverse/first-audio timing and full stress stopped at owner's request.

Repeat on a final identified build/current assembly with serial closed: 2 h+renewal,
counted directed/rapid transitions with equal settled memory endpoints, actual
stack/burst budget, outages/transfers and full soak. Never infer audible continuity
from HTTP 200/PCM counters alone or reinterpret older failures as current-image tests.
