# S1 native Spotify candidate (standalone)

## S2 integrated experiment

From the repository root, run
`experiments/spotify-native/build-integrated.sh`. It builds the normal
PlatformIO image to prepare the UI assets, prepares the pinned cspot/Bell
candidate under `.pio/spotify-candidate` if absent, builds the direct
ESP-IDF/Arduino image under `.pio/spotify-idf-build`, and compares its
partition table with the normal image. Set
`RADIOHEAD_SPOTIFY_CANDIDATE=/path/to/prepared/candidate` to reuse an
already prepared checkout. The build requires the installed PlatformIO
ESP-IDF 5.5.5 and Arduino 3.3.11 packages. The normal
`pio run -e esp32s3` image contains no Spotify adapter.

This image is experimental and has not passed S2 playback or transition
acceptance. It reads S1 Spotify credentials and pairing from NVS and must be
flashed app-only at `0x10000` if used for a device probe; do not erase or
replace the partition table, OTA metadata or LittleFS. Current device
evidence and resource concerns are in
[the validation ledger](../../docs/spotify-validation/progress.md).

This is a reproducible, isolated playback spike based on Waveshare commit
`9c51b087` and its pinned cspot/Bell submodules. It is **not** part of the
normal Radiohead build. `prepare.sh /path/to/empty-directory` fetches the exact
candidate and applies the patches here. Build with ESP-IDF 5.5.5. A successful
standalone build does not establish coexistence with Radiohead; that is S2.

The patches replace the Waveshare ES8311 output with 44.1 kHz, 16-bit stereo
I2S on BCK 15, WS 17 and DOUT 16 for the attached MAX98357A. They copy the
Radiohead partition offsets verbatim, remove the example's formatting SPIFFS
mount and NVS erase fallback, and read existing Wi-Fi settings from the
`radio` Preferences namespace. The first boot waits for a private USB serial
message with `SPOTIFY_CLIENT_ID=...` and `SPOTIFY_CLIENT_SECRET=...` lines,
then stores them in a separate `spotify` NVS namespace. Neither value is in
the source or image. A successful phone pairing is stored in that namespace
for restart. Signed CDN URLs, account names and keys are omitted from logs.
The I2S sink starts at a conservative volume and supports software volume.
The IDF 5.5.5 port supplies an explicit mbedTLS RNG, and the main, decoder
and queue tasks use correctly sized internal static stacks. Dynamic 32 KiB
stack allocations failed after TLS, while smaller stacks overflowed during
CDN work. The current diagnostic build emits low-rate heap and stack samples.
ESP-IDF's supported `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` option places transient
TLS allocations in PSRAM after an internal-memory exhaustion run. Espressif
recommends internal allocation for stronger physical security when external
RAM encryption is not enabled. This is an S1 experiment, not yet an accepted
Radiohead runtime setting; measure playback and renewal before adopting it.
The current experiment also disables hardware AES for mbedTLS because its DMA
path could not allocate a contiguous internal buffer during concurrent CDN
requests. Software AES performance and audio continuity require device proof.
The MAX98357A sink retries partial I2S writes with a bounded deadline and
reports aggregate PCM byte, partial-write and failure counters every five
seconds; a previous version dropped each partial write and flooded serial.
ESP-IDF 5.5.5's `i2s_channel_write` takes its timeout in milliseconds. The
sink passes 50 directly; passing `pdMS_TO_TICKS(50)` caused repeated short
writes and failed output in the first playback trial.
An initial real-track run with the corrected timeout still exhausted internal
RAM after a track change. The current candidate lowers
`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` from 16 KiB to 1 KiB to prefer PSRAM for
ordinary larger allocations. The allocation callback records the requested
capability of its latest failed allocation. This change requires device
resource, audio and lifecycle validation; it is not a stability claim.
The adapter also handles cspot's `DEPLETED` event after buffered PCM drains,
matching its CLI reference player's `notifyAudioEnded()` lifecycle. An
earlier phone run moved output to the phone before the next playlist song;
this handoff is still needed for a complete queue lifecycle, but a repeated
device run showed that it did not prevent the unexpected phone transfer.
The run-14 diagnostics located a missing output-boundary notification:
the standalone sink played three preloaded songs while the Connect queue
still pointed at the first. The current candidate records bounded PCM
track-boundary offsets and calls cspot's `notifyAudioReachedPlayback()`
when each boundary reaches the speaker, as its CLI reference does. This
advances the playlist index, refills the preload queue and reports the
current track to Connect. On run 15, the phone stayed on the radio when
Spotify was opened and a fourth song started naturally; run 16 passed
eight output boundaries. Added diagnostics print counts and queue
positions only, never track or account identifiers.
The final experimental build also returns the actual number of accepted PCM bytes to
cspot's decoder callback. Its existing retry loop then applies backpressure
when the output queue is full, including during a long pause. The earlier
standalone adapter returned all bytes as accepted and could discard PCM
after a two-second full-queue timeout. Run 16 passed a user listening
check after a longer pause and 25m47s of captured output. The user chose
to stop there; formal S1 acceptance remains open. See the
[evidence ledger](../../docs/spotify-validation/progress.md) for exact
measurements, limitations and remaining tests.

