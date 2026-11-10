# Web configuration

Open `http://radio.local` or the IP on the TFT. Current sections:
**Stations, Programs, Audio, Spotify, Network, Weather & time, Device & maintenance**.
Setup AP shows Network only. [Progress and outstanding checks](progress.md).
This page is the change guide; a planned feature is not automatically implemented.

## Use the existing design

Reuse [radiohead.css](radiohead.css), `#rh-config`, body class `radiohead-page` and
[coast.jpg](assets/coast.jpg). Desktop/phone [references](reference/desktop.png)
show navy/coastal/blue styling, not live data. Mockup scripts/results are simulated;
do not deploy them or restore the old orange/cyan palette. No external UI backend,
CDN fonts/scripts or runtime AI is needed. Keep core setup usable offline.

| Detail | Rule |
| --- | --- |
| Palette | Background #0b1321, panel #111d30, raised #17263c, line #29374b; text #f4f7fc, dim #a7b5ca, blue #3188f4, primary fill #1768ce |
| Desktop | Shell ≤1180 px, 197 px sidebar, 27 px main padding; list/form panels rather than extra dashboard tiles |
| Smaller screens | 780 px: 165 px sidebar/20 px padding; ≤560 px: wrapping nav/16 px padding; usable at 320 px |
| Text/input | Local/system sans; body 14 px/1.5, fields ≥16 px on touch, actions ≥44×44; Hebrew names `dir=auto`/`bdi`, URLs LTR |
| Photo/icons | Bundled 960×720 coast in banner, not behind inputs; local line icons, real artwork or initials |

Serve CSS/photo separately from flash; preserve CSS-relative asset mapping.
Do not inline large base64 images or concatenate styles per request. Inspect
response size, RAM/flash and audio-service gaps after packaging changes.

## Pages and behavior

**Stations:** ten slots; Add, edit, favorites, confirmed remove, saved-row click
plays with existing selection. Keep 20 px after Add. Explicit directory Search
and manual URL entry prepare editable drafts. Suggestions/health flags are not
device playback proof. Only **Test on radio** interrupts playback before explicit
selection; ordinary browsing/save does not. At capacity, disable Add but allow
edit/remove. A replacement slot must never inherit another station's artwork/favorite.
No M3U import and no logo editor/upload controls; keep showing existing logos.
[Discovery and stored-artwork rules](../station-discovery-and-artwork-plan.md).

**Programs:** editable ten-slot Omny/public RSS catalog, directory suggestions,
manual fallback, non-playing preview, confirmed removal and shared favorites.
[Limits/persistence](../../programs-catalog.md). Do not invent offline downloads.

**Audio:** volume 0–21 via existing curve; Bass/Mid/Treble **−15…15 levels**, not
mockup −10…10 dB. Save/Cancel/reload and stale-edit protection. Filter defaults off,
3000 Hz; one integer **100–10000** or an increasing range (zero allowed only at
range start), e.g. `5000-5100` / `0-10000`. In-band rolloff is strongest at center;
zero-start is high-pass, upper edge reaching Nyquist becomes low-pass, full-band
zero-start outputs digital silence. Radio/podcasts/Spotify share it. Analog noise
added after PCM may remain; a broad filter is not a mute control.

**Spotify:** own `/spotify` tab; Connect name and private app Client ID/Secret,
Save then explicit restart then select radio in Spotify's device picker. Explain
Premium/same-network and phone local-network permission. Link official
[Dashboard](https://developer.spotify.com/dashboard) and
[credential guide](https://developer.spotify.com/documentation/web-api/tutorials/getting-started).
A developer loopback Redirect URI is not a radio callback. Never request Spotify
account password or echo saved app credentials. Save confirms storage, not authentication.

**Network:** actual SSID/address/security/RSSI, async scan (≤20 visible results),
hidden-network input, password show for the draft only, connect/forget confirmation.
Selecting a scan result copies SSID; security/password remain explicit. Empty unchanged
password keeps stored value; Open network explicitly clears it. Connect saves then
restarts: browser disconnects; rejoin/reopen the displayed address. Failed next boot
uses setup AP. Never display a sample SSID/IP as actual. Background roaming is a
persisted switch; [policy/test](../../wifi-roaming.md).

**Weather & time:** location ≤80, C/F, Home visibility, supported provider, private
key keep/replace/clear, supported timezone and 12/24-hour clock. Empty key keeps old;
never return it. New/untouched placeholders use Asia/Jerusalem; explicit zones survive.
Only unambiguous supported city/country chooses its zone automatically. Save success
is separate from weather refresh; show stale/unavailable/unsynced honestly. No TFT
weather/time editor or fixed-offset substitute for DST.

**Device:** actual build/resources, browser diagnostics, touch calibration start
(tap corners on TFT), compiled app upload, release updates, confirmed restart/reset.
Upload `.pio/build/esp32s3/firmware.bin`, not factory/bootloader/merged images; keep
power on. Installation succeeds only after image validation/restart/version re-read,
not at 100% transfer. Reset requires literal `RESET`, erases radio/Wi-Fi/stations/
programs/favorites/artwork; retains Spotify credentials/pairing and touch calibration.
[Backup API](../../settings-backup.md) is available without browser controls.

## Shared rules

One real source-aware player strip appears on all pages: radio/podcast/Spotify,
connecting/playing/paused/muted/failed. No fake title, LIVE label for podcast,
clock/RSSI or success message. Mute preserves volume/stream; turning while muted
keeps mute. Volume writes coalesce rather than writing flash for each pixel.

Keep saved state separate from draft; explicit Save/Cancel; guard unsaved navigation.
Reject stale revision with current state and retain the draft for review. Late polls/
search responses cannot overwrite edits. Escape untrusted names/URLs, validate
schemes/bounds, and never inject provider SVG/HTML as markup. Secrets stay out of
responses, logs and browser persistence. Mark errors inline; use semantic labels,
keyboard focus and status announcements. Modal focus starts Cancel, stays inside,
Escape closes and restores trigger; check at 200% zoom and 320 px without overflow.

Retired: alarms, sleep timer, EQ presets, brightness/idle-clock/visualizer/language/
reorder/color editors. No Appearance tab. Background upload remains unimplemented;
W7 needs a new approved entry point/budget before activation. Standby/off/rename
are outside this package. [Controls specification](../configuration-and-controls-plan.md).

## For code changes

`web_server` owns pages/validation; `settings` persistence/assets; `media` playback;
`display`/controller reflect state; `device_control` power safeguards; startup stays
orchestration. Bound workers/requests, reject stale identities and serialize OTA,
calibration and other maintenance. Don't block the hot audio path or fetch images
synchronously inside `server.handleClient()`.

Update [progress](progress.md) before/after a package. W0 assets, W1 shell, W2 state,
W3 stations, W4 artwork, W5 network, W6 weather, W7 background, W8 maintenance,
W9 discovery/QR are labels for tracking, not proof of completion. After code changes,
build, inspect RAM/flash, run focused checks and diff check; inspect actual desktop/
phone pages and physical persistence/audio/OTA separately. Docs-only changes need no flash.
