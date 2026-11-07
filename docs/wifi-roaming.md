# Playback-aware Wi-Fi roaming — 2026-11-07

Implemented in `wifi_network` and `media`; device acceptance is pending.
The saved `wifiRoam` preference, its default, and existing HTTP routes are
unchanged. A radio with roaming disabled stays disabled after installing this
firmware. No firmware upload or device setting change was performed here.

## Policy

- Only scan after reception stays at or below -75 dBm for 30 seconds. The
  existing ten-minute scan cooldown and 10 dB improvement requirement remain.
  Recheck reception and media eligibility when the scan finishes.
- During saved-radio playback, require approximately 12 seconds of compressed
  audio actually buffered, computed from the library's bitrate estimate.
  Unknown bitrate, connecting/failed playback and inadequate reserve defer
  scans/switches. Stopped local playback permits roaming. Podcasts, unsaved
  station previews and Spotify defer voluntary roaming.
- Request an asynchronous disconnect, then yield to the loop until association
  and connected-IP status clear before calling `WiFi.begin`. This avoids the
  installed Arduino implementation's up-to-one-second disconnect wait inside
  `begin` when called while connected.
- Give the selected AP eight seconds to provide a connection/IP, then attempt
  an unpinned all-channel join. Restore the previous automatic-reconnect setting.
  Also watch subsequent disconnections from a successfully selected AP: its
  BSSID remains configured, so eight seconds of failure triggers the same
  unpinned fallback, even if voluntary roaming has since been disabled.
- A disconnect request that has not completed after 1.5 seconds is abandoned;
  a later disconnect is still watched so it cannot strand the radio. Failed
  unpinned join calls retain the fallback watchdog.

## Playback handover

The media module is notified when the deliberate disconnect has completed.
It leaves the audio buffer and playing state intact while Wi-Fi recovers.
Once connected, incoming buffer growth ends the handover guard without a
restart. Otherwise the radio retries its selected stream as the reserve falls
to half a second, or after 750 ms without PCM progress, bypassing the ordinary
recovery delay. Explicit source selection, stop and Spotify transfer cancel
this pending recovery. GPIO2's existing mute contract is unchanged.

This does **not** guarantee gapless AP switching: reconnecting a broken stream
still resets the decoder/buffer through the existing audio API and needs a
server response. Bitrate-based reserve duration is approximate. The audio
library also retains its own stream-loss recovery. No vendor patch, second
decoder, new buffer allocation or Wi-Fi event-callback network operation was
introduced. The intended improvements are to eliminate needless healthy-link
switches and remove avoidable recovery waits while retaining playable data.

Source basis: the pinned Arduino 3.3.11 `STA.cpp`/`WiFiSTA.cpp` and vendored
`Audio.cpp` were inspected locally. Espressif describes how disconnects affect
TCP connections in its [Wi-Fi driver documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-guides/wifi.html#wifi-event-sta-disconnected).

## Verification

Host checks passed:

- `python3 tools/check_wifi_roaming.py`: healthy reception, sustained weakness,
  buffer/source deferral, scan ownership, setting persistence, disable during
  scanning, asynchronous disconnect, join failure/fallback, later pinned AP
  loss, delayed disconnect, browser result expiry and timer rollover.
- `python3 tools/check_station_recovery.py`: existing recovery regressions plus
  reserve calculation, handover buffer drain, resumed input, stalled PCM,
  immediate retry, source cancellation and rollover; local and Spotify builds.
- Startup sequence, speaker mute, Programs and Audio-route checks. The
  Audio-route fixture needed the existing setup-mode flag/redirect mock added;
  its setup redirect is now asserted too. No production web code changed.
- `pio run -e esp32s3` and `git diff --check`.

Final build: 4,731,792-byte firmware, 1,821,808 bytes free in the application
slot; DIRAM 267,351 bytes (78.23%), 74,409 bytes remaining. These figures
include the pre-existing worktree changes; they are not a roaming-only delta.

Hardware acceptance still required: enable roaming explicitly after flashing;
listen on a strong AP for more than ten minutes and confirm no roam attempt;
then create sustained weak reception with a stronger same-SSID AP available,
capture `/api/radio/diagnostics`, and measure Wi-Fi and PCM recovery. Repeat
with the target AP unavailable, and with podcast/Spotify playback to confirm
voluntary roaming is deferred. Keep the USB serial monitor closed during
playback validation. Do not label the switch inaudible until listening and
diagnostic evidence establish it.
