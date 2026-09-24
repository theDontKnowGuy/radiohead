# Native Spotify S0–S1 evidence ledger

Started 2026-09-24 on branch `spotify/native-s0-s1` from `b47961c5684731e8e2d79c5c4f5999eab99aa5c8`.
The source handoff is `docs/spotify-native-validation.md`. This ledger records observed
results separately from pending device tests. Private serial captures, NVS and
firmware backups are kept outside the repository.

| Package | Code/build | Hardware/function | Resources/timing | Evidence / next action |
| --- | --- | --- | --- | --- |
| S0 | PASS | PASS (user listening report) | PASS (baseline measured; existing defects below) | 30-minute private capture; `tools/spotify_validation_summary.py`. |
| S1 | PASS (standalone build/upload) | FAIL (run 10 audio unintelligible) | FAIL (allocation failures and audio loss) | Corrected I2S candidate run 11 awaits a fresh phone-controlled track; renewal, controls and restarts remain unverified. |
| S2 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Outside this request. |
| S3 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Outside this request. |
| S4 | NOT VERIFIED | NOT VERIFIED | NOT VERIFIED | Outside this request. |

## Baseline before changes

- `pio run -e esp32s3` **PASS** at source `b47961c` on 2026-09-24.
- PlatformIO platform 55.3.311; Arduino framework 3.3.11; ESP-IDF libraries
  5.5.5 `b774170ff46`; toolchain xtensa-esp-elf 14.2.0+20260121;
  vendored ESP32-audioI2S 4.0.0; LovyanGFX 1.2.21; ArduinoJson 7.4.3.
- Linker static RAM: 96,340 / 327,680 bytes (29.4%). Firmware: 3,491,471
  bytes reported by PlatformIO. Actual `firmware.bin`: 3,542,448 bytes.
  Each OTA application slot is 0x640000 = 6,553,600 bytes; baseline image
  headroom is 3,011,152 bytes. Static reports do not establish runtime fit.
- `git diff --check` **PASS** before changes. Existing untracked `build/`,
  `docs/spotify-integration-research.md` and `docs/spotify-native-validation.md`
  predate this branch and are preserved.
- USB serial `/dev/cu.usbmodem11201`, USB VID:PID 303A:1001, reported as ESP USB
  JTAG/serial debug unit. `esptool chip-id` identified ESP32-S3 revision 0.2,
  8 MB embedded PSRAM. User identifies the currently attached speaker module as
  MAX98357A; one channel is installed, with a second planned.
- Private recovery directory: `~/.radiohead-recovery/spotify-s0-s1-20260924/`
  (mode 0700). It contains source baseline firmware (SHA-256 prefix `b76d6567`),
  a successful 20,480-byte NVS read, and an 8,192-byte OTA metadata read, all
  mode 0600. OTA metadata reports app0 as active. A full-flash read at 460800
  and app0 read at 115200 both failed with `Serial data stream stopped` after
  partial transfer; no full-flash backup is claimed. Neither failed read
  modified flash. Recovery of the known source build, preserving NVS/LittleFS:
  `pio run -e esp32s3 -t upload --upload-port /dev/cu.usbmodem11201`.
  A full original binary restore is unavailable; preserve the private NVS
  backup and do not erase the chip.

## S0 runs

- Instrumentation is gated by `esp32s3-validation`. It samples every five
  seconds, uses fixed event and gap buffers, records internal/DMA/PSRAM heap
  capabilities separately, allocation failures, task stack reserves, audio
  input fill and main-loop/audio/web call gaps. IDF 5.5.5 documents task high
  water marks in **bytes**. Source 0=stopped, 1=radio, 2=podcast.
- `pio run -e esp32s3-validation` **PASS**: 96,612 bytes static RAM versus
  96,340 normal baseline (+272); 3,494,831 bytes linked flash versus
  3,491,471 (+3,360). Diagnostic firmware was uploaded to the identified
  ESP32-S3 with esptool write verification **PASS**. NVS and LittleFS partitions
  were preserved. This measures static overhead only; runtime cost is pending.
