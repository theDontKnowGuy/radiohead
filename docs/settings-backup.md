# Settings backup and restore API

Added 2026-10-24 at the user's request. No browser UI is required.

Download all **persisted application settings** as an opaque `.rhb` file, then
upload that file to replace the saved settings. Both endpoints work in normal
station mode and setup AP mode. Use the radio's IP address if `radio.local` is
unavailable; the setup AP address is displayed on the device.

```sh
# Export. The file contains credentials; keep it private.
umask 077
curl --fail --show-error \
  -H 'X-Radiohead-Backup: 1' \
  http://radio.local/api/settings/backup \
  --output radiohead-settings.rhb

# Restore. This replaces saved settings and restarts the radio.
curl --fail --show-error \
  -H 'X-Radiohead-Backup: 1' \
  -F 'backup=@radiohead-settings.rhb;type=application/octet-stream' \
  http://radio.local/api/settings/restore
```

A successful restore returns HTTP 202 with
`{"restored":true,"restarting":true}` before rebooting. If the backup contains
different Wi-Fi credentials, reconnect on that network. A lost response does not
establish whether restore succeeded; inspect the radio after it reconnects.

## Coverage

The snapshot preserves every saved key and its NVS type in these namespaces:

| Namespace | Included settings |
| --- | --- |
| `radio` | Stations, selected station, volume/tone/notch, auto-dim, weather credentials/location, units/timezone/clock, auto-update, boot mode, Spotify name, legacy Wi-Fi mirror, saved program catalog and its favorites |
| `wifi` | All saved Wi-Fi profiles, passwords and active profile |
| `favorites` | Station favorites and legacy show favorites |
| `touch` | Saved touch calibration and version |
| `spotify` | App client ID/secret and saved Connect pairing |

Ignored legacy keys are preserved as data; restoring them does not enable retired
features. This is a snapshot of flash, not runtime state: after changing volume
or other coalesced settings, allow the existing one-second save debounce to finish
before exporting. Defaults never saved on the chip remain firmware defaults.

LittleFS station artwork, firmware images, bundled assets and ESP-IDF internal
namespaces are excluded. Existing artwork files remain on the destination;
the usual stream-URL identity check determines whether a logo still applies.
Unsaved runtime state, current playback sessions and transient caches are excluded.
Restore also replaces touch calibration; use a backup from the same radio unless
the destination has matching hardware and calibration. Use unedited backups from
compatible firmware; this is not a portable settings editor or a firmware downgrade
migration system. Normal startup validation/migrations still apply after restore.

## Request and failure behavior

- `GET /api/settings/backup`: binary download with `Content-Disposition:
  attachment`, `Cache-Control: no-store`, and `X-Content-Type-Options: nosniff`.
- `POST /api/settings/restore`: multipart upload with exactly one file field named
  `backup`, maximum 32,768 bytes. No filename extension requirement.
- Both require `X-Radiohead-Backup: 1`. No cross-origin permission is granted.
  The header blocks ordinary cross-site forms; it is **not authentication**.
  These use the existing local HTTP server. Backups are unencrypted and contain
  Wi-Fi/API/Spotify secrets; use a trusted network and store the file privately.
  `.rhb` files are ignored by Git. Neither endpoint logs or echoes settings values.
- Firmware maintenance, pending restart and touch calibration exclude backup and
  restore. Restore holds the updater's maintenance reservation through reboot.
- The entire file is received and structurally validated before any NVS erase or
  write. Validation checks format version, length, CRC32, namespace/type allowlist,
  key/value limits, scalar widths, string termination and duplicate keys.
  CRC32 detects accidental corruption; it is not a signature.
- Aborted uploads, malformed multipart requests, extra files and oversized data
  discard the temporary RAM buffer. No uploaded data is written to LittleFS.
- Before replacing settings, the firmware takes an in-memory snapshot of the old
  settings. A reported erase/write/commit failure triggers rollback. Success
  requires all writes and commits to succeed. If rollback also fails, the API
  returns HTTP 500 and restarts with an explicit incomplete-settings error.
- Replacement spans multiple namespaces and is **not power-loss atomic**. Keep
  power connected during restore. A reset/power loss after writes begin can leave
  mixed or partial settings; retain the file for retry using setup mode if needed.
- Success replies and reboots inside the request handler, before the application
  loop can service pending saves with old runtime values. Playback stops on reboot.

| HTTP status | Meaning |
| --- | --- |
| 200 | Complete backup download |
| 202 | Restore committed; restarting |
| 400 | Missing/extra file, incomplete upload, invalid/corrupt/unsupported backup; no imported settings written |
| 403 | Required header missing or incorrect |
| 409 | Conflicting maintenance or restart |
| 413 | Upload exceeds 32 KiB |
| 503 | Could not allocate upload buffer |
| 500 | Snapshot/storage failure; response distinguishes no writes, successful rollback, or failed rollback |

## Format and implementation

`settings_backup` owns persistence; `web_server` owns HTTP and maintenance/reboot
coordination. No existing Preferences keys or storage formats change.

Version 1 is little-endian: 8-byte magic `RHSET01\0`, a 32-bit payload byte count,
a 32-bit IEEE CRC32 of the payload, then records. Each record has namespace index
(`radio`, `wifi`, `favorites`, `touch`, `spotify`), NVS type, key length, reserved
zero byte, 32-bit value length, key bytes, and value bytes. Strings include their
single trailing NUL. Integers retain the ESP32-S3 little-endian representation.

Limits: 256 records, 15 printable ASCII key bytes, 16 KiB per value, and 4 KiB per
string including NUL. Unsupported stored data fails export rather than silently
omitting keys. Snapshot/upload buffers are bounded to 32 KiB each; restore needs
an upload buffer plus a rollback snapshot (64 KiB maximum payload allocation).
Buffers are wiped before release. No large stack buffers or unbounded JSON trees.

NVS enumeration/get/set/commit behavior was checked against the installed ESP-IDF
5.5.5 `nvs.h`, and multipart callback ordering against the installed Arduino
WebServer source. Host mocks test application logic, not flash-driver failure or
actual TCP behavior.

## Verification

- `python3 tools/check_settings_backup.py` passes production storage and extracted
  production HTTP handlers under ASan/UBSan, using synthetic values only. Covers
  every NVS scalar type, strings/blobs, all app namespaces, system-key exclusion,
  exact replacement, every truncation/single-byte corruption, invalid types/keys/
  namespaces/lengths/duplicates, injected apply failures and rollback failures,
  missing/wrong headers, maintenance conflicts, multipart extra/oversized/aborted
  files, complete-request commit gating, cache/download headers and reboot.
- `python3 tools/check_firmware_upload.py` passes the retained OTA path; the
  fixture isolates OTA callbacks from the existing Programs handlers and stubs
  the separately tested settings-upload cleanup.
- `pio run -e esp32s3` passes. Image: 4,683,392 bytes; 1,870,208 bytes of app-slot
  headroom. DIRAM: 261,803 / 341,760 bytes (76.60%), 79,957 bytes free. Compared
  with the preceding Programs build: image +6,496 bytes, static DIRAM unchanged.
- `git diff --check` passes. No vendored files, real backups or credentials added.
- Not flashed or verified on hardware. Pending: export/change/restore/reboot
  comparison including Wi-Fi, Spotify, programs/favorites and calibration;
  interrupted TCP uploads; setup-AP recovery; real NVS exhaustion and power loss;
  playback and heap headroom with the USB serial monitor closed.
