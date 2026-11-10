# Web UI: current state and tests

2026-11-10 documentation cleanup: shortened the guides and replaced repeated logs
with these summaries. No firmware/CSS/assets/settings/device behavior changed;
no new hardware verification is claimed. Full older detail is in Git history.
The 49 Markdown files each shrink by at least 50% in words/bytes; local/incoming
links, headings, fences and scoped whitespace checks pass. No firmware build is needed.

## What exists

| Area | Implemented / still to check |
| --- | --- |
| W0/W1 design and shell | Local CSS/coast/markup; navy responsive pages compile; full firmware-served desktop/phone/accessibility review remains open |
| W2 shared player | Revisioned source/status/metadata, volume/mute; tone now in Audio; sustained web/TFT/audio synchronization needs device testing |
| W3 stations | Ten-slot CRUD/favorites/search/manual/test; saved row click plays; M3U removed and logo editor hidden by October scope |
| W4 saved artwork | Identity-safe RGB565 storage/rendering exists; reboot/interruption/full-storage acceptance remains open |
| Programs | Ten editable Omny/RSS slots, browser search and non-playing previews; [catalog tests/limits](../../programs-catalog.md) |
| Audio | −15…15 levels, optional frequency/range filter, revisioned Save/Cancel; host/native/browser fixtures passed, physical listening/persistence open |
| Spotify | Own tab, name/credentials, restart/pairing guide; configured read-only response observed; real provisioning/restart/full-page testing not established there |
| W5 network | Scan/dedup, hidden/open/security/draft, save→reboot/forget, setup AP; roaming persistence/control built, multi-interval device test open |
| W6 weather/time | Private-key entry/clear, C/F/visibility, friendly zones and DST; new default Jerusalem, explicit non-placeholder values retained; physical DST/weather checks open |
| W7 background | Not implemented. Appearance tab removed; do not expose mockup upload controls |
| W8 maintenance | Actual info, upload/release updater, restart/reset, touch diagnostics/calibration, recorder and crash summary; broad real OTA/reset testing remains open |
| W9 discovery | mDNS + branded connection/setup QR; actual camera scans/reachability across boot failures remain open |

Normal order: Stations, Programs, Audio, Spotify, Network, Weather & time, Device.
AP setup shows Network only. No removed feature should return because of an old plan.

## API reference for existing integrations

These summaries retain earlier reviewed contracts; reread current handlers before changing them.
Invalid input normally returns 400; stale revision/maintenance 409; failed persistence
500. Success means what the row says, not automatic playback/network success.

| API | Request / behavior |
| --- | --- |
| GET `/api/player` | Source/state/metadata/revision, volume 0–21, mute and tone; no secrets |
| POST `/api/player/volume` | `revision`, `volume`; apply now, coalesced save after 1 s idle |
| POST `/api/player/mute` | `revision`; retain volume and stream |
| POST `/api/player/tone` | `revision`, `bass/mid/treble` each −15…15; retained compatibility |
| GET `/api/network`; POST `/api/network/scan/start`; GET `/api/network/scan` | Async scan ≤20 visible SSIDs/security/RSSI, deduplicated; no passwords |
| POST `/api/network/connect`, `/retry`, `/forget` | Connect revision, exact SSID ≤32/password ≤63 and keep/replace/clear; 202 schedules reboot, not successful association |
| GET `/api/weather`; POST `/api/weather/save` | Revision, location ≤80, units/visibility/zone/format, key keep/replace/clear; response excludes key and reports refresh separately |
| GET `/api/device` | Build, connection, memory/storage/update/calibration and configured flags; never credential values |
| POST `/api/device/spotify-credentials` | `X-Radiohead-Config: 1`, revision, both `clientId/clientSecret` ≤128 non-space printable ASCII bytes; one checked `spotify/app_creds` blob, 200 durable save/restart needed |
| POST `/api/device/spotify-name` | Revisioned bounded UTF-8 name; existing keys/ID implications retained; applies after restart |
| POST `/api/device/ota?size=<bytes>` | Exactly one `firmware` multipart file, exact size within slot, 36-byte initial ESP32-S3 app-header check, then `Update.end()`; 202 only after validation, reboot follows |
| POST `/api/device/restart`, `/factory-reset` | Acknowledge before action; reset requires `confirm=RESET` |

[Programs](../../programs-catalog.md), [backup](../../settings-backup.md),
[boot screen](../../boot-modes.md), [touch](../touch-diagnosis.md),
[recorder](../../radio-stall-diagnostics.md), [network incident](../../radio-stall-investigation/network-followup.md)
and [crash](../../radio-crash-diagnostics.md) pages hold their own bounds/routes.

