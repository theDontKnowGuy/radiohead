# Choose the boot screen

Both images are built in. Use the displayed IP if `radio.local` fails.
Read the current and saved choices:

```sh
curl http://radio.local/api/device/boot-mode
```

Save `mode=0` for the original, or `mode=1` for the newer artwork, then restart:

```sh
curl -X POST http://radio.local/api/device/boot-mode \
  -H 'X-Radiohead-Config: 1' --data 'mode=1'
curl -X POST http://radio.local/api/device/restart
```

Restart stops playback. No TFT/browser selector exists. GET/POST return
`bootMode`, `configuredBootMode`, and `bootModeRestartRequired`.
POST errors: 400 invalid mode, 403 missing header, 409 busy/restarting, 500 save failed.

## For changes to the code

- Stored byte: `radio/bootMode`; active and next-boot choices are separate.
  Missing/invalid values and factory reset select mode 0. Normal saves and OTA retain it.
- Sources: mode 0 `docs/boot.png`; mode 1 `docs/ui/boot screen/bootscreen.png`.
  Older image variants remain beside it. Replacing an image needs a rebuild.
- Both bars use Y=189. Mode 0: cyan, 4.5 s, Radiohead branding.
  Mode 1: white, 6.75 s, `רדיו יהיה בסדר`. Web/Spotify naming is separate.
- Wi-Fi starts during the boot screen. The configuration QR stays for 7 s;
  joining has a total 15 s budget, then falls back to setup AP. No credentials
  or a held encoder switch starts AP setup directly.

`tools/check_startup_sequence.py` covers startup logic. Artwork was flashed
2026-10-13 with verified hashes; appearance, audio and reset behavior still need device checks.
