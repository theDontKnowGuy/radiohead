# TFT UI: current state

This summarizes the old iterative logs. [Controls](configuration-and-controls-plan.md)
and [visual rules](visual-contract.md) define the intended result; a code change,
native image and physical test are separate evidence. Full prior detail is in Git.

## Latest device result, November 9

Spotify icon and then Home tile alpha-PNG redraws crashed in an unchecked
LovyanGFX DMA allocation. Production now uses 40 allocation-free RGBA flash assets
(transport/header/Wi-Fi, Home, list surfaces). Heap-forbidden ASan/UBSan/clipping
checks and every-pixel comparison on four RGB565 backgrounds passed, as did full
native rendering. Home and Spotify 320×240 fixtures were inspected.

Installed/hash-verified **`fcc2828a57733e5445c8d6baecf41df6c36f454e0b36adfb4165a7b6d85798c5`**:
owner heard working Spotify/song changes, Mac pause/resume, Home navigation and
return to radio with debugger/serial closed; HTTP confirmed Playing without reset.
App 4,811,936 B, DIRAM 267,767 B. [Full failure/fix summary](../spotify-validation/stop-disconnect-2026-11-09.md).
This is a short functional check, not full visual/timing/soak completion.

## Working source behavior and recorded evidence

| Area | Behavior / evidence / remaining check |
| --- | --- |
| Home | Supplied coast, four lower square tiles, native clock/weather/branding and active-source summary; many host image checks; complete latest physical comparison remains open |
| Radio | Valid-slot list and independent favorites/options/info. Selecting a station/favorite returns Home, not Listening; radio Home summary is not a player-return target |
| Lists | Four 46 px rows, rounded navy/blue surfaces, right Up/Down rail; page-position and identity checks; comfortable touch reliability remains unresolved |
| Recorded programs | Omny/RSS mutable catalog skips deleted slots, keeps copied playing metadata and empty state; [focused native fixtures](../programs-catalog.md) pass, device catalog edits/reboot/favorites still to test |
| Spotify | Source transition opens player; Back/Home reopens without stopping; source-aware bounded metadata and transport, vector art placeholder; [Spotify UI details](spotify-ui-plan.md) |
| Settings | Audio, Display, Wi-Fi, Device; weather/time web-only. Tone preview/cancel/save and held repeat, explicit calibration; no retired alarm/timer/preset features |
| Shared header | Back chevron/title target `[0,210)×[0,44)`, clock/Wi-Fi/mute regions; supplied white Wi-Fi icon, no opaque photo-blocking bar |
| Volume | Centered 224×150 navy panel, 0–21, ~1.5 s; muted turns stay muted; low-memory fallback. Native variants pass; physical restoration/legibility open |
| Mute | Shared actual-state feedback, 2.5 s Muted/Sound on; Home/header indicators, volume turn replaces notice. Host input/native cases pass; broader physical check open |
| Touch | First valid target fires once; same-page initial miss can settle onto target; page change/wake consumes hold; tone repeat retained. Host production bridge tests pass, physical coordinates/light pad unresolved |
| Startup | Boot modes, Hebrew mode-1 branding, joining/connected/setup QR, saved calibration/default startup without tap wait; native layouts and simulated lifecycle pass, camera scans/physical startup matrix open |
| Firmware update | Real status/download/validation/result overlay, wakes screen, ≤4 Hz sampling, failure notice 8 s; native/focused checks pass, broad real update failures still to test |

## Short history, without abandoned pixel iterations

- September 19: first cream/olive functional slice was incorrectly treated as
  finished P2/P3. Reopened for concept fidelity, native Hebrew/artwork/background
  and physical measurements. Early photos rejected opaque cards and readback-based
  anti-aliasing; that trial was rolled back, then readable-memory-canvas typography added.
- September 19–23: native Home/list/player/favorites/program/settings layouts,
  Roboto/DejaVu mixed text, Inter clock/temperature atlas, coastal surfaces,
  header/navigation/QR/branding and focus/touch geometry refined. Host/native builds
  did not establish physical acceptance. [Recipes](assets-manifest.md), [Home notes](home-graphic-polish-plan.md).
- September 20–22: button response replaced swipe/release activation; right paging
  rail and four rows. Pressure-bypass/settling trials did not solve reliable light-pad
  use. The September 22 report reopens sensitivity regardless of earlier pass labels.
- September 29–30: Spotify snapshots/TFT/web and then transport added. Native
  [evidence folder](evidence/2026-09-29-spotify-ui/) retains playing/paused/long/missing states.
- October 9–13: header Wi-Fi/confirm spacing, QR startup, update feedback, Hebrew
  boot branding, broader Back title target, shared mute/centered volume panels.
  Relevant native fixtures inspected; source-only entries were not physical approval.
- October 22: program catalog views and missed-first-target retry; Spotify status
  moved up 4 px and metadata origins up 6/11/10 px for glyph bearings, fixing album
  overlap without changing cards/transport. Full native run then passed after the
  obsolete Spotify `photoHintCard` expectation was corrected.
- November 9: alpha compositor fixes above supersede earlier “not flashed” icon
  entries. Recorded playback/pause/Home/radio trial is device evidence; not a long soak.

Old Home `(299,190)`/Spotify `photoHintCard` assertion failures apply to the dated
runs; they must not be repeated as current full-suite failures after later passing runs.
Likewise, old prose saying the clean background is still missing is obsolete:
`docs/bg1.png` is supplied and used. Existing [evidence/](evidence/) images remain unchanged.

## Still to check

P0/P1 baseline runtime resources, actual TFT clock/service gaps and comprehensive
input/wake/navigation need measurements. P2/P3 have substantial concept/native code
and fixtures, but full physical visual fidelity, restoration/performance and sustained
media acceptance remain open. P4/P5 favorites/program behavior exists; do not call
P4–P8 entirely absent because the earliest slice table did. P6 follows the newer
retained-settings spec; downloads/background upload and separate power menu remain future.

Run production native checks for Home/list/live/recorded/Spotify/volume/dialog,
long Hebrew/Latin and missing/error states. Inspect 320×240 on the TFT and test
edges, held press across pages, dim-wake, rapid paging, web edits and fallback drawing.
Listen with serial closed while measuring repaint/audio service gaps, min free/largest
internal block, PSRAM and stack reserve over mixed-use soak. [Touch fault](touch-diagnosis.md)
and [white-screen fault](../hardware/tft-white-screen-diagnosis.md) remain separate.
Record a short dated update with actual build, checks/evidence and untested items;
no new physical acceptance was added by the November 10 documentation cleanup.
