# S1 native Spotify candidate (standalone)

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