Spotify credential reads expose configured/revision only, `no-store`; missing new
blob may use legacy pair, malformed new blob must not resurrect it. Pairing remains.
Factory reset clears radio/Wi-Fi/stations/programs/favorites/artwork, retaining touch
and Spotify credentials/pairing. Backup restores all app namespaces, including those
retained by reset. Background reset behavior must be defined before W7 ships.

## Results worth retaining

| Date | Change / recorded proof / limit |
| --- | --- |
| Sep 21–25 | Shell, revisions, station/artwork, network/weather and release OTA built; isolated scripts/native checks. Initial package labels did not close device gates |
| Sep 22–Oct 11 OTA | Release status/TFT progress and compiled upload validate actual written bytes; download failure keeps current firmware, shows result 8 s, restores current page; worker progress sampled ≤4 Hz |
| Sep 29 | Source-aware Spotify shared player, escaped/bounded metadata, retained editing drafts; host polling tests passed, physical updates separate |
| Oct 7–8 | Private weather/Spotify key forms, own Spotify tab and name. Empty stored-key fields/flags verified read-only; never credentials echoed |
| Oct 9 | Initial scan, unique SSIDs, AP address display, scan ownership/roaming fixes, persisted boot artwork, QR startup. Host builds/checks, broad device acceptance open |
| Oct 11–12 | Removed Appearance/M3U/logo editor; 20 px Add gap. Browser touch recovery/stock ADC counters flashed; actual failure recovery/finger-pad reliability unresolved |
| Oct 13 | Encoder debug endpoint/read-only counters built; GPIO13 switch worked but rotation failure remained; [remap record](../../hardware/touch-spi-wiring.md) |
| Oct 21–22 Audio | Application PCM hooks explicitly linked, local EQ pre-gain compensated, Spotify normalization removed; shared limiter immediate attack/100 ms release for real overload; flat/cut-only bypass. Public ±15 levels/backend ±12 dB clamp retained |
| Oct 22 filter | In-band ranges strongest near center; zero-start high-pass, Nyquist-crossing low-pass, covering full spectrum silent. Sanitizer/routes/browser checks passed; synthetic 0–10000 attenuated 1 kHz ≥39 dB at 22.05–96 kHz, exact silence at 8/16/20 kHz; actual analog-noise cause unresolved |
| Oct 22 maintenance | Strict app upload/header/size, atomic maintenance through reboot, split transfer/validation, TFT callback progress; host failure/abort/cleanup/reconnect tests passed, not physical OTA acceptance |
| Oct 22–25 | Network-only AP navigation, editable Programs, API backup/restore, click-to-play saved station rows built/host checked; independent guides retain pending device cases |
| Nov 3–4 recorder | Numeric RTC rings/logging switch and first-network episode; real silence/recovery and software-restart archive observed; logging/persistence long trial open |
| Nov 4 panic | Controlled one-task capture, exact decode, retention/OTA preservation, clear/rearm and absent normal test routes verified on device; spontaneous capture/audio not established |

Audio single-notch Q=30; `notchOn/notchHz` retained, `notchEnd` added (0 single).
Old POSTs omitting upper end remain single mode; invalid saved range falls back.
Keep atomic PCM configuration, no hot-path allocation/logging/vendor edits.
An observed `100-9999` test reduced sound; `0-10000` on 44.1/48 kHz can retain
>10 kHz/transition content. This is not proof of a fault or a mute feature.

## Still to test on the actual radio

Inspect all pages at 320/390/768/1024/1440 px and 200% zoom with long Hebrew/Latin,
keyboard/focus/modal/labels and upload errors. Existing desktop/320 px screenshots
were mostly host fixtures with synthetic state, not pages served by the ESP32.

Exercise stale edits from two browsers/TFT; empty/full station/program catalogs;
search no-match/outage/AP mode; explicit test/save/remove; credential keep/replace/
clear; reboot/DST; offline saved artwork/full storage/interrupted replacement;
real OTA success/failure/progress/reconnect; reset scope/calibration retention.
Measure audio gaps, heap/largest blocks/stacks during search/upload/saving and a
mixed-use soak. Use current Spotify/TFT records for later November fixes; old
fixture failures or unverified claims must not be repeated as current pass results.

For each future change, add a short dated result: behavior, checks, evidence path,
what was actually flashed/observed and what remains. Skip duplicate build logs;
keep exact images/symbols in ignored build archives and raw incident files where linked.
