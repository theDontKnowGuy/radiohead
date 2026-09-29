# S3 resource and responsiveness run

Status: implementation built and short device runs measured; full S3 acceptance open.

## First device probe and correction

The first S3 image, SHA-256 `13b864410304ed0511c19c63a2c4c50025b2efae034439d7dd51dfaa2ad83793`,
built and passed app0-only flash verification. A request to the new resource
endpoint caused a device assertion and reboot. Private USB capture recorded
`xTaskGetHandle tasks.c:2864 (strlen( pcNameToQuery ) < 16)`.
The endpoint had queried `mercury_dispatcher`, which exceeds the ESP-IDF
FreeRTOS task-name limit. The code now rejects overlong lookup names and uses
the actual truncated task name, `mercury_dispatc`. This failed image does not
count toward S3 acceptance. The device retains the same partition table,
NVS and LittleFS; the prior verified app0 image is saved privately for rollback.

The corrected image, SHA-256 `e2025288ae40a059afb13869d93f083c1e706089ffbafc5f60bee994335f363e`,
built and passed an app0-only flash and independent verification. The resource
endpoint then returned valid JSON without reboot. With USB serial closed, a
120-second idle/authentication capture and a 180-second attempted-playback
capture used one sequential GET each to resources, PCM diagnostics, player and
weather every five seconds. Raw JSONL files and flash logs are private under
`~/.radiohead-recovery/spotify-s3-20260929/`.

During the attempted-playback run, `/api/player` had 36/36 HTTP 200 responses,
p95 30.42 ms and max 34.13 ms; weather had 36/36 HTTP 200 responses and p95
30.02 ms. The resource endpoint had 35/36 successes, with one three-second
timeout; Spotify diagnostics had 36/36 successes but one 2.16-second response.
The loop-gap histogram gained five samples over 100 ms; its since-boot maximum
rose from 4.39 to 7.61 seconds. Minimum sampled free internal/DMA/PSRAM was
37,127 / 29,447 / 6,378,072 bytes, minimum largest internal block 19,456
bytes, and cumulative allocation failures zero. These are sampled values, not
a future allocation-burst budget.

The phone saw `Radiohead Native Test` but did not connect during the capture.
The session reported ready while output ownership and PCM stayed zero. The
owner later connected from a computer on the same image and suspected an
iPhone-specific issue; the phone result remains unresolved. The decoder
task appeared during the phone's attempt and reported a 4,840-byte stack high
water mark against a 32,768-byte allocated stack, about 15%, below the preset
25% S3 reserve. The 1 KiB floor was met. The current image therefore also
fails the stack gate; increasing its stack would consume scarce internal RAM,
so the allocation tradeoff needs investigation before changing it.

## Passed checks on the corrected image

The normal `pio run -e esp32s3` build and
`experiments/spotify-native/build-integrated.sh` both passed. The integrated
binary was 0x432530 bytes with 0x20dad0 bytes (33%) left in the smallest app
partition. The device's partition table matched the build, and valid OTA
sequence 1 selected app0. Only app0 was written; independent `verify-flash`
matched the corrected image hash above. NVS, LittleFS, bootloader, partition
table and OTA metadata were not written. The prior app0 image is saved in the
private recovery directory.

The exact successful capture commands were:

```sh
python3 tools/spotify_s3_capture.py --base-url http://192.168.11.199 \
  --output ~/.radiohead-recovery/spotify-s3-20260929/http-playing03.jsonl \
  --duration 180 --interval 5 --weather
python3 tools/spotify_s3_capture.py --base-url http://192.168.11.199 \
  --output ~/.radiohead-recovery/spotify-s3-20260929/http-controls04.jsonl \
  --duration 110 --interval 5 --weather --control-probes 20
```

The saved JSONL files, `flash-fixed.log` and `verify-fixed.log` are the durable
private evidence for these checks. The normal build used 96,404 bytes of static
RAM and 3,599,207 bytes of linked flash in PlatformIO's summary.

After the owner selected Radiohead from a computer, the radio played Spotify
and the owner reported **clear audio throughout**. The 180-second playing run
captured 36/36 successful samples of each HTTP endpoint with USB serial closed
and the TFT and weather workload active. All 36 Spotify samples showed output
owned, unpaused and unbuffered. Five-second PCM output ranged down to 863,232
bytes; empty polls and I2S write failures stayed at zero. The owner heard no
skips, distortion, dropouts or overlap. This is a three-minute listening and
counter check, not renewal or soak evidence.

| Playing run (36 samples) | Result |
| --- | --- |
| `/api/player` status | 36/36 HTTP 200; p95 38.05 ms; p99/max 348.43 ms |
| `/api/weather` status | 36/36 HTTP 200; p95 34.22 ms; max 57.54 ms |
| Spotify PCM and resource endpoints | 36/36 HTTP 200 each; resource p95 142.87 ms |
| Loop gap histogram during capture | p95 upper bound 5 ms; p99 upper bound 20 ms; 80,355 gaps |
| Minimum sampled free internal / DMA / PSRAM | 24,915 / 17,235 / 6,315,356 bytes |
| Minimum largest internal block | 14,336 bytes |
| Allocation failures and resets | 0 reported failures; no reset observed in the capture |

