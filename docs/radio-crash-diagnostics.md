# Persistent panic evidence

Implemented November 4, 2026 after the 12:37 panic. The ordinary radio build
captures a bounded ELF-format core dump into the existing 64 KiB `coredump`
partition. No partition offsets, saved settings or stream-retry policy change.

## After an unexpected reboot

Keep USB serial closed. From this checkout, run:

```sh
python3 tools/collect_radio_crash.py http://192.168.11.127 --output .pio/next-crash.json
```

The tool saves the selected crash summary and all three flight-recorder banks,
annotates web/media operations, and uses the exact matching ELF to resolve
addresses to functions and source lines. Output files must be new. Collection
does not erase or acknowledge the dump. Normal later boots also preserve it.

To collect, decode, durably save and then explicitly clear/rearm capture:

```sh
python3 tools/collect_radio_crash.py http://192.168.11.127 --output .pio/crash-before-clear.json --clear
```

Clearing requires valid crash evidence, successful bank collection and a matching
symbol decode. The file is flushed/fsynced before the clear POST. Clearing an
invalid/torn dump requires a deliberate manual POST with its ID after separately
preserving the available evidence. Never erase on a GET or automatically on boot.

## Capture and API contracts

- `GET /api/radio/crash`: cached startup validation result; `state`, partition
  fingerprint `id`, dump byte count, IDF error code, `captureArmed`, `testBuild`.
  A valid dump adds full crashing ELF SHA-256, task name, selected panic category,
  exception PC/cause/fault address, up to 16 backtrace PCs and corruption flag.
  Missing, unreadable, invalid and uninitialized states are explicit. Replies
  are bounded and `Cache-Control: no-store`; reads do not scan flash during playback.
- `POST /api/radio/crash/clear`: `X-Radiohead-Config: 1`, form `confirm=CLEAR`
  and exact eight-lowercase-hex `id` from the current summary. Maintenance/restart
  conflicts reject the request. A stale ID or erase failure returns 409 without
  reporting successful rearming; success returns 200. Erasure is an explicit
  flash-maintenance action and can temporarily delay playback servicing.
- The normal image serves no raw core dump, task stack, general register set, assert text, request
  contents, credentials, SSID or stream URL is served. Raw core memory may
  contain secrets. If later needed, read the partition explicitly over the local
  service connection into ignored private storage; never commit it or expose it
  through ordinary diagnostics. The temporary bench build has the guarded export
  described below for service connections that cannot use USB.
- ESP-IDF 5.5.5 always emits the faulting task first. Both task-iteration loops
  stop before extra tasks with `CONFIG_ESP_COREDUMP_MAX_TASKS_NUM=0`. Whole DRAM
  capture is off, a dedicated 2048-byte panic stack is reserved, CRC32 validation
  is on, and no-overwrite preserves the first occupied dump until cleared. A
  faulting task too large for 64 KiB, severe corruption, interrupted flash write,
  power loss or a reset bypassing the panic handler can still defeat capture.
  This is not a journal of every crash. `captureArmed:false` means collect and
  explicitly clear existing evidence before waiting for another capture.
- The default shortened IDF ELF hash is expanded to 64 characters. The build
  entry point enforces crash settings for existing sdkconfig files as well as
  new builds, so stale incremental configuration cannot silently disable capture.
  Older dumps may contain only a short hash; those are retained and reported
  without an invented full build identity, and automatic decode/clear refuses them.

Primary references: [ESP-IDF 5.5.5 core dumps](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-guides/core_dump.html),
and the pinned package's `espcoredump/Kconfig`, `core_dump_elf.c`,
`core_dump_flash.c` and public summary API.

## More precise flight-recorder breadcrumbs

The additive 80-byte checksummed `breadcrumbs` extension records Web and Media
operations plus the last completed Media operation. It remains active with
detailed playback logging off. Updates are loop-owned and numeric only; there
are no extra PCM-task prints, allocations, network calls or flash writes.