- Private capture `s0-serial.log` completed after 30 minutes. It contains 358
  five-second samples spanning 1,787.3 seconds (the capture began before the
  first sample and ended after the last). Source counts: 171 radio samples
  (~14m15s) and 187 podcast samples (~15m35s); the boot and transition gaps
  complete the 30-minute wall-clock capture. The user reports radio and
  recorded show playback, volume, mute, station selection, screen and web
  actions all worked and sounded good. Exact action times, station/show IDs
  and HTTP response latency were not recorded. Log summary command:
  `python3 tools/spotify_validation_summary.py ~/.radiohead-recovery/spotify-s0-s1-20260924/s0-serial.log`.

  | Source | Samples | Least free internal / DMA / PSRAM | Least audio-task stack reserve | Largest loop gap / audio call |
  | --- | ---: | ---: | ---: | ---: |
  | Radio | 171 | 39,804 / 32,140 / 6,656,700 B | 404 B | 1,596,289 / 217,156 us |
  | Podcast | 187 | 43,756 / 38,524 / 6,912,928 B | 404 B | 1,348,937 / 9,177 us |

  The three heap capabilities overlap and cannot be summed. Across all
  samples, the smallest largest allocatable block was 26,612 B for internal
  and DMA, and 6,553,588 B for PSRAM. The boot-to-date minimum free was
  17,744 B internal, 10,080 B DMA, and 6,656,700 B PSRAM. The maximum
  five-second-window p95 loop-gap bucket upper bound was 20,000 us; max
  `audio.loop()` call 217,156 us; max `server.handleClient()` call 6,688 us.
  Maximum task count was 16. Lowest stack reserves were 10,228 B loop,
  3,276 B controls, 404 B audio and 7,464 B updater (ESP-IDF units: bytes).
  Input fill minimum was 10,634 B. These are S0 baseline observations, not
  S3 acceptance measurements: web end-to-end latency, actual PCM underruns,
  and I2S errors were not directly measured.
- The uninstrumented normal build after source changes also **PASS** with
  unchanged 96,340-byte static RAM. Its linked flash report is 3,491,499 bytes.

## S1 runs

The standalone candidate built reproducibly from
`experiments/spotify-native/prepare.sh` plus the pinned patches on ESP-IDF
5.5.5. The current run-11 image is 1,768,560 B, leaving 4,785,040 B of a
6,553,600 B app slot. Its partition-table binary is byte-identical to the
Radiohead table. Flashing only app0 at `0x10000` completed with hash
verification; NVS, otadata, bootloader and LittleFS were left in place.
The run-6 flashed image SHA-256 begins `7046dd096890485f`; run 7's flashed
image begins `f2f61bd9573a7cff`.
Run 8's flashed image begins `93d70c409bb2610f` and retains the same
1,763,040-byte image size; its TLS memory allocation mode differs.
Run 9 uses a 1,768,304-byte image with software mbedTLS AES, SHA-256 prefix
`cde7ae5dabc97faf`. Run 10 modifies the I2S write loop and telemetry,
producing a 1,768,560-byte image with SHA-256 prefix `42dda8052a8ab31b`;
its measured runtime failure is below. Run 11 fixes the I2S timeout units and
was flashed and hash verified (SHA-256 prefix `6e9777557ffc8003`). A fresh
clone, pinned submodules and run-11 patches built independently to a
1,768,656-byte image.
Its generated configuration confirms external mbedTLS allocation and software
AES, and its partition table matches the Radiohead binary byte for byte.
An independent fresh clone plus patches of the earlier run-6 candidate built
successfully on the same IDF 5.5.5 toolchain. Its size report showed
237,435 / 341,760 B DIRAM used (69.47%), with 104,325 B static headroom.
The earlier independent binary was 1,779,296 B; the 96-byte difference in
the current fresh build was not investigated. Static headroom is separate
from runtime heap.
The flashed run-7 build reports 221,059 / 341,760 B DIRAM used (64.68%),
120,701 B static headroom. Its runtime limits are reported with run-7 logs,
not inferred from the linker report.
Wi-Fi association and DNS succeeded on device. The first provisioning attempt
failed because `fgets(stdin)` returned EOF with the default USB serial/JTAG
console. The second candidate installs the USB serial/JTAG driver and reads
bounded lines directly. It stored the two privately supplied app credentials
in the `spotify` NVS namespace, started HTTP port 8080 and advertised the
pairing service. A redacted host GET of `/spotify_info` returned HTTP 200,
status 101, with no active user before phone pairing. The phone then supplied
a login blob; it persisted across reboot. The native AP connection and
authentication succeeded, and the device fetched one CDN access token. A
track file selection, key exchange and CDN URL fetch reached the device in
failed runs. Run 10 produced audible but unintelligible sound, so S1 is
**FAIL**. Run 11 is awaiting a new device track and listener report.