A separate 110-second playing run issued 20 same-volume player POSTs while
sampling the same endpoints. **All 20 controls returned HTTP 200**, p95
32.69 ms, p99/max 160.58 ms, within the preset 250 ms acknowledgement gate.
All 22 Spotify samples had active output; the minimum nonzero five-second PCM
count was 881,664 bytes, with zero empty polls and I2S write failures. Player,
weather, Spotify diagnostics and resource GETs all returned HTTP 200 in 22/22
samples. No reset or allocation failure was observed. This probes one existing
web command type at the current volume; it does not time source-selection or
all local controls.

One web `/next` command during Spotify playback released Spotify output and
emptied its PCM queue. Two seconds after the command completed, `/api/player`
reported local station NYOX as `playing`, with Spotify output unowned. Free
internal/DMA/PSRAM changed from 27,011 / 19,331 / 6,316,924 bytes before the
command to 36,567 / 28,887 / 6,347,708 bytes afterward. The HTTP call took
6.15 seconds including its redirect and page fetch; that is **not** a
first-audio or acknowledgement measurement. No listening result was received
for local NYOX, and the requested return to Spotify was not completed.

The user ended testing after these checks. The partial handoff capture was
stopped at nine samples and remains private as `http-handoff05.jsonl`; it
shows Spotify output unowned throughout and does not establish a reverse
handoff. The decoder's worst observed unused stack during playing was 4,744
of 32,768 bytes, and the queue's was 7,812 of 32,768 bytes. Both are below
the preset 25% reserve, so **S3 does not pass**. TLS renewal, network recovery,
ten-transition memory batches, source-to-first-audio timing, future allocation
bursts and the full stress duration also remain unverified.

The opt-in integrated image serves `GET /api/spotify/resources`. Heap arrays
are `[current free, minimum free since boot, largest free block]` in bytes for
internal 8-bit, internal DMA and PSRAM 8-bit capabilities. These capabilities
overlap and must not be summed. `stackBytes` is each named task's minimum
unused stack in **bytes** under the installed ESP-IDF FreeRTOS API; zero means
the task was absent or could not be found, not proof of stack exhaustion.
`loopGapBuckets` use upper bounds of 1, 2, 5, 10, 20, 50 and 100 ms, then
an unbounded final bucket. Counters are since boot. The endpoint does not
perform a heap-integrity walk or emit serial output during playback.

## Gates fixed before the run

Use the numerical S3 gates in [the plan](../spotify-native-validation.md):
zero allocation, stack, watchdog or I2S ownership failures; every measured
owned task has at least 1 KiB and 25% of allocated stack free; capability-specific
free and contiguous-block reserves cover the labeled remaining allocation
burst plus 25%; investigate ten-transition settled-batch loss above 4 KiB
internal, 32 KiB PSRAM or 10% of largest internal block, and fail any ongoing
decline; same-LAN control p95 at most 250 ms, status p95 at most 500 ms,
source-to-audio p95 at most 5 s; no unexplained steady PCM underruns or
audible dropouts. Do not change gates after a failed run without a measured
allocation/timing justification and a fresh run.

The known PCM allocation is 384 KiB PSRAM. Stack allocations for
`spotify_output` and `spotify_connect` are 8 KiB and 16 KiB respectively.
Record other task allocations and the largest pending TLS/decoder/web/UI
allocation before evaluating the capability-specific reserve gate. A zero
stack field for an upstream task leaves that task's coverage open.

## Capture

Build with `experiments/spotify-native/build-integrated.sh`, retaining its
printed app image hash and partition-table comparison. Follow the existing
private app0-only backup/flash procedure; do not erase NVS or LittleFS. Close
the USB serial reader during playback. Keep raw captures outside git:

```sh
python3 tools/spotify_s3_capture.py --base-url http://radio.local \
  --output ~/.radiohead-recovery/spotify-s3-run01.jsonl \
  --duration 3600 --interval 5 --weather --control-probes 20
```

The tool issues at most one sequential GET each to resources, Spotify PCM
diagnostics, player status and weather status per interval. It stores response
codes, timings and the two diagnostic JSON objects, not player/weather bodies
or signed Spotify URLs. It prints p95/p99/max request latency, minimum heap
figures, minimum named-task stack reserve and cumulative allocation failures.
With `--control-probes 20`, the first 20 intervals each also submit the
current volume and player revision to the existing control endpoint and time
the acknowledgement. This intentionally queues settings saves, so the option
is bounded and off by default. The captured player data contains only volume
and revision, not a station or track name.
Annotate the private run timeline with actual source transitions, Spotify
TLS/renewal, network recovery, audible quality and equal settling periods.
Repeat S2's directed and rapid transitions while the TFT, web and weather
remain active. The phone-controlled portion and listening checks require a
person at the device. Compare ten-transition endpoints after warm-up; the
first and last sample of an arbitrary run are not a leak test.

`/api/player` is a status GET, so its latency is not the control-acknowledgement
metric. Use the optional control POST probe, and pair source selection with
first audible/PCM output time. Capturing no allocation failures cannot establish
stack safety for unnamed upstream tasks or an unknown future allocation burst.
