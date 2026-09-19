# Touch UI implementation status

## 2026-09-19 — Light-finger XPT2046 acceptance pass

The application now uses a narrow XPT2046 wrapper that accepts two mutually
consistent valid-coordinate samples rather than requiring the installed
driver's pressure calculation to become nonzero. This addresses the observed
case where a pencil is accepted but a normal finger requires excessive pressure,
while still rejecting inconsistent floating-bus samples. It does not alter the
vendored LovyanGFX library, touch calibration data, or display SPI wiring.

`tools/render_ui_fonts.py`, `git diff --check`, and `pio run -e esp32s3` pass
with **65,484 B static RAM (20.0%)** and **2,723,031 B flash (41.6%)**.
**Status: complete (user-directed).** Reopen this slice if device testing later
shows no-touch false activations or remaining station-list swipe problems.

## 2026-09-19 — Live Stations header alignment and light-touch input pass

The Live Stations back arrow, title, clock and Wi-Fi indicator now share the
same 22 px visual centre, retaining the transparent photo header. The native
fixture was regenerated and visually checked at native resolution.

Touch sampling now runs every 8 ms, permits 16 px of normal finger movement
before rejecting a tap, and waits 36 ms before treating a missing XPT2046 sample
as a release. This lets a light contact survive a brief raw-sample dropout and
allows the existing vertical-swipe gesture to complete. The installed XPT2046
driver has no exposed configurable pressure threshold, so a panel that reports
no raw contact for a finger still needs physical diagnosis/calibration.

`tools/render_ui_fonts.py`, `git diff --check`, and `pio run -e esp32s3` pass
with **65,484 B static RAM (20.0%)** and **2,723,115 B flash (41.6%)**. The
firmware has not been flashed; finger touch and station-list scrolling remain
target-device acceptance checks.

## 2026-09-19 — Live Stations composition and swipe source pass

The Live Stations page now follows reference panel 2 more closely: the sunset
background reaches the top edge, the former opaque header bar is gone, and the
header uses the Home Wi-Fi asset. Five 39 px station rows fit beneath it, with a
blue selected row, photo-visible unselected rows, independent right-hand stars,
and no footer buttons. The row subtitle is the actual active stream title only
for its confirmed playing station; all other rows say `Live radio` rather than
inventing programme data or station facts.

Touch now distinguishes release-inside taps from vertical swipes of at least
28 px. On Live Stations only, a vertical swipe scrolls one row without tuning;
horizontal/diagonal movement and out-of-bounds gestures are ignored. Encoder
navigation still reaches every row and its star as separate actions. The minute
update rebuilds this page instead of restoring the old black header strip.

The production native fixture, including five-row hit geometry, passes through
`tools/render_ui_fonts.py`; its current result is reproducible at
`.pio/ui_native/stations-smooth.ppm`. `git diff --check` passes. `pio run -e
esp32s3` passes with **65,484 B static RAM (20.0%)** and **2,723,251 B flash
(41.6%)**. This was not flashed: touch calibration, swipe direction/threshold,
five-row legibility, transparent-header contrast and sustained-audio behavior
still require target-device verification.

## 2026-09-19 — P4 favorites and station information source slice

Implemented the current guide's P4 scope for station favorites, not the
superseded interaction plan's combined podcast/settings milestone. Favorites use
a versioned `favorites` Preferences namespace containing a bounded ten-bit slot
mask. The identity is the persistent station slot, never a filtered list row:
renaming preserves a favorite, while replacing a URL or importing into a slot
clears it. Factory reset clears that namespace but retains touch calibration.

The concept renderer now has station options (panel 4), Favorites with Stations
and Shows tabs (panel 9), and Station Information (panel 12). Stars and row
bodies have separate touch targets and separate encoder focus positions. The
Favorites station row plays only on its body; its star removes the favorite.
The web catalog replacement paths clear affected favorites, and the bounded
`/favorite?station=N` hook uses the same validation and persistence path.

No station facts, website URL, share target, or phone handoff were invented:
information presents the saved station name plus an explicit unavailable
provenance fallback. Show favorites remain empty until the P5 podcast identity
work exists. These capability gaps, missing device verification, and P3's open
hardware acceptance mean **P4 is not complete**.