Run-6 standalone diagnostic samples at paired, authenticated and output
idle states show 103,983 / 74,931 / 42,903 B least free internal RAM, with
96,195 / 67,143 / 42,903 B DMA free and 8,343,952 / 8,343,952 /
8,111,984 B PSRAM free, respectively. These are **not** track-decoding minima.
The boot-to-date DMA minimum is 4,648 B and internal minimum 12,436 B in
the output-idle sample; largest internal/DMA block is 30,720 B. Main task
stack reserve at authentication is 24,856 B, output task minimum 14,268 B,
queue task minimum 23,616 B so far. Decoder stack has not been measured
during active decode. Capability totals overlap; do not add them.

- A private file containing the developer app Client ID and Secret was
  provisioned by the user outside the repository with mode 0600. A host-side
  HTTPS `client_credentials` probe on 2026-09-24 returned HTTP 200, an access
  token and `expires_in=3600`; neither credential nor token was logged. This
  verifies only app credentials and token issuance. It does **not** verify CDN
  authorization, Premium Connect pairing, native renewal or ESP32 playback.

## Failures and unresolved decisions

- The Waveshare native example is pinned at `9c51b087`, cspot `37b650a5`,
  Bell `ead27050`, mdnssvc `44e78c14` and nlohmann/json `07182ebc`.
  It uses an ESP-IDF 6.0 build, a board-specific ES8311 I2S codec and a
  compiled Spotify client secret for CDN tokens. Its audio and authentication
  paths cannot be used unchanged on this Arduino board. Compatibility and
  secret-free native authorization remain open.
- The user identified MAX98357A; the firmware uses BCK 15, WS 17 and DOUT 16.
  Wiring has not been electrically measured; audibility is still pending.
- The S0 run recorded one failed internal 16,717-byte allocation
  (`MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT`), a 404-byte low audio-task stack
  reserve and three station recovery scheduling events (two observed station
  connection timeout messages in the log). User listening remained good.
  These are existing radio findings, not Spotify regressions. One reported
  input fill exceeded the library's nominal capacity; this could be a
  sampling/interpretation issue and needs investigation before claiming a
  buffer defect.
- The Waveshare example uses a distinct SPIFFS partition with
  `format_if_mount_failed=true`, while this radio's data partition holds
  LittleFS artwork. Flashing it unchanged could destroy saved artwork. Its
  partition layout and credentials header are therefore unsuitable.
- A standalone candidate build under `/tmp` with the current pioarduino
  PlatformIO platform selected ESP-IDF 5.5.5, not the example's IDF 6.0. After
  selecting the example's `main` source directory, PlatformIO failed before
  compilation with `ModuleNotFoundError: SCons.Tool.FortranCommon`. This is a
  host build-system failure, not a cspot compatibility result.
- Failed S1 boot 1: default `fgets(stdin)` returned EOF instead of waiting
  for provisioning. Using the ESP32-S3 USB serial/JTAG driver for bounded
  private input fixed this; credentials are now in NVS, not compiled in.
- Failed S1 boot 2: mbedTLS 3.6 under ESP-IDF 5.5.5 returned `-0x7400`
  (`MBEDTLS_ERR_SSL_NO_RNG`) because the upstream candidate assumes mbedTLS 4
  supplies the RNG. Adding `mbedtls_ssl_conf_rng` with ESP32 hardware random
  input allowed a real AP TLS connection and authentication.
- Failed S1 boots 3–5: `cspot_player` needed a contiguous 32 KiB internal
  stack after dynamic TLS/task allocations and aborted, despite >100 KiB
  total internal free. The candidate had also statically reserved 131 KiB
  for a nominal 32 KiB main-task stack because `StackType_t` is four bytes.
  Correcting the declaration freed 98,304 B of static DRAM, but fragmentation
  still prevented the later 32 KiB decoder allocation. A 16 KiB decoder
  stack then overflowed when opening the CDN stream. The 24 KiB queue stack
  also overflowed during CDN URL fetch. Both are now reserved as 32 KiB
  static internal stacks; the current rerun has no early abort, but has not
  yet been driven through a track. These failures are S1 candidate failures,
  not S0 regressions. The working native configuration must retain measured
  stack and heap headroom before S1 can pass.
