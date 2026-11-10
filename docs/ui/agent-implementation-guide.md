# Changing the TFT UI

Read [AGENTS.md](../../AGENTS.md), the firmware skill, [controls](configuration-and-controls-plan.md),
[visual rules](visual-contract.md) and [status](implementation-status.md). Inspect
[uiconcept.png](uiconcept.png) and overlapping diffs. Work in small buildable changes;
keep pins/settings/audio behavior unless the requested change requires otherwise.
Older page-dependent encoder, alarm and timer plans are superseded.

## Current hardware and input

320×240 landscape ILI9341, XPT2046 on shared SPI2 at 1 MHz; N16R8 ESP32-S3.
Use [current wiring](../hardware/tft-wiring.md), not historical pin tables.
Encoder A/B/SW 4/7/13; backlight 1; audio BCK/DIN/LRCK 39/40/41; GPIO2 speaker mute.
GPIO35–37 are octal PSRAM. Calibration lives in `touch/version,data` and loads at boot;
held encoder requests Wi-Fi setup. Light sleep wakes from encoder/optional K0.

- Encoder always controls volume/mute/3 s sleep; touch owns navigation.
- Touch buttons activate on first valid target, once per press; release re-arms.
  If initial on-panel contact misses, later points may reach a target on the same
  page. Once dispatched, consume through release—even after navigation.
- External page change or dim-wake cancels eligibility. Tone buttons retain hold
  repetition. Tracks accept position taps; no swipe scrolling or drag scrubbing.
- Keep four quadrature ticks/detent, direction and task synchronization. `volatile`
  is not a replacement for locking a read/reset. No locks across drawing/network/flash.

## Layout and state

One geometry definition serves drawing and hit testing, half-open rectangles.
Use at least 44×44 targets; star and row actions are disjoint.

| Component | Working layout constraints |
| --- | --- |
| Shared header | 44 px high; Back/title/time/Wi-Fi regions cannot collide; title also accepts Back |
| Lists | Four 46 px rows with 2 px gaps below header; right Up/Down rail, controls at top/bottom |
| Home | Four square 74×74 destinations near x=3/83/163/243, y=163; weather left, date/clock right; Spotify player return above tiles |
| Live player | Artwork/metadata below header, three separated transport targets; volume feedback follows knob |
| Recorded/Spotify player | Source-specific status/metadata and only supported transport; no invented live seek/duration |
| Dialog | Cancel first, named action, separate 44 px buttons; modal owns input |

Treat geometry as constraints checked against current production layout, not an
instruction to revert later accepted refinements. See source/native fixtures for exact positions.
Volume stays 0–21 through `volCurve`; turning while muted stays muted. Feedback:
volume about 1.5 s, mute/sound-on 2.5 s. Rebuild the latest underlying screen when
an overlay closes; never restore stale text.

Controller owns a bounded Back stack (initial budget eight), page/list positions,
drafts, modal and overlay deadlines. Carry stable entity IDs and layout generations;
a filtered row number is not a station slot. Reconcile catalog edits by identity,
clamp missing selections, cancel stale capture and reject stale revisions.
Keep requested versus confirmed-playing IDs separate. HTTP/connect success is
not proof of decoded or audible playback.

## Ownership and background work

Use the [module map](../firmware-architecture.md): `device_control` samples input,
`ui_controller/ui_runtime` route semantic actions, `media` owns Audio/playback,
`settings` commits, `display` draws, `main` coordinates the loop. Screens
must not start HTTP/Preferences/Audio work directly.

Fetch metadata/weather in bounded jobs/results with generations. One metadata job
is the initial limit; copy inputs and discard replaced results. Workers never
mutate live Strings or draw. Moving a blocking call to a queue does not make its
execution nonblocking. Do not create a second caller of `audio.loop()` or Audio.
Keep `audio.loop()` and `server.handleClient()` frequent, without delay animations.

Finite input queues coalesce deltas/moves and preserve terminal events. Overflow
cancels capture and resynchronizes physical input rather than leaving a stuck control.
Limit event work each loop. Initial measurement targets: touch polls 10–20 ms,
feedback <100 ms, render slices 3–5 ms, clock/progress ≤1 Hz; measure actual worst
and percentile service gaps instead of claiming these targets are achieved.

## Rendering and assets