The production native renderer generated and was visually inspected at native
size in `.pio/ui_native/favorites.ppm`, `station-options.ppm`, and
`station-info.ppm`; its hit-map assertions now cover independent list/favorite
targets and Favorites tabs. `tools/render_ui_fonts.py` and `git diff --check`
pass. `pio run -e esp32s3` passes with **65,476 B static RAM (20.0%)** and
**2,722,699 B flash (41.5%)**. The device was not flashed: favorite persistence
across real reboot, touch edges, encoder direction/focus legibility, display
timing and sustained audio still need physical testing.

## 2026-09-19 — P3 playback-state and live-player integration

The concept-based live player now receives an explicit bounded playback snapshot:
**Stopped**, **Connecting**, **Playing** (only after the installed audio library
reports `stream ready`), or **Failed**. Requested and confirmed-playing station
slots are distinct, so the Stations list does not label a pending or failed tune
as live. A connection that does not become ready within 12 seconds becomes an
honest unavailable state.

Player touch targets now implement Back, artwork/options handoff, valid-station
previous/next, Stop/Rejoin, mute and direct 0…21 volume selection; the temporary
volume overlay remains nonblocking. The encoder's short press enters visible
player control focus, starting at Stop/Play; rotation then chooses a control,
press activates it, and hold returns Home. Normal player rotation still changes
the canonical volume setting. Web previous/next now follows the same nonempty
station traversal, and web station edits invalidate the TFT. Destinations whose
packages are still pending remain explicitly unavailable rather than silently
routing to stations.

`pio run -e esp32s3` passes with **65,460 B static RAM (20.0%)** and
**2,716,551 B flash (41.5%)**. `tools/render_ui_fonts.py` passes its production
native renderer checks, including the player transport and volume hit geometry;
`git diff --check` passes. No device was flashed for this change. Sustained audio,
touch-edge behavior, control-focus legibility, stream-ready/failure timing and
web-to-TFT updates therefore remain hardware acceptance work; P3 is not marked
complete.

## 2026-09-19 — Hebrew/mixed-text Phase 2 completion pass

The production renderer now applies a bounded visual-order adapter before it
measures, ellipsizes, aligns or draws text. It handles the UI's Hebrew and
Latin/digit runs without reversing UTF-8 bytes, keeps combining marks with their
base glyph, preserves embedded Latin/numeric run order, right-aligns RTL fields,
and puts an RTL ellipsis on the logical tail rather than hiding the beginning of
a title. The adapter accepts at most 160 grapheme-like clusters per field, so a
remote title cannot make the display work unbounded.

The native fixture uses a Hebrew/mixed station (`תחנה 101 FM`) and long metadata
(`פרק 15 - 15 בספטמבר 2025`). The production C++/LovyanGFX path passed its
ordering assertions and was inspected at native size: [Stations](evidence/2026-09-19-bidi/stations-mixed.png)
and [Live Player](evidence/2026-09-19-bidi/player-mixed.png). `tools/render_ui_fonts.py`
also rechecks font coverage, target geometry, restoration and RGB565 transfer.

`pio run -e esp32s3` passes with **65,420 B static RAM (20.0%)** and
**2,712,667 B flash (41.4%)**; RAM is unchanged and flash increased by 1,524 B
from the prior Phase 2 build. `git diff --check` passes. This is a Hebrew
adapter for the bundled Hebrew/Latin assets, not a claim of full Unicode bidi or
niqqud coverage.

**Visual:** native fixtures inspected. **Functional:** compiler and native
renderer checks pass. **Hardware:** the target device was not connected, so the
required TFT legibility, repaint timing and sustained-audio observations remain
unverified. Per the visual contract, those device checks are the only remaining
Phase 2 acceptance gate; P3 must not be treated as accepted until they pass.

## 2026-09-19 — Bold clock and aligned Home header

The clock now uses **Roboto Bold at 34 px**, retaining its size and position.
Home title/date use middle-left/middle-right text anchors around y=22; the date
has a 1 px optical correction for its smaller face. The Wi-Fi asset is positioned
by its visible pixel center, rather than its transparent canvas bounds. The
bitmap fallback icon follows the same header center.

[Native Home evidence](evidence/2026-09-19-header/home-smooth.png) was inspected.
`tools/render_ui_fonts.py` and `git diff --check` pass. The firmware build passes:
65,420 B static RAM (20.0%), 2,711,119 B flash (41.4%). Runtime buffers are unchanged.
This revision was not flashed; physical confirmation remains pending.

## 2026-09-19 — Home fidelity pass: all six requested refinements

