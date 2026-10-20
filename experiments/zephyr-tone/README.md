# Independent Zephyr tone test — ESP32-S3 N16R8 / MAX98357A

This is a separate Zephyr application. Production firmware and the existing
`experiments/audio-only` ESP-IDF application are not dependencies and were not edited.
No device has been flashed or accessed. Playback and electrical behavior still need
on-device verification. The background evidence is
[the recording analysis](../../docs/hardware/audio-sequence-recording-analysis.md).

**Flash-layout incompatibility is real:** this build uses Zephyr Simple Boot at
`0x0`, not an ESP-IDF OTA application at `0x10000`. The current image occupies
199,048 bytes and erases sectors through `0x31000` (exclusive). That overlaps the
radio bootloader, partition table, NVS, OTA selection and the beginning of app0.
A verified full flash backup and an exact restore prefix are mandatory. Saved
settings are preserved in that local backup and restored byte for byte afterward;
**NVS is temporarily overwritten during this test.** If settings must remain in
place throughout, do not install this image. This is not an OTA-compatible image.
Do not use the radio's uploader, web OTA, `west flash`, or an erase-all command.

## Delivered files

Build/reference outputs are deliberately git-ignored under `artifacts/`:

- [Default right firmware](artifacts/right/zephyr.bin), plus ELF, map, resolved
  configuration, device tree, build log and [manifest](artifacts/right/manifest.json).
- [Left firmware](artifacts/left/zephyr.bin), with the same supporting evidence.
- [Right reference WAV](artifacts/reference-right.wav) and
  [left reference WAV](artifacts/reference-left.wav): 47 seconds, 44.1 kHz,
  16-bit stereo; [PCM checks and hashes](artifacts/pcm-checks.json).

The WAV is exported by host-compiling the **same `sequence.c` and PCM constants**
used in firmware. The build checker independently extracts all 18 PCM arrays
from each linked ELF and checks every byte against the generator. Reference WAVs
are not amplified or normalized after export. A right-only WAV can play through
only one computer speaker; preserve stereo routing and disable spatial effects
for a source comparison.

## Hardware and automatic run

Leave all wiring, amplifier SD/channel selection and gain connections unchanged.
The application's only pin routing is I2S0 TX: **BCLK GPIO39, WS/LRCLK GPIO41,
DOUT GPIO40 → amplifier DIN**. No MCLK pin, GPIO controls, cues, music, encoder,
display/backlight, Wi-Fi, Bluetooth, filesystem, settings or runtime logging.
Peripheral nodes for the board's default UART/SPI/I2C/touch and buttons are disabled.
Flash boot necessarily uses the module's existing flash interface; the ESP32 ROM
can emit its own reset messages before the application. No eFuse is changed.
PSRAM is unused/disabled; tables stay in flash, DMA buffers in internal SRAM.
The N8 board target is overridden to the actual **16 MiB flash**; no octal PSRAM
initialization or pin routing is enabled on this N16R8 module.

Times are relative to **first transmitted I2S frame**, not power-on. Boot/setup
adds an unmeasured offset. The first five seconds are transmitted digital zeros.

| Time | Signal | Combined active-slot sample peak |
| --- | --- | --- |
| 0–5 s | Silence | 0 |
| 5–10 s | 250 Hz | −30 dBFS |
| 10–12 s | Silence | 0 |
| 12–17 s | 1 kHz | −30 dBFS |
| 17–19 s | Silence | 0 |
| 19–24 s | 250 Hz + 1 kHz | −30 dBFS |
| 24–26 s | Silence | 0 |
| 26–31 s | 250 Hz | −24 dBFS |
| 31–33 s | Silence | 0 |
| 33–38 s | 1 kHz | −24 dBFS |
| 38–40 s | Silence | 0 |
| 40–45 s | 250 Hz + 1 kHz | −24 dBFS |
| 45 s onward | Silence, clocks and zero buffers continue | 0 |

