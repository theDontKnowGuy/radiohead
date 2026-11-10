# Collect evidence after a reboot

Keep USB serial closed: opening it resets this radio. Substitute its displayed IP:

```sh
python3 tools/collect_radio_crash.py http://radio.local --output .pio/next-crash.json
```

Use a new filename. The tool collects the summary and all three recorder banks,
then decodes addresses with the exact matching ELF. It does not erase evidence.
To save/decode durably and then explicitly clear/rearm:

```sh
python3 tools/collect_radio_crash.py http://radio.local --output .pio/crash-saved.json --clear
```

Automatic clear requires valid evidence, successful bank collection and full-hash
symbol matching. Invalid/torn dumps need a deliberate manual clear after preserving
what is available. GET and normal boot never clear a dump.

## What is retained

Normal firmware uses the existing **64 KiB coredump partition**: one faulting task,
no whole-DRAM capture, 2048 B panic stack, CRC32, no overwrite. Power loss, torn
flash, severe corruption, an oversized task or resets bypassing panic can defeat
capture. `captureArmed:false` means occupied: collect/clear before waiting for another.

`GET /api/radio/crash` returns cached startup validation: state, eight-hex `id`,
size/error, armed/test flags; valid dumps add full ELF SHA-256, task, panic category,
PC/cause/address and up to 16 backtrace PCs. Normal HTTP never serves raw memory,
assert/request text, credentials or SSID/URLs. Raw dumps can contain secrets.

`POST /api/radio/crash/clear` requires `X-Radiohead-Config: 1`, `confirm=CLEAR`
and exact current lowercase-hex `id`. Busy/stale/erase failure returns 409;
success 200. Clearing is flash maintenance and may briefly delay audio.

Successful builds archive immutable matching images/ELF, manifest, sdkconfig and
route/operation maps in `.pio/symbols/<full-ELF-sha256>/`. Keep these ignored local
files. Older short-hash dumps are reported but automatic decode/clear refuses them.
Build scripts enforce panic settings even with cached sdkconfig.

## Recorder breadcrumbs

Numeric loop-owned Web/Media breadcrumbs remain active with detailed logging off.
Web code = `routeId * 16 + HTTPMethod`; IDs are in `include/web_diagnostic_routes.h`.
They cover dispatch/response and multipart start; an active marker alone does not
prove the route caused the crash. Pre-dispatch parser work has only the Web phase.

Media: Playlist=1, Connect=2, AwaitPlayback=3, RetryWait=4. Results: playlist HTTP
status/error (0 direct skip, −1 setup failure), Connect immediate 1/0, AwaitPlayback
1 playing / −2 timeout / −3 canceled. No DNS/TCP/TLS breakdown inside vendor connect.
An 80 B checksummed extension gives store/archive/RTC SLOW sizes **6928/6932/6960 B**,
leaving 1232 B RTC. Readers accept old 6324/6852 B archives; absent extensions are null.
Export before downgrade. Core dump and latest crash bank can describe different
incidents: compare hashes/boot identity, not just names.

## Controlled bench test

```sh
python3 tools/build_firmware.py --crash-test
```

Artifacts: `.pio/build/crash-test/`; building does not trigger a crash.
Only this image exposes POST `/api/radio/crash/test` with config header,
`confirm=CRASH`, empty/armed dump and maintenance ownership. It mutes, acknowledges,
then faults in `radioCrashTestFault()`.

Only the bench image also permits GET `/api/radio/crash/test-partition`, requiring
`X-Radiohead-Backup: 1`, exact `id`, maintenance availability. Use
`tools/download_test_core.py <radio> --id <id> --output <ignored-private-path>`:
exclusive 0600 file, exactly 64 KiB, CRC-ID check/fsync, no clear. Never commit/print raw memory.

Recorded November 4 device test: one-task dump, matching source decode, GET retention,
occupied rejection, survival across normal-image OTA, clear/rearm and absent test
routes in normal firmware. Build and crash/radio/startup/artifact host checks passed.
Spontaneous-crash capture and audible continuity remain unverified. Restore the
normal image after bench tests; [stall evidence](radio-stall-diagnostics.md) is separate.