- Failed S1 run 6: a real track selected Ogg Vorbis format 1, retrieved its
  audio key and CDN URL and opened a CDN stream. A concurrent metadata/CDN
  request drove free internal RAM to 2,063 B (largest block 576 B), DMA free
  to 1,207 B (largest block 16 B), and boot-to-date minima to 64 B internal
  and 12 B DMA. `esp-aes` then failed to allocate memory; the queue task
  triggered repeated watchdog reports. No audible playback or clean track
  completion was established. The latest candidate reduces the main stack
  from 32 to 16 KiB and the output stack from 16 to 8 KiB based on observed
  reserves, and adds an allocation-failure counter. Run 7 is currently
  collecting evidence. Any further allocation failure, overflow, watchdog or
  inaudible output blocks S1.
- Failed S1 run 7: reducing the measured overlarge main/output stacks raised
  free memory at authentication, but a real track still selected format 1,
  opened one CDN stream, and started a concurrent CDN request. Free internal
  RAM fell to 2,247 B (largest block 448 B) and DMA to 1,387 B (largest
  block 184 B). There were at least nine allocation failures, two explicit
  AES allocation errors and repeated queue-task watchdog reports. The
  candidate did not deliver accepted audio. ESP-IDF 5.5.5's supported
  `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` was enabled for run 8 to move
  transient TLS allocations to the abundant PSRAM. ESP-IDF recommends
  internal TLS allocation for physical security unless external RAM is
  encrypted. Run 8 is provisional and needs both functional and resource
  evidence; this choice is not an integrated firmware decision.
- Failed S1 run 8: external mbedTLS allocation raised free internal memory
  during output idle, but concurrent CDN stream requests still exhausted the
  internal DMA path used by hardware AES. At the first error, output samples
  showed 39,231 B internal free and 33,207 B DMA free, falling to 15,239 B
  internal and 9,347 B DMA with a 6,144-byte largest block. One allocation
  failure, an explicit `esp-aes` failure and repeated decoder watchdog
  reports followed. Run 9 disables ESP-IDF's hardware AES acceleration so
  TLS uses software AES without the extra internal DMA buffer. Its CPU/audio
  cost must be measured; no S1 acceptance is inferred from the build.
- Failed S1 run 9: software AES avoided the earlier hardware-AES DMA error
  long enough to open three CDN streams and advance through a track, but the
  sink discarded thousands of partial I2S writes (3,458 in the first 63
  seconds), then internal free RAM declined to 159 B with at least 75 failed
  allocations. This is neither acceptable sound nor memory stability.
  Run 10 loops over partial I2S writes with a 200 ms deadline, records PCM
  bytes/short writes/failures, and suppresses per-write log flooding. The
  remaining heap decline must be measured again; its cause is not yet proven.
- Failed S1 run 10: over 356.7 s of capture, two tracks started and two CDN
  streams opened. The sink accepted 1,454,336 PCM bytes but recorded 14,867
  partial writes and 12,804 failed writes; internal free fell to 6,191 B,
  DMA free to 2,319 B and at least two allocations failed. The user heard
  audio but described it as very bad and unintelligible. The installed IDF
  5.5 I2S API expects a timeout in milliseconds; the sink passed
  `pdMS_TO_TICKS(50)`, giving a roughly 5 ms wait with this 100 Hz tick.
  Run 11 passes 50 ms directly. The timing error is proven in source, while
  its contribution to the heap decline and audible fault still needs a
  device measurement.
- S1 run 11: app-only flash and image hash verification passed. The saved
  pairing authenticated after reboot, fetched one authorization token, and
  remained connected and idle for over nine minutes with zero observed
  allocation failures, PCM writes or I2S write failures. No track was
  started during this interval, so it supplies no evidence about sound or
  memory during playback. A phone-controlled real track and listening report
  are required before this candidate can be assessed.
