# Wi-Fi roaming

Playback-aware roaming was implemented 2026-11-07; device testing is pending.
It preserves the saved `wifiRoam` setting: installing firmware does not enable it.

## How it works

- Scan only after RSSI stays at or below -75 dBm for 30 s, with a 10-minute
  cooldown. A candidate must improve reception by 10 dB; recheck when the scan ends.
- Saved-radio playback needs about 12 s of buffered compressed audio. Unknown
  bitrate, connecting/failed playback, podcasts, previews and Spotify defer roaming.
  Stopped local playback permits it.
- Disconnect asynchronously before joining the selected AP. Abandon an incomplete
  disconnect after 1.5 s, but watch for a late disconnect. Allow 8 s to connect/get IP,
  then retry without BSSID/channel pinning. Later loss of the pinned AP also triggers
  this fallback, even if roaming was subsequently disabled. Restore auto-reconnect.
- Keep buffered audio during handover. Buffer growth ends the guard. Retry the stream
  when reserve falls to 0.5 s or PCM stalls for 750 ms. Selection, stop or Spotify
  transfer cancels pending recovery; GPIO2 mute behavior is unchanged.

AP switching can still interrupt audio: buffer duration is estimated and a broken
TCP stream needs reconnection. No second decoder or extra audio buffer is allocated.

## Try it

After flashing, enable roaming explicitly. Listen on a strong AP for over ten minutes:
there should be no voluntary switch. Then create sustained weak reception with a
stronger same-SSID AP available. Capture `/api/radio/diagnostics`; repeat with the
new AP unavailable and with podcast/Spotify playback. Keep the serial monitor closed.

Recorded: `check_wifi_roaming.py`, `check_station_recovery.py`, startup/mute/Programs/
Audio checks, build and diff check passed. Combined-worktree image: 4,731,792 B;
DIRAM 267,351/341,760 B; app-slot free 1,821,808 B. No device flash/listening test
was part of that change; gapless switching has not been demonstrated.