Implemented the six changes discussed with the user, retaining the second-pass
Roboto typography and the readable PSRAM composition path:

1. Clock moved from y=88 to **y=40**, directly below the date, using the already
   refined 34 px medium face. Its return-to-player hit region moved with it.
2. Home has a separate **320 × 240 toned background**: modest desaturation and
   a smooth navy veil. The supplied source and other pages' background stay intact.
3. Regular temperature digits, medium tile labels, measured word spacing and
   larger white weather captions remain in use; weather text aligns at x=90.
4. Home uses prepared anti-aliased icons: **filled heart**, rounded radio strokes,
   a proper eight-tooth gear, balanced list marks, thicker Wi-Fi arcs and weather
   artwork for clear/cloud/rain/storm/snow/obscured/unknown conditions.
5. Tiles are **70 × 70 at y=154**, with restrained vertical gradients, soft edges,
   centered 36 × 36 icon canvases and an anti-aliased focus outline. Touch targets
   follow the taller tiles; the gaps remain noninteractive.
6. Header reads **Internet Radio**. The city display strips a comma-separated
   country suffix without changing the saved/query setting. Weather text derives
   from the existing primary condition ID, using concise family labels (including
   Partly cloudy for 801/802) rather than the fixed "Current weather" placeholder.
   Mapping was checked against the provider's
   [condition-code documentation](https://old.openweathermap.org/weather-conditions).

A small station/play link remains beneath the clock. This deliberate addition to
the target preserves the existing return-to-player action after replacing the
station-name header. Its 110 × 70 target is separate from weather text and tiles.
No playback, settings key, network-fetch, route or controller-action changes were
needed. The no-PSRAM path retains legible bitmap fonts and native primitives;
PNG alpha compositing is used only in the readable memory canvas.

### Native evidence and checks

- [Home](evidence/2026-09-19-home/home-smooth.png),
  [long city/negative temperature](evidence/2026-09-19-home/home-long.png),
  [104°F](evidence/2026-09-19-home/home-fahrenheit.png), and
  [unavailable data](evidence/2026-09-19-home/home-unavailable.png).
- Weather examples: [clear](evidence/2026-09-19-home/home-weather-800.png),
  [overcast](evidence/2026-09-19-home/home-weather-804.png),
  [rain](evidence/2026-09-19-home/home-weather-500.png),
  [storm](evidence/2026-09-19-home/home-weather-211.png),
  [snow](evidence/2026-09-19-home/home-weather-601.png),
  [fog](evidence/2026-09-19-home/home-weather-741.png), and
  [unknown](evidence/2026-09-19-home/home-weather-999.png).
- The native harness now uses the real controller state/types and production
  `uiHitTest`, checking the new player region, tile centers/edges and gaps. It
  also checks concise city/condition output, typography bounds, alpha/color
  rendering and exact restoration. `tools/render_ui_fonts.py` passes.
- `pio run -e esp32s3` passes: **65,420 B static RAM (20.0%)**, **2,711,143 B flash
  (41.4%)**. Static RAM is unchanged; flash grows by 165,448 B versus the Roboto
  pass. The existing 153,600 B PSRAM canvas and font tables are unchanged. The
  prepared PNGs total 162,836 B; decoder allocations/timing need device measurement.
  Audio is serviced after the background, between Home tiles and between transfer
  stripes. No runtime blur, asset scaling, asset download or new task was added.
- `git diff --check` passes. No vendored audio changes or firmware binaries were
  added to the source tree; build outputs remain under `.pio`.

**Visual:** native Home inspected against target; new device photo pending.
**Functional:** build and native checks pass; device touch/encoder test pending.
**Hardware:** not flashed in this task; repaint timing and sustained playback
still need observation. These six source refinements do not close P2/P3.

## 2026-09-19 — Second typography pass: Roboto, optical weight and spacing

The user supplied a photo of the memory-canvas revision, confirmed improvement,
and confirmed that white looks correct on the physical display. The remaining
request is closer typography to the target. This is evidence for the preceding
revision, not hardware acceptance of this new font set.

Comparison of DejaVu, Roboto, Noto Sans and Lato specimens favored Roboto as a
closer visual match: narrower letterforms, lighter temperature digits, and
medium-weight small labels. This is a visual judgment, not identification of
the exact typeface used to create the reference image. No live Jev judgment was
used; font generation, metrics and validation are deterministic.

The production set now uses Roboto Regular/Medium, with these changes:

- Home tile labels: 11 px regular → **13 px medium**. City/weather captions:
  11 px → **13 px regular**, with white weather text for legibility.
- Temperature: 32 px bold → **32 px regular**. Clock: 40 px regular →
  **34 px medium**. Other titles retain 22 px but use medium weight.
- Word spaces now use the source font's measured advance. VLW's default
  line-height-derived space was too wide for the new family (for example, the
  13 px label needs a 3 px space, not the loader's 4 px guess).
- The Hebrew alphabet retains DejaVu glyphs as a fallback. Bidi, niqqud and
  broader Unicode layout are still open; this pass does not claim to solve them.
- Layout, backgrounds, icons, hit regions and the PSRAM composition/transfer
  path remain as before, apart from a 1 px caption alignment adjustment.

Compare the native [previous Home](evidence/2026-09-19-fonts/home-smooth.png) and
[new Home](evidence/2026-09-19-typeface/home-smooth.png) at 320 × 240. Also checked:
[long city/negative temperature](evidence/2026-09-19-typeface/home-long.png),
[104°F](evidence/2026-09-19-typeface/home-fahrenheit.png),
[unavailable](evidence/2026-09-19-typeface/home-unavailable.png),
[Stations](evidence/2026-09-19-typeface/stations-smooth.png),
[Player](evidence/2026-09-19-typeface/player-smooth.png),
[Volume](evidence/2026-09-19-typeface/volume-smooth.png), and
[Confirmation](evidence/2026-09-19-typeface/confirm-smooth.png).
These are the actual production C++ layout and LovyanGFX font renderer in RAM.

`tools/render_ui_fonts.py` passes: label insets, caption/temperature widths,
measured word spacing, Hebrew glyph presence, grayscale edge pixels, color
transfer and exact full/partial background restoration. Glyph presence does not
prove Hebrew reading order. `pio run -e esp32s3` passes with **65,420 B static RAM
(20.0%)**, **2,545,695 B flash (38.8%)**: +152 B RAM and +13,540 B flash over the
previous typography build. Font lookup tables grow by 2,232 B to approximately
5,814 B; the 153,600 B PSRAM canvas is unchanged. `git diff --check` passes.

**Visual:** native comparison favors the revised typography; another device
photo is pending. **Functional:** build/raster checks pass. **Hardware:** this
revision has not been flashed in this task; sustained audio and runtime resource
measurements remain open. P2/P3 are not complete. The target's enlarged image
cannot establish the exact sharpness attainable at native 320 × 240, but the
first smooth-font revision was not the practical limit of typeface fidelity.

## 2026-09-19 — Memory-composited smooth typography; device test pending

This entry supersedes the font rollback below. The supplied photo shows the
legible but jagged `Font0` / FreeSans bitmap path. Inspection of pinned
LovyanGFX 1.2.21 confirms that transparent VLW glyphs call `readRectRGB()` on
their destination. The earlier physical failure was attributed to panel
readback; no new electrical/readback measurement was performed in this task.

The native UI now composes into one reusable 320 × 240 RGB565 PSRAM sprite.
Grayscale glyph edges blend against that memory, including the photographic
background and solid tiles. Only finished opaque pixels reach the TFT. No
smooth font is drawn directly to the panel. Licensed DejaVu Sans glyphs are
loaded once; fonts and canvas allocation are not repeated on redraw. If PSRAM
or font allocation fails, the bitmap path remains usable and serial output
identifies the fallback. Successful initialization logs `Smooth fonts enabled`.

Small labels are proportional 11 px glyphs; body/title/temperature/clock use
16/22/32/40 px sizes. Home city text fits more naturally, the degree symbol is
anti-aliased, and temperature space accommodates three-digit Fahrenheit values.
Centered labels now honor vertical centering (LovyanGFX's `drawCenterString`
forces top-center regardless of the previously selected datum). Header clock
and Wi-Fi have separate slots, shared by full and partial refreshes. Valid UTF-8
is no longer split when ellipsizing. Hebrew alphabet coverage is present, but
bidi layout, niqqud and comprehensive Unicode handling remain unfinished P2 work.

### Evidence and verification for this typography change

- [Native Home](evidence/2026-09-19-fonts/home-smooth.png) and
  [bitmap comparison](evidence/2026-09-19-fonts/home-bitmap.png), both 320 × 240.
  The comparison uses the updated layout with the fallback bitmap fonts, not a
  reconstruction of the user's photograph.
- Native [Stations](evidence/2026-09-19-fonts/stations-smooth.png),
  [Player](evidence/2026-09-19-fonts/player-smooth.png),
  [Volume](evidence/2026-09-19-fonts/volume-smooth.png), and
  [Confirmation](evidence/2026-09-19-fonts/confirm-smooth.png) were inspected for
  typography. These fixtures do **not** establish complete screen acceptance:
  existing artwork/transport substitutions and the volume panel's slider/value
  placement still differ from the concept.
- [Long city/negative temperature](evidence/2026-09-19-fonts/home-long.png),
  [104°F](evidence/2026-09-19-fonts/home-fahrenheit.png), and
  [unavailable data](evidence/2026-09-19-fonts/home-unavailable.png) were inspected.
- `tools/render_ui_fonts.py` extracts the production C++ layout and links the
  actual pinned LovyanGFX sprite, PNG and VLW renderer with `display_fonts.cpp`.
  Only Arduino's String container and device state are adapted for the host.
  It confirmed 239 intermediate RGB565 pixels in a representative glyph run,
  exact red/green/blue transfer ordering, pixel-identical Home restoration after
  content changes, and partial clock refresh equality with a fresh render.
  It is native software evidence, not TFT or SPI hardware evidence.
- `pio run -e esp32s3` passed: **65,268 B static RAM (19.9%)**, **2,532,155 B
  flash (38.6%)**. Compared with the recorded rollback: +728 B static RAM and
  +82,428 B flash. `git diff --check` passed.
- Runtime allocations add **153,600 B PSRAM** for the canvas and approximately
  **3,582 B** of font lookup tables, excluding allocator overhead. The static
  build report does not measure these. Transfers use synchronous eight-row
  stripes with `audio.loop()` between stripes; composition/PNG time, audio
  service gaps, free heap and actual PSRAM availability still need measurement.

**Visual:** native typography correction inspected; physical legibility pending.
**Functional:** build and native raster checks pass; device input/playback pending.
**Hardware:** not flashed or observed in this task. Test the serial initialization
result, Home and other pages, minute rollover, repeated navigation, Fahrenheit,
and sustained playback. P2/P3 remain incomplete.

## 2026-09-19 — Partial functional slice; P2 and P3 incomplete

### 2026-09-19 — P2 Home correction awaiting a native-size photo

`docs/bg1.png` is now the supplied, reproducibly resized 320 × 240 background
asset used by the native renderer.  The Home renderer was revised after a device
photo comparison: its opaque weather and player cards were obscuring the sunset
composition.  It now keeps the image visible, uses native weather artwork and a
large clock, has a bounded Now Playing return target, and uses muted icon-led
destination tiles.  This is a P2 visual correction, not an acceptance result.

The 2026-09-19 `pio run -e esp32s3` build passed after the correction: 64,532 B
RAM (19.7%) and 2,447,103 B flash (37.3%). `git diff --check` also passed.
No new physical display, touch, encoder, or sustained-audio observation has yet
been collected for this revision. The previous device photo remains evidence of
the pre-correction composition only.

### 2026-09-19 — Second Home photo: P2 deviations corrected in source

The second physical Home photo established these P2 failures: an opaque Home
header, time/Wi-Fi collision, insufficient bottom tile margin, white tile
outlines, an indistinct Favorite glyph, overly large tile labels, and the absence
of weather after reboot. These are not deferred P4+ features. The header, date,
Wi-Fi glyph, Home tile geometry/focus marker/labels, and native heart were
corrected in the production renderer. The weather regression had a concrete
cause: the controller loop no longer called the old weather function, which was
also its fetch scheduler. That scheduler is now a rendering-free
`updateWeatherData()` call in the main loop; the old painter is not invoked.

The post-correction build passed (`pio run -e esp32s3`): 64,540 B RAM (19.7%) and
2,449,683 B flash (37.4%). `git diff --check` passed. The corrective build has
not been flashed for a post-change photo or an API-success observation, so this
entry is not visual, weather, or hardware acceptance.

### 2026-09-19 — Anti-aliasing trial rejected by physical display

The third physical Home photo rejected the attempted VLW anti-aliased font and
smooth/readback-dependent icon path: dynamic text became unreadable and icon
geometry degraded on this panel. The cause is specific to that technique's
alpha blending/readback assumption, not a claim that the hardware supports
native anti-aliasing. The complete trial was removed from production source and
the prior legible bitmap-font/native-primitive path was restored. The requested
temperature degree ring remains. No generated font asset or system font is part
of the build after this rollback.

The rollback build passed: 64,540 B RAM (19.7%) and 2,449,727 B flash (37.4%);
`git diff --check` passed. A device photograph of the rollback is still needed.
P2 typography and icon fidelity remain open rather than being marked accepted.

This corrects the earlier “P0 to P3 first on-device slice” label. That label did
not establish completion of the guide's phases or physical verification. The
current renderer follows the superseded cream/olive proposal, rather than
[uiconcept.png](uiconcept.png). The user requires visual similarity to that image;
see the [visual contract](visual-contract.md). No visual acceptance is recorded.

| Package | Status and remaining work |
| --- | --- |
| P0 | Partial: build report below exists; runtime memory, actual TFT timing, audio service gaps and physical baseline checks remain unverified here. |
| P1 | Partial: input/controller foundations exist. Audit navigation, transition coverage, dim/wake/priority and device behavior against the guide; the current Listening/Stations/StandbyConfirm pages do not cover the concept's Home/list/player navigation. |
| P2 | Incomplete, reopened: concept background, artwork/icon/font assets, Hebrew/mixed text, reusable concept compositions, photo restoration/stripe rendering and native visual evidence are missing. These are P2 work, not later polish. |
| P3 | Functional integration implemented; concept visual acceptance and all required sustained hardware checks remain open. |
| P4–P8 | No completion claimed by this slice. Continue after the prerequisite packages satisfy their checks. |

### Reusable implementation already present

Implemented a native 320 × 240 LovyanGFX screen path that starts on Listening
and includes a Stations list, mute, canonical 0…21 volume readout, and a
cancel-first standby confirmation. The list filters empty station slots and
keeps focus separate from the selected/tuned slot.

The encoder and XPT2046 now produce debounced semantic input: rotary detents
are transferred under a critical section, a press yields either Push or Hold,
and taps activate only after release within a 10 px tolerance. The physical
panel still needs calibration, target-size, contrast, encoder-direction, audio
continuity, and touch-edge verification.

Previously recorded build evidence: `pio run -e esp32s3` succeeded on 2026-09-19. It reported
64,492 bytes RAM (19.7%) and 2,257,119 bytes flash (34.4%). The configured
environment identifies the target as an N8 board in the build banner despite
the project OPI PSRAM settings, so runtime PSRAM availability remains an
explicit on-device measurement rather than an assumption.

This plan correction changes documentation only and does not rerun or extend that
build evidence. Hardware appearance, audio continuity and interaction have not
been verified by this documentation review.

### Next implementation handoff

Preserve useful input/controller/playback code. Complete the missing P0/P1 checks,
then P2's native Home, Stations, Live Player, Volume and Confirmation fixtures before
finishing P3's live integration. Do not postpone Hebrew/RTL, artwork, background
restoration or concept layout to P7. Podcast/settings/weather migration and
nonblocking metadata work retain their packages in the guide.

The user has offered a clean background; a separate clean asset is still pending.
Follow the visual contract's source-image and conversion requirements. A temporary
navy fixture background is allowed during development but cannot close visual
acceptance. No whole-screen image presentation mode is being restored.

### Visual acceptance record

| Screen / reference panel | Native renderer evidence | Visual result | Functional result | Hardware result |
| --- | --- | --- | --- | --- |
| Home / 1 | Native source implemented; new photo pending | Pre-correction photo failed comparison: opaque middle cards hid the sunset composition. Corrected native Home is awaiting a 320 × 240 device comparison. | Home navigation/Now Playing target compiled; no new device interaction result | Pre-correction photo only; no post-correction visual, touch or audio result |
| Live Stations / 2 | None recorded for concept rendering | Fail: current slice uses cream/olive presentation | Partial implementation; acceptance not verified | Not verified |
| Live Player / 3 | None recorded for concept rendering | Fail: current Listening composition differs | Partial implementation; acceptance not verified | Not verified |
| Volume overlay / 11 | None recorded | Not verified; concept overlay missing | Volume behavior exists; overlay not verified | Not verified |
| Confirmation / 16 | None recorded for concept rendering | Not verified | Standby confirmation exists; acceptance not verified | Not verified |

Future agents must attach evidence locations and record deviations with reasons,
updating visual, functional and hardware results separately. Do not infer physical
success from compilation, screenshots of the reference, or this table's existence.