Keep LovyanGFX. Prepare native-size assets from `docs/bg1.png`; dynamic text,
favorites and controls are never baked into a screenshot. [Recipes](assets-manifest.md).
Layers: clean background → navy surface → artwork/text → selection → overlay.
Restore the old/new union and intersecting layers before repaint; repeated alpha
on already-composited pixels accumulates tint. Dirty-region overflow schedules a
full repaint, not a lost invalidation. Tie hits to layout generation during transitions.

Use the readable RGB565 PSRAM canvas; physical TFT readback is not a blend source.
Transfer in reusable stripes, service audio between slices and wait for DMA before
reusing buffers. Forty Home/list/transport/header alpha assets now use predecoded
flash RGBA runs to avoid unchecked PNG DMA allocation; preserve that path.
Opaque photos retain PNG. Low-memory fallback uses legible opaque primitives/bitmap text.
No runtime blur, network/image decode in rendering, per-frame allocation or giant stack buffers.

| RGB565 payload | Bytes | Ideal 16 MHz SPI time (not measured) |
| --- | ---: | ---: |
| 320×240 | 153600 | 76.8 ms |
| 320×8 stripe | 5120 | 2.56 ms |
| 88×88 artwork | 15488 | 7.744 ms |

Initial extra-UI budgets: ≤512 KiB PSRAM, ≤32 KiB internal buffers/state, one optional
150 KiB background and two 5 KiB stripes; stacks/library scratch count separately.
Measure free/largest internal blocks, PSRAM and stack watermarks. DMA descriptors
need suitable internal memory. Check actual clock/wiring before increasing SPI speed.
Two OTA slots are 6.25 MiB each; filesystem 3.375 MiB and NVS 20 KiB are not spare
application flash. Cosmetic work must not repartition the device.

## Text and media correctness

Validate bounded UTF-8 without splitting code points. Keep logical strings in
models; apply bidi only for display, preserving digits/punctuation. Raw reversal
or right alignment is not bidi. Bundle licensed Latin/Hebrew fonts, measure pixel
advances and clip/wrap before shrinking. Test `גלצ`, `אילנה דיין`, `Kan Bet`,
`פרק 15 – 15 בספטמבר 2025`, `12:34 / 28:17`, long mixed text, missing glyphs,
combining marks, malformed/empty input. Hebrew content does not imply a language selector.

Ten station/program slots and eight episodes require command-time bounds/identity
checks. Previous/Next skips empty station slots; browsing never retunes.
Favorites survive rename; replacement/removal clears old association.
Podcast pause/−15/+30 depend on real library/source capabilities: signed/clamped
seek, unknown duration `--:--`, honest failure. Never expose live-radio seek.
Downloads/resume are future: no SD/storage/quota manager exists; a 30-minute
128 kbit/s episode needs 28.8 MB. Do not consume OTA space or pretend it is saved.
Website/share is a validated phone handoff/scan-tested QR, never automatic messaging.

Every page handles loading, empty, offline, missing/long metadata, allocation failure
and external edits. OTA excludes conflicting power/storage work. Keep retained
Preferences/routes compatible; retired features stay inert per the controls spec.

## Packages and verification

| Package | Scope / remaining proof |
| --- | --- |
| P0 | Build + runtime resource/input/timing baseline |
| P1 | Bounded controller, navigation/input and conflict checks |
| P2 | Native background/artwork/Hebrew and reusable Home/list/player/volume/dialog compositions |
| P3 | Same compositions wired to real playback/web state; sustained physical checks |
| P4 | Identity-safe favorites/options/info |
| P5 | Bounded podcast fetch and supported streaming transport |
| P6 | Retained settings only; original alarm/timer/preset package is retired |
| P7 | Refinement/soak/OTA; not where core visual design starts |
| P8 | Optional downloads only after a storage decision |

After code/config edits: build, `git diff --check`, focused input/bounds/persistence/
UTF-8 checks, native production render fixtures at 320×240. Compare Home/list/live/
podcast/dialog, missing/Hebrew content and fallback paths; inspect the physical panel.
Start with a 60-minute mixed-use soak, recording audio gaps, repaint timing,
heap/largest block/PSRAM/stacks. Test slow network/stale jobs, touch edges/holds/wake,
web synchronization, reboot/settings, OTA recovery and sleep/wake. USB serial stays
closed for listening. Report visual, functional and hardware evidence separately;
leave P2/P3 open where physical/timing tests are missing. Never flash for docs-only work.
