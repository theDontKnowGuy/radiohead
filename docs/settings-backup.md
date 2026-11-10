# Back up and restore settings

Use a trusted local network. The unencrypted `.rhb` file contains Wi-Fi and Spotify
credentials: keep it private. Substitute the displayed IP if `radio.local` fails.
Works in normal and setup AP modes; no browser controls are provided.

```sh
umask 077
curl --fail --show-error -H 'X-Radiohead-Backup: 1' \
  http://radio.local/api/settings/backup -o radiohead-settings.rhb
```

Restore **replaces saved settings and restarts** the radio. Keep power connected:

```sh
curl --fail --show-error -H 'X-Radiohead-Backup: 1' \
  -F 'backup=@radiohead-settings.rhb;type=application/octet-stream' \
  http://radio.local/api/settings/restore
```

Success: HTTP 202, `{"restored":true,"restarting":true}`. Reconnect using the restored
Wi-Fi details. If the response is lost, check the radio before retrying.

## What is saved

All persisted values in `radio`, `wifi`, `favorites`, `touch`, and `spotify`:
stations/programs/favorites, audio, weather/clock, boot mode, Wi-Fi profiles,
Spotify credentials/pairing, and touch calibration. Wait one second after changing
coalesced settings before exporting. Unsaved state, artwork in LittleFS, firmware,
and ESP-IDF internal settings are excluded. Existing artwork is left in place.
Use an unedited file from compatible firmware, preferably from the same radio;
restoring another panel's calibration can misalign touch. Legacy keys stay inert.

## Failure and recovery

The required header prevents ordinary cross-site forms; it is not authentication.
Backup/restore are blocked during maintenance, restart, or calibration.
Exactly one multipart field named `backup` is accepted, up to 32 KiB.
The whole file is validated before writing; malformed/aborted uploads change nothing.
Write failures attempt rollback. Failed rollback returns 500 and restarts with an error.
Power loss during replacement can leave partial settings: retain the backup and retry
from setup mode. CRC32 detects damage, not tampering.

| HTTP | Meaning |
| --- | --- |
| 200 / 202 | Download / restore committed, restarting |
| 400 / 403 | Invalid file/request / wrong or missing header |
| 409 / 413 | Busy / over 32 KiB |
| 500 / 503 | Storage failure (read response for rollback result) / allocation failure |

## Format and checks

Version 1: magic `RHSET01\0`, little-endian 32-bit payload size and IEEE CRC32,
then records: namespace index, NVS type, key length, zero reserved byte,
32-bit value length, key and value. Strings include one trailing NUL.
Limits: 256 records, 15 printable ASCII key bytes, 16 KiB/value, 4 KiB/string.
Restore can allocate two 32 KiB buffers; both are wiped before release.
`settings_backup` owns storage; `web_server` owns HTTP/restart. Existing keys are unchanged.

Recorded checks: `tools/check_settings_backup.py` (ASan/UBSan),
`tools/check_firmware_upload.py`, firmware build and diff check passed on 2026-10-24.
Device export/restore, setup recovery, real flash errors and power interruption remain unverified.
