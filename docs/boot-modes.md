# Persisted boot modes

Both screens are embedded in the same firmware image; the former
`RADIOHEAD_NEW_BOOT_SCREEN` build flag is retired.

| Stored ID | C++ value | Artwork | Progress bar Y |
| --- | --- | --- | --- |
| 0 | `BootMode::Original` | `docs/boot.png` | 168 |
| 1 | `BootMode::NewArtwork` | `docs/ui/boot screen/bootscreen.png` | 210 |

Mode 1 remains the default for missing, unreadable or unsupported stored values.
The selection is one unsigned byte at `radio/bootMode` in Preferences. Existing
settings remain intact; normal settings saves do not write this independently
owned key. Factory reset clears it with the other radio settings and restores
mode 1. Preferences survive ordinary firmware updates.

`loadSettings()` loads the mode before drawing the boot screen. `bootMode` is
active for this startup; `configuredBootMode` is the durable next-startup choice.
Call `saveBootMode(BootMode::Original)` or `saveBootMode(BootMode::NewArtwork)`
from the main/application context and check its boolean result. Validation or
storage failure leaves configured state unchanged. Saves do not restart the
device or change the active mode.

For software control over HTTP, use `radio.local` or replace it with the radio's
displayed IP address. Check the current and saved selections:

```sh
curl http://radio.local/api/device/boot-mode
```

Select the original screen:

```sh
curl -X POST http://radio.local/api/device/boot-mode \
  -H 'X-Radiohead-Config: 1' --data 'mode=0'
```

Select the new screen:

```sh
curl -X POST http://radio.local/api/device/boot-mode \
  -H 'X-Radiohead-Config: 1' --data 'mode=1'
```

After a successful save, explicitly restart to apply the selection. Playback
stops during reboot:

```sh
curl -X POST http://radio.local/api/device/restart
```

GET and successful POST return
`bootMode`, `configuredBootMode`, and `bootModeRestartRequired`; `/api/device`
includes the same fields. All mode responses are uncached. POST accepts exactly
`0` or `1` and returns 400 for invalid input, 403 for a missing configuration
header, 409 during maintenance or a scheduled restart, and 500 on storage failure.
There is no web/TFT selector yet.

The two `BootStyle` definitions in `src/boot_screen.cpp` independently own their
artwork, bar position, fill and trough colors, and progress duration. Mode 0 keeps
its cyan fill and 4.5-second animation; mode 1 uses a white fill and a 6.75-second
animation (50% longer). Mode 1 waits for that animation before closing even if
the audio ends early or fails to open. Both retain the blue trough, boot sound,
audio tail and maximum hold limit. Future second-mode branding can use the shared
active `bootMode`; a replacement name has not been selected.
Changing the mode after startup must not change branding until restart.

Wi-Fi joining starts alongside the boot artwork. Finishing the boot sound/bar
never waits for Wi-Fi or clears the display to black: the configuration QR screen
appears next, showing `Joining saved Wi-Fi` / `Connecting...` until an address is
available. The stable `http://radio.local` QR remains visible throughout joining;
HTTP/mDNS become available when connected. The QR screen retains its ten-second
hold, then waits only for any remaining part of the 15-second join budget measured
from the start of joining. A failed join switches to the existing setup AP and
its Wi-Fi QR; normal station playback cannot start in setup mode. Explicit setup
and boots with no credentials retain the existing immediate AP handoff.

`python3 tools/check_startup_sequence.py` exercises production startup, Wi-Fi
join/fallback and HTTP/mDNS lifecycle with timed hardware seams, including a
short boot that leaves join time after the QR hold. Both normal and Spotify
service-registration paths are tested. This does not measure physical Wi-Fi,
boot audio or display latency.

Physical acceptance still requires saving each mode, restarting, checking the
artwork/bar/audio, exercising a normal settings save and firmware update, and
confirming factory-reset behavior. Host checks/builds do not establish device
acceptance. No device flash is part of this change.