Short-run S1 continuation exposed a Wi-Fi recovery failure: after a router
interruption, the original candidate did not reassociate and the decoder
spun while parsing an incomplete HTTP response. The additional recovery
patches register an ESP-IDF station-disconnect handler, retry the same CDN
range on a new socket after a short outage, and stop parsing at socket EOF.
The first run with these changes regained IP and continued PCM, but repeated
interruptions exposed a session connection lifetime race and a device panic.
The run-19 patch snapshots the shared Shannon connection across send/receive
calls and replaces recursive reconnect attempts with a loop. Run 19 played
clearly before the Wi-Fi interruption, but a CDN range timeout led to an
uncaught TLS socket exception in `CDNAudioFile::openStream` and an abort.
After reboot the radio rejoined Wi-Fi, while Spotify moved playback to the
phone. This standalone iteration fails S1 Wi-Fi recovery and is retained for
diagnosis, not acceptance. See the ledger for measured results.

On this development Mac, after `prepare.sh /tmp/spotify-native`, build with
the installed ESP-IDF 5.5.5 package:

```sh
IDF_PATH="$HOME/.platformio/packages/framework-espidf" \
IDF_TOOLS_PATH="$HOME/.platformio" \
IDF_PYTHON_ENV_PATH="$HOME/.platformio/penv/.espidf-5.5.5" \
IDF_PYTHON_CHECK_CONSTRAINTS=no \
PATH="$HOME/.platformio/tools/tool-cmake/bin:$HOME/.platformio/tools/tool-ninja:$HOME/.platformio/tools/toolchain-xtensa-esp-elf/bin:$PATH" \
"$HOME/.platformio/penv/.espidf-5.5.5/bin/python" \
"$HOME/.platformio/packages/framework-espidf/tools/idf.py" \
-B build-radiohead build
```

The test device had app0 active. Its app-only upload command is
`python -m esptool --chip esp32s3 --port /dev/cu.usbmodem11201 write-flash
--flash-size 16MB 0x10000 build-radiohead/spotify-esp32.bin`. Confirm the
active slot on any other device before using this command. Do not run the
upstream `idf.py flash`: it also writes bootloader, partition table and OTA
metadata. The restore command for this radio is
`pio run -e esp32s3 -t upload --upload-port /dev/cu.usbmodem11201`.

The standalone candidate currently selects Ogg Vorbis 160 kbps with a 96 kbps
fallback. It does not advertise lossless playback. The phone and service may
choose a different file, so actual format and PCM settings must be taken from
device logs. The build has no TFT, radio, podcast, web configuration or weather;
it is only an S1 protocol/audio test. It must be replaced by a gated media
adapter, with measured resource and source-ownership tests, before S2.

Do not flash the upstream project unchanged. Its partition table differs from
Radiohead's, it formats a SPIFFS partition that overlaps Radiohead's LittleFS
artwork, and its credentials header embeds secrets in the firmware.