Web codes are `routeId * 16 + HTTPMethod`. Stable route IDs are in
`include/web_diagnostic_routes.h`; unknown paths have route ID zero. Middleware
covers dispatch/handler/response execution; multipart callbacks mark upload
start before middleware. The HTTP request parser before upload/dispatch remains
covered only by the top-level Web phase. Aborted uploads can leave the last
started route active, so an active marker alone does not prove a route crashed.

Media codes: Playlist=1, Connect=2, AwaitPlayback=3, RetryWait=4. Each records
start/end uptime, active flag, retry attempt and available numeric result.
Playlist result is HTTP status/error, zero for a direct-stream skip, or -1 for
failed HTTP setup. Connect is 1/0 for the library's immediate success/failure;
it is not proof of decoded audio. AwaitPlayback ends on observed Playing (1),
connection timeout (-2), or explicit source-change/stop cancellation (-3).
Completed duration uses unsigned 32-bit subtraction. DNS, TCP and TLS inside
the unmodified vendor `connecttohost()` remain one Connect operation.

Old 6,324-byte and 6,852-byte archives remain readable, with absent extensions
reported as null. New store size is 6,928 bytes, archive 6,932 bytes, RTC SLOW
6,960 bytes; 1,232 bytes remain. Old firmware cannot read the larger archives;
export before downgrading. Core-dump preservation and flight-recorder crash-bank
replacement are independent: compare firmware hashes/boot evidence, never assume
an older preserved dump and the latest crash bank refer to the same incident.

## Build symbols and controlled test

Each successful build automatically writes an immutable archive under
`.pio/symbols/<full-ELF-sha256>/`: matching firmware images/ELF, manifest,
effective sdkconfig, variant and checksummed route/operation maps. Interrupted
archives never become final; existing archives are validated and never replaced.
Keep this directory with the investigation. It is local ignored build output.

Build the isolated test image:

```sh
python3 tools/build_firmware.py --crash-test
```

Its default artifacts are `.pio/build/crash-test/`; ordinary builds explicitly
set the test option OFF. Only the test image registers
`POST /api/radio/crash/test`, requiring `X-Radiohead-Config: 1`,
`confirm=CRASH`, an empty/armed partition and available maintenance ownership.
It mutes speakers, acknowledges the request and enters `radioCrashTestFault()`.
The trigger is absent from the normal image; building the test does not run it.

Only the bench image also registers `GET /api/radio/crash/test-partition`,
requiring `X-Radiohead-Backup: 1`, the exact current `id` query parameter and
maintenance availability. It streams exactly the 64 KiB partition in 256-byte
chunks. This is private memory evidence, potentially including credentials:
save only into ignored local storage with permissions 0600, validate its size
and CRC fingerprint against the summary ID, and never print its memory contents.
This export is absent from normal builds and has no browser UI/CORS permission.
`tools/download_test_core.py <radio> --id <summary-id> --output <ignored-private-path>`
performs that explicit download with exclusive creation, mode 0600, bounded size,
CRC fingerprint verification and fsync; it never clears the device dump.

Bench acceptance: install the test image with the settings-preserving OTA flow,
verify its hash/test flag, invoke exactly one controlled crash, collect/decode
the report and verify the test source frame. Verify repeat GETs preserve its ID
and another test POST is rejected while occupied. Install the normal image,
verify the test dump survives and still decodes against its archived ELF, then
explicitly clear it. Confirm `captureArmed:true`, no test route, and normal
playback progress. Do not leave the test image installed for everyday listening.

## Verification status

The firmware build and `check_crash_diagnostics.py`, `check_crash_web.py`,
`check_radio_diagnostics.py`, `check_crash_tools.py`, startup and artifact checks
passed. The November 4 controlled device test verified a one-task dump,
matching-symbol decoding, read retention, occupied-dump rejection, preservation
across normal-image OTA, explicit clear/rearm and absent test routes in normal
firmware. This does not establish audible playback or spontaneous-failure
capture. Detailed incident captures are not retained in this checkout.