Each tone includes 50 ms raised-cosine fades at both ends. Phase is indexed by
absolute sample position within the tone; it never resets at DMA boundaries.
The two sinusoids in each mix have equal amplitudes and their **sum** is peak
normalized. Integer sample quantization gives actual peaks 1,036 counts
(−30.00180 dBFS) and 2,068 counts (−23.99799 dBFS), a 6.00382 dB peak step.
Full scale means 32,768 counts. In a mix each component has about 5.7 dB less
amplitude than a single tone at the same combined peak. The other stereo slot
is exact zero throughout; amplifier SD wiring still determines which physical
speaker responds (or whether a module mixes slots).

The 47-second reference includes two seconds of final silence. Firmware keeps
sending zeros indefinitely. Reset/power-cycle replays the test; there are no
controls. Keep the serial monitor closed. Start the phone recording before reset,
then capture all six tones and final silence; record the flashed channel and
listening result. There is no software-controlled amplifier mute or shutdown.

## Pinned implementation and limits of independence

Verified with **Zephyr v4.2.1**, commit
`64e98eb5278d790f3b3e5279ca8b39b45097bbc0`, SDK **0.17.4**, Xtensa GCC **12.2.0**,
west **1.4.0**, CMake **3.31.6**, Ninja **1.13.0**, Python **3.11.7**, and esptool
**4.8.1**. The host dependency lock is in `tools/requirements-host.lock`.
The local setup uses only these two modules from the pinned Zephyr manifest:

| Module | Commit |
| --- | --- |
| hal_espressif | `f3453bdeced28642424692aae32cce4eec3f2d7f` |
| hal_xtensa | `b38620c7cc61e349e192ed86a54940a5cd0636b7` |

