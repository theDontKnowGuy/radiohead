# Per-device screen orientation

The same firmware supports both landscape orientations. Each radio stores its
own choice in NVS. Read the active orientation:

```sh
curl http://radio.local/api/device/display-orientation
```

Flip upside down and save immediately on that radio:

```sh
curl -X POST http://radio.local/api/device/display-orientation \
  -H 'X-Radiohead-Config: 1' --data 'flipped=1'
```

Use `flipped=0` to restore the original view. Replace `radio.local` with the
specific radio's IP when configuring multiple devices. Both requests return
`{"flipped":true}` or `{"flipped":false}`. POST returns 200 after saving and
applying the choice; no restart is required. It sets an explicit state, so
retrying a request cannot toggle the screen back. No browser or TFT control
is added.

The full screen, including startup artwork, uses the saved orientation after
reboot, sleep/wake and OTA. The ILI9341 stays 320×240. LovyanGFX rotates touch
coordinates with the display; saved calibration remains unchanged. A held
touch is suppressed until stable release when the orientation changes.

POST errors: 400 missing/invalid `flipped` (only `0`/`1` accepted), 403 missing
configuration header, 409 maintenance/restart pending, 500 storage failure.
Storage failure leaves the active view unchanged. Responses use `no-store`.

Storage: byte `radio/displayFlip`, 0 original/1 flipped. Missing, invalid or
wrong-type values select original orientation. Normal settings saves and OTA
retain it; factory reset clears it with the existing `radio` namespace.
Settings backup/restore includes it with that namespace, so restoring another
radio's backup also restores that radio's orientation. Change it again through
this API if their mounting differs.

`python3 tools/check_display_orientation.py` exercises production storage,
startup, handlers and held-touch suppression with simulated NVS/TFT/requests.
Physical screen/touch alignment, reboot persistence and audio during rotation
still require testing on the device.