ESP32-S3 support was checked in the pinned
[SoC device tree](https://github.com/zephyrproject-rtos/zephyr/blob/64e98eb5278d790f3b3e5279ca8b39b45097bbc0/dts/xtensa/espressif/esp32s3/esp32s3_common.dtsi),
[native I2S driver](https://github.com/zephyrproject-rtos/zephyr/blob/64e98eb5278d790f3b3e5279ca8b39b45097bbc0/drivers/i2s/i2s_esp32.c),
[native GDMA driver](https://github.com/zephyrproject-rtos/zephyr/blob/64e98eb5278d790f3b3e5279ca8b39b45097bbc0/drivers/dma/dma_esp32_gdma.c),
and two successful builds targeting `esp32s3_devkitc/esp32s3/procpu`.

The application calls Zephyr `i2s_configure()`, `i2s_write()` and
`i2s_trigger()`. It transfers ownership of slab-allocated buffers to Zephyr;
it does **not** call or wrap `i2s_channel_write()` or install an ESP-IDF channel
driver. The linked map is checked for this distinction. It uses the Zephyr
scheduler, memory slabs, message queues, interrupt handling and GDMA driver.
Ten 3,528-byte buffers hold 882 stereo frames each (20 ms). Six blocks are
prefilled before a single START; there are no stop/reconfigure operations between
tones. The driver queue holds eight waiting blocks. Blocking waits allow DMA to
pace the producer. No software-timed GPIO waveform is used. On an API failure,
the application drops the stream, records a RAM fault code and sleeps indefinitely;
it never automatically retries/restarts the sequence. RAM counters count accepted
buffers, not proven physically transmitted frames.

**Requested framing is supported without a deliberate format deviation:**
Philips I2S, MSB first, signed 16-bit samples, two 16-bit slots, WS low for left,
WS high for right, one-bit delay, nominal LRCLK 44,100 Hz and BCLK 1,411,200 Hz.
The pinned driver's slot configuration sets 16-bit WS width, bit shift, stereo
slot mask, little-endian memory, and normal clock polarity. Source review and
host packing tests support the intended slot ordering; wire-level ordering and
actual clocks have not been measured. No application MCLK pin is routed. The
driver internally derives 256×Fs (11.2896 MHz), which does not require a wire.
Its default S3 source is PLL_F160M, as in the previous IDF default clock setup;
the HAL's fractional divisor can represent nominal 44.1 kHz exactly (14 + 76/441,
then /8 for BCLK). Oscillator accuracy/jitter remains unmeasured.

Independence is partial at the lowest level. `hal_espressif` contains Espressif
**ESP-IDF-derived 5.1.6** headers/HAL code, including
[`i2s_hal.c`](https://github.com/zephyrproject-rtos/hal_espressif/blob/f3453bdeced28642424692aae32cce4eec3f2d7f/components/hal/i2s_hal.c)
and S3 I2S/GDMA LL register helpers. Clock-tree, GPIO matrix/pinctrl, interrupt,
ROM/boot/cache and Xtensa support also have common Espressif ancestry. The old
experiment uses ESP-IDF 5.5.5's channel driver; this build changes that high-level
driver, OS, queue/DMA servicing and PCM source, while retaining I2S0/GDMA/PLL and
the physical path. Zephyr's driver reloads/restarts GDMA from its completion ISR
for each block. Host checks do not prove the FIFO never starves at those boundaries.
That remaining timing uncertainty must be assessed on the device/wire. No Zephyr
or HAL patches were made.

A clean result implicates something changed by this implementation; it does not
identify a particular old-driver bug. An unchanged result does not completely
clear shared HAL/LL code, clocking or the I2S peripheral. Missing/truncated tones
or added periodic gaps invalidate a simple acoustic comparison. The two-tone
frequencies are harmonically related, so harmonic and IMD lines overlap.

## Setup, build and checks

From the repository root on Apple Silicon macOS, with Python 3.11, `git`, `curl`,
`tar`, host C compiler and `dtc` installed (`brew install dtc` if needed):

```sh
python3.11 experiments/zephyr-tone/tools/setup.py
python3.11 experiments/zephyr-tone/tools/build.py
python3.11 experiments/zephyr-tone/tools/build.py --left
python3.11 experiments/zephyr-tone/tools/check_pcm.py
.pio/zephyr-tone-env/venv/bin/python experiments/zephyr-tone/tools/check_restore.py
git diff --check
```

The isolated venv, SDK and Zephyr checkout live in ignored `.pio/zephyr-tone-env/`.
Nothing is installed in the production build graph. Firmware needs no Wi-Fi/BT
blobs or network access at runtime. `setup.py` verifies exact source commits and
SDK archive SHA-256 values; `build.py` rejects modified dependency trees and pins
the toolchain through explicit environment paths. It never opens a device.
The right build sets `CONFIG_TONE_LEFT=n`; the alternate sets it to `y`.

The installer is verified only on Apple Silicon macOS. On another host use the
same Zephyr/module commits and the matching SDK 0.17.4 minimal bundle plus
`xtensa-espressif_esp32s3_zephyr-elf` toolchain; verify its upstream release checksums
and adapt the host-specific installer. Do not silently substitute an IDF toolchain
or newer Zephyr. A full `west update`/MCUboot install is unnecessary here.

Checks performed:

- Generated constants reproduce exactly; all compiled table bytes match in both ELFs.
- Every sample in the C-exported 47-second stereo stream matches a separate direct
  sine/envelope calculation; all silent gaps and inactive slots are exact zero.
- All frequencies are checked by coherent 1-second DFT lines and adjacent bins;
  peaks/6 dB step are within 0.005 dB, combined tones normalized, no clipping.
- 882-, 257- and 1,024-frame block traversals produce byte-identical full output
  on both channels. Steady periods repeat exactly; post-sequence buffers are zero.
- Resolved device-tree pins, TX DMA channel, disabled peripherals, internal SRAM,
  disabled logging/storage/network, linked driver provenance, image target,
  flash parameters, checksum and production CSV layout are checked.
- Synthetic backup tests reject size/chip/layout/partition-MD5/flags/hash errors
  and recover every byte, including the last erased sector's tail.

Both firmware builds report 199,048-byte images, IRAM region 35,596 bytes and
DRAM region 73,392 bytes (linker regions include their reservations; not a runtime
heap measurement). Firmware's configured flash capacity is 16 MiB. No production
build or artifact was replaced. These are host/build checks. No device playback,
clock, analog distortion, DMA-underrun or restore observation is claimed.

## Explicit backup, install and exact restore

These commands are **instructions for the user, not actions already performed**.
Use a stable USB connection, close all serial monitors and use the actual current
port. If auto-download fails, hold BOOT, tap RESET, release BOOT, and retry. Only
use this procedure on the same unencrypted, non-secure-boot ESP32-S3. Stop if
`get_security_info` reports secure boot or flash encryption enabled, or `flash_id`
does not show 16 MB. Do not use force/security-bypass options or modify eFuses.

Run from the repository root. The last known bridge port is shown; replace it if
necessary. Read-only detection/backup commands can reset the current firmware.

```sh
ZPY="$PWD/.pio/zephyr-tone-env/venv/bin/python"
ZTONE="$PWD/experiments/zephyr-tone"
ZPORT=/dev/cu.wchusbserial5C932096561
ZBACKUP="$ZTONE/backup/before-zephyr-$(date +%Y%m%d-%H%M%S)"
mkdir -m 700 -p "$ZBACKUP"

"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --after no_reset get_security_info
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --after no_reset flash_id
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --baud 460800 --after no_reset \
  read_flash 0x0 0x1000000 "$ZBACKUP/original-16mb.bin"
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --baud 460800 --after no_reset \
  verify_flash 0x0 "$ZBACKUP/original-16mb.bin"

"$ZPY" "$ZTONE/tools/prepare_restore.py" \
  --backup "$ZBACKUP/original-16mb.bin" \
  --image "$ZTONE/artifacts/right/zephyr.bin" \
  --out "$ZBACKUP/restore"
chmod 600 "$ZBACKUP/original-16mb.bin" "$ZBACKUP/restore/restore-prefix.bin"
```

**Stop after any failed command.** Confirm that the full-backup verification
succeeded and the offline plan accepted the actual partition table/MD5. The
backup and prefix contain private Wi-Fi/settings data: keep them local and do
not attach them to chat, commit them or replace them with a fresh firmware build.
`restore-plan.json` contains hashes only. Its `device_readback_verified: false`
means the offline tool cannot attest to the preceding live verification.
Keep the full snapshot and the restore prefix until the normal radio is restored.

For the default right test, after the successful checks above:

```sh
"$ZPY" -m esptool --chip esp32s3 image_info --version 2 "$ZTONE/artifacts/right/zephyr.bin"
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --baud 460800 --after hard_reset \
  write_flash --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 "$ZTONE/artifacts/right/zephyr.bin"
```

This writes just the image's affected sectors, **not** erase-all. Current supplied
images both need `0x0..0x31000`. The exact prefix tool rounds up to sectors and
preserves the original trailing bytes erased beyond EOF. The test starts after
the final reset. Close the uploader; leave the serial monitor closed. It sends
five seconds of silence, the six tones, then silence. Reset once the phone is
recording if the first run was missed.

To test left instead, prepare a fresh plan directory with
`--image "$ZTONE/artifacts/left/zephyr.bin"`, using the **original pre-Zephyr full
snapshot**, then substitute `left` in the image/write commands. Never take a
Zephyr snapshot as the restore source. Reusing the original snapshot for the
other channel is safe only on the same device without other writes; both
supplied builds have the same affected sector span.

To restore the exact firmware/boot selection/settings that were installed
before this test, use that run's original restore prefix:

```sh
# Re-establish these variables if this is a new terminal; keep the ORIGINAL backup path.
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --baud 460800 --after no_reset \
  write_flash --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 "$ZBACKUP/restore/restore-prefix.bin"
"$ZPY" -m esptool --chip esp32s3 --port "$ZPORT" --baud 460800 --after no_reset \
  verify_flash 0x0 "$ZBACKUP/original-16mb.bin"
```

After full-flash verification succeeds, press RESET or power-cycle to boot the
restored radio. Verification occurs **before** the normal radio can update its
own settings. This restores original NVS, OTA data, partition table, bootloader
and every changed app0 byte; app1/filesystem/rest of flash were never touched.
If verification fails, remain in download mode and recover from the original full
snapshot; do not boot the radio against partial settings. A full recovery write,
if needed, uses `write_flash` with the same `keep` options at `0x0` and
`original-16mb.bin`, then full verification before reset—still no erase-all.
Actual backup contents, security state and restore success can only be verified
when the user performs these steps. No such device claims are made here.
