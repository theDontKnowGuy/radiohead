# Touch UI implementation status

## 2026-09-23 — Matching web-handoff typography

**Scope:** the TFT Settings → Web configuration handoff now uses the accepted
boot handoff's text hierarchy and optical spacing. Both render five rows with
centers 28 px apart: a 16 px heading, 13 px supporting labels, and 18 px URL,
IP-address and network-name values. `http://radio.local` therefore no longer uses
the oversized 24 px bold face. The connected and setup-AP variants share this
renderer with the boot handoff while retaining their own heading and QR payload.
The handoff renderers also clear any retained canvas clip before drawing their
details so a preceding screen cannot partially erase the address text.

**Visual: pass in the native production renderer.** The 320×240 fixtures show
the revised [connected Settings handoff](evidence/2026-09-23-web-handoff-typography/settings-connected.png),
[setup-AP handoff](evidence/2026-09-23-web-handoff-typography/settings-setup-ap.png),
and unchanged hierarchy in the [connected boot handoff](evidence/2026-09-23-web-handoff-typography/boot-connected.png).
The fixture asserts that the full URL and representative DHCP address fit their
137 px text column without truncation. **Functional: pass for fixture/build
checks.** `python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
`git diff --check` pass. The build reports **96,340 B RAM (29.4%), 3,491,471 B
flash (53.3%)**, and a **3,542,295-byte** total image. **Hardware: not verified;
not flashed.** Final optical size, QR scanning and legibility still require the
physical TFT. TypeSafe's deterministic/semantic separation was applied; no live
Jev judgment or runtime AI integration was used.

## 2026-09-23 — Web configuration settings icon

**Scope:** the TFT Settings list now distinguishes the browser configuration
handoff from the local Wi-Fi page. **Web configuration** uses the existing white
gear artwork, while **Wi-Fi** retains the prepared signal icon. Navigation, touch
targets, web routes and persisted settings are unchanged. The native host fixture
also gained inert stubs for the firmware's existing Wi-Fi status helpers so its
production-render extraction builds again.

**Visual: pass in the native production renderer.** The gear is centered in the
first 46 px row and remains clear of the label and card edge in the native
[320×240 Settings fixture](evidence/2026-09-23-web-configuration-icon/settings.png).
**Functional: pass for fixture/build checks.** `python3 tools/render_ui_fonts.py`,
`pio run -e esp32s3`, and `git diff --check` pass. The shared-worktree build
reports **96,340 B RAM (29.4%), 3,491,639 B flash (53.3%)**, and a
**3,542,459-byte** total image. **Hardware: not verified; not flashed.** Final
gear legibility and alignment still require observation on the physical TFT.
TypeSafe's deterministic/semantic separation was applied; no live Jev judgment
or runtime AI integration was used.

## 2026-09-23 — Artwork boot screen and progress

**Scope:** startup now shows the supplied `docs/boot.png` artwork before the
network QR handoff. The build reproducibly resizes its native 4:3 composition to
the rectangular 320×240 panel and embeds the compressed PNG in program flash as
a generated `BootLogo.h`. A centered blue/cyan progress pill animates over seven
seconds. Wi-Fi association begins underneath the animation; if the existing
15-second join window is still pending, the completed bar remains visible until
the join succeeds or that window expires. Requested setup mode and missing
credentials retain their existing AP behavior and show the same seven-second
boot animation before the setup QR.

**Visual: source and generated asset inspected at native resolution.** The
generated 320×240 artwork preserves the supplied image edge to edge without a
square crop; the progress bar sits in the open lower-center region. **Functional:
pass in build.** `pio run -e esp32s3` and `git diff --check` pass. The build
reports **96,340 B RAM (29.4%), 3,491,195 B flash (53.3%)**, and a
**3,542,015-byte** total image. **Hardware: not verified; not flashed.** Panel
color, PNG decode, progress pacing and the connected/failure/AP transitions still
need observation on the physical radio. TypeSafe's deterministic/semantic
separation was applied; no live Jev judgment or runtime AI integration was used.

## 2026-09-23 — Branded Wi-Fi recovery handoff

**Scope:** every startup path that enters the `Radio_Setup` access point now
uses the same branded 320×240 composition as the connected-device boot QR:
no saved credentials, an encoder-held setup request, and failure to join the
saved network after the existing 30 × 500 ms attempt window. The setup variant
uses the existing Wi-Fi join QR, labels the action “Connect to Wi-Fi,” shows the
actual AP address beneath “After joining, open:”, and retains the setup SSID for
manual connection. The connected `radio.local` variant and its ten-second
handoff are unchanged. AP recovery remains visible across ordinary UI redraws;
the web server continues to be serviced and no credential is displayed or
encoded.

**Visual: pass in the native production renderer.** The new setup composition
was inspected at native 320×240 in the reproducible
`.pio/ui_native/wifi-setup-boot-smooth.ppm` fixture. The version-3 join QR is
centered within the existing card, and `192.168.4.1` plus `Radio_Setup` fit
without clipping. **Functional: pass in fixture/build.**
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
`git diff --check` pass. The shared-worktree build reports **96,164 B RAM
(29.3%), 3,382,107 B flash (51.6%)**, and a **3,432,487-byte** total image.
**Hardware: not verified; not flashed.** The physical TFT still needs all three
entry paths checked, and the Wi-Fi QR/address must be verified from a phone.
TypeSafe's deterministic/semantic separation was applied; no live Jev judgment
or runtime AI integration was used.

## 2026-09-23 — Weather and time removed from TFT Settings

**Scope:** the TFT Settings list now contains only Wi-Fi, Display, Audio and
Device. Device moves into the fourth row on the first page, and the former
Weather & Time browser-handoff route and render branch are removed. Weather and
clock display on Home, persisted configuration, and the web Weather & Time page
are unchanged; their settings are now web-only.

**Visual: pass for native geometry.** The production C++/LovyanGFX fixture was
inspected at 320×240 and shows the four retained rows with no Weather & Time
entry: [settings](evidence/2026-09-23-settings-without-weather/settings-smooth.png).

**Functional: pass for source/host/build checks.** The controller test confirms
that paging cannot leave the single Settings page and that its fourth row opens
Device. `python3 tools/check_touch_input.py`,
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
`git diff --check` pass. The shared-worktree build reports **96,164 B RAM
(29.3%), 3,382,107 B flash (51.6%)**, and a 3,432,487-byte total image.
**Hardware: not verified; not flashed.** The updated row order and touch target
still require observation on the physical TFT. TypeSafe's deterministic/semantic
separation was applied; no live Jev judgment or runtime AI integration was used.

## 2026-09-23 — Settings touch-calibration start fix

**Scope:** the Device → Touch calibration action now always starts LovyanGFX's
interactive corner-marker flow. Previously it called the boot initializer, which
correctly preferred an existing saved calibration unless the encoder was held;
therefore the Settings action silently reloaded that saved data and returned.
Boot behavior is unchanged: a valid saved calibration is restored normally, while
holding the encoder or having no valid saved data still enters recovery calibration.
Calibration remains stored under the existing `touch` namespace and keys.

**Functional: pass for source/host/build checks.** The Device row hit target and
controller command path were already covered by the native UI fixture; the runtime
command now calls a separate forced-calibration entry point. The touch-input host
check, `pio run -e esp32s3`, and `git diff --check` pass.
The build reports **96,164 B RAM (29.3%), 3,382,223 B flash (51.6%)**, and a
3,432,603-byte total image. **Hardware: not verified; not flashed.** Starting the
flow, tapping all four markers, persistence across reboot, touch-edge accuracy and
audio continuity still require observation on the physical radio. TypeSafe's
deterministic/semantic separation was applied; no live Jev judgment or runtime AI
integration was used.

## 2026-09-23 — Square, lower Home destinations

**Scope:** the four Home destination cards now follow the supplied mockup more
closely: their native geometry changes from 72×70 to square 74×74 cards, the row
moves from y=149 to y=163, and the cards finish at y=236 with a 3 px bottom margin. The
four columns retain explicit 6 px noninteractive gaps. Their prepared icons grow
from 34×34 to 38×38 and the one- and two-line labels are rebalanced within the
taller cards. Production hit testing uses the same geometry as rendering.

**Visual: pass in the native production renderer.** The 320×240 Home and Hebrew
station-title fixtures were inspected; the enlarged row remains clear of the
weather and clock, and its 3 px bottom margin reads as the mockup's near-edge
placement. **Functional: pass.** The native fixture verifies each card's corners
and center, all three gaps, and the pixels immediately above and below the row.
`python3 tools/render_ui_fonts.py`, `python3 tools/check_touch_input.py`,
`pio run -e esp32s3`, and `git diff --check` pass. The shared-worktree build
reports **96,164 B RAM (29.3%), 3,381,595 B flash (51.6%)**, and a 3,431,891-byte
total image. **Hardware: not verified; not flashed.** Physical TFT spacing,
touch comfort and audio continuity during navigation still require device
observation. TypeSafe's deterministic/semantic separation was applied; no live
Jev judgment or runtime AI integration was used.

## 2026-09-23 — Home clock colon spacing

**Scope:** the Home clock now adds one native pixel of horizontal space on
each side of the colon. Digit-to-digit tracking, glyph masks, clock color,
vertical alignment, and the fixed x=301 right edge are unchanged. The atlas
manifest records the spacing in the same 1/10,000-pixel units as its advances.

**Visual and functional: pass in the native fixture.** The production
C++/LovyanGFX renderer verifies the exact one-pixel left gap and one-pixel right
gap for `14:37`, while exercising 24-hour, 12-hour and unavailable clock values.
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and
`git diff --check` pass. The shared-worktree build reports **96,164 B RAM
(29.3%), 3,380,755 B flash (51.6%)**, and a 3,431,051-byte total image.
**Hardware: not verified; not flashed.** Final optical spacing still requires
inspection on the physical TFT. TypeSafe's deterministic/semantic separation
was applied; no live Jev judgment or runtime AI integration was used.

## 2026-09-23 — Home station-title spacing and contrast

**Scope:** physical-TFT feedback superseded the first subtle native-render
adjustment. The Home active-station subtitle now uses a panel-calibrated x=40
drawing anchor beneath the `Radiohead` title's x=38 anchor. Its clipping boundary
moves from x=200 to x=180, leaving an 18 px gap before the widest tested Home
clock case (`12:05`). The subtitle color changes from muted gray to full white
for stronger contrast against the photograph. Marquee timing,
source labels, station identity, playback and input behavior are unchanged.

**Visual: pass for native geometry.** The production C++/LovyanGFX fixture was
inspected at 320×240 for a clipped
[long Latin station](evidence/2026-09-23-home-station-title/home-long-station.png)
and the preserved
[Hebrew station layout](evidence/2026-09-23-home-station-title/home-hebrew-station.png).
The Hebrew station and Latin source suffix remain separately rendered adjacent
runs, so the contrast and clip changes do not alter Hebrew ordering.

**Functional: pass.** `python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`
and `git diff --check` pass. The shared-worktree build reports **96,164 B RAM
(29.3%), 3,381,299 B flash (51.6%)**, and a 3,431,651-byte total image.
**Hardware: not verified; not flashed.** Final optical alignment, brightness and
clock clearance still require inspection on the physical TFT. TypeSafe's
deterministic/semantic separation was applied; no live Jev call or runtime AI
integration was used.

## 2026-09-23 — Branded configuration boot handoff

**Scope:** the connected-device startup handoff now adapts the supplied
configuration mockup into the native 320×240 product UI. It retains the existing
ten-second boot interval after the web server starts, but replaces the ordinary
page header/card with a Radiohead brand header and a handoff card placed below it
with an explicit 11 px visual gap from the subtitle. The card contains the large
configuration QR, full `http://radio.local` URL and the actual DHCP address
fallback separated by “Or.”
Both addresses use the same 18 px face and the five text rows use optical centers
spaced exactly 28 px apart; both muted supporting lines use the 13 px caption
face. The boot-only Wi-Fi/ready status
and former three-part footer are intentionally omitted. The QR still contains only
`http://radio.local`; no network credential is displayed or encoded. AP recovery
continues to use its separate Wi-Fi join QR, and the Settings Network handoff
remains an ordinary navigable page.

QR badges now use conventional black modules on a white quiet zone instead of
the former inverted presentation. This applies to configuration and AP recovery
badges and is intended to improve phone-camera compatibility; scan behavior still
requires physical-display verification.

**Visual: pass for native geometry.** The production C++/LovyanGFX composition
was inspected at 320×240 in
[the boot-screen fixture](evidence/2026-09-23-configuration-boot/configuration-boot-smooth.png).
The host fixture uses a representative `192.168.11.199` address and a QR-shaped
module grid with the production version-2 dimensions; the firmware build uses the
checked-in `http://radio.local` modules. The supplied mockup remains a visual
reference rather than a whole-screen firmware asset.

**Functional: pass in the native fixture and ESP32-S3 build.** The real QR payload
and mDNS address retain their compile-time consistency assertion.
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3` and `git diff --check`
pass. The current shared-worktree build reports **96,164 B RAM (29.3%),
3,382,199 B flash (51.6%)**, and a 3,432,523-byte total image.
**Hardware: not verified; not flashed.** Final TFT contrast, QR scanning,
the shown DHCP address and the ten-second handoff still require device observation.
TypeSafe's deterministic/semantic separation was applied; no live Jev judgment or
runtime AI integration was used.

## 2026-09-23 — Shared non-Home header

**Scope:** the user accepted the second Live Radio preview, so its header is now
the one shared renderer for every ordinary non-Home page: live list/player,
Favorites, recorded-show list/episodes/player, station options/information,
Settings and subpages, confirmation backdrops, unavailable pages and the startup
configuration handoff. The latter intentionally omits Back because it has no
navigable parent. The exclusive firmware-write overlay remains interruption-free.

The accepted component uses one y=22 optical centerline, x=40 title anchor, a
22 px Roboto Medium face for both title and clock, the existing Wi-Fi slot, and
the existing 72×48 px Back touch target. Titles are direction-aware and ellipsize
within a fixed region before the clock. Screen-specific backgrounds and all Back
navigation behavior remain unchanged.

The first filled-polygon chevron was rejected on-device as bold, sluggish and
pixelated. The accepted revision uses a native 24×24 SVG-derived asset with a
2.5 px stroke, rounded caps/join and grayscale edge coverage; it is never enlarged
at runtime. The fixture verifies anti-aliased edge pixels and a 2–5 pixel center
waist.

**Visual: pass for native geometry across the non-Home family.** Production
C++/LovyanGFX evidence includes [Live Radio](evidence/2026-09-23-shared-header/stations-smooth.png),
[Station Information](evidence/2026-09-23-shared-header/station-info.png),
[Recorded player](evidence/2026-09-23-shared-header/podcast-player.png),
[Settings](evidence/2026-09-23-shared-header/settings-smooth.png), and the
[no-Back configuration header](evidence/2026-09-23-shared-header/configure-header-smooth.png).
The same evidence directory contains every rendered list, player, Settings page
and confirmation backdrop.

**Functional: pass in the native fixture and ESP32-S3 build.** Existing hit maps
and navigation are unchanged. The shared-worktree build uses **96,780 B RAM
(29.5%) and 3,378,391 B flash (51.6%)**. **Hardware: the accepted Live Radio
chevron was user-reviewed; propagation to the other pages is not device-verified
and was not flashed.** TypeSafe's deterministic/semantic separation was applied;
no live Jev call or firmware AI dependency was used.

## 2026-09-23 — Settings header alignment, back action and contrast

**Superseded header appearance:** the shared non-Home header above replaces this
package's filled Settings-only chevron. Its navigation and contrast work remain.

**Scope:** every native Settings screen now uses one optical header centerline
for the back chevron, title, clock and Wi-Fi mark. The Settings chevron is a
filled 5 px-wide shape instead of the former hairline mark, while retaining the
existing 72×48 px touch target. Main Settings, Audio, Display, Device, Firmware,
Network and Weather/Time continue to return to their existing parent pages.
Restart/factory-reset confirmation now also accepts the visible header Back
target and returns to Device without queuing the destructive action.

All Settings pages use a 152/255 black veil over the coastal background on the
PSRAM canvas, making card/button labels easier to read while retaining the
sunset direction. The direct-TFT fallback uses solid product navy because that
path cannot safely depend on full-screen alpha readback. Non-Settings screens
are unchanged.

**Visual: pass for native geometry.** The production C++/LovyanGFX fixture was
inspected at 320×240 across the full family: [main](evidence/2026-09-23-settings-header-contrast/settings-smooth.png),
[second page](evidence/2026-09-23-settings-header-contrast/settings-page-two-smooth.png),
[Audio](evidence/2026-09-23-settings-header-contrast/settings-audio-smooth.png),
[Display](evidence/2026-09-23-settings-header-contrast/settings-display-smooth.png),
[Device](evidence/2026-09-23-settings-header-contrast/settings-device-smooth.png),
[Firmware](evidence/2026-09-23-settings-header-contrast/settings-firmware-smooth.png),
[Network header](evidence/2026-09-23-settings-header-contrast/settings-network-smooth.png),
[Weather & Time](evidence/2026-09-23-settings-header-contrast/settings-weather-time-smooth.png),
and [confirmation](evidence/2026-09-23-settings-header-contrast/settings-restart-confirm-smooth.png).
The host Network fixture stubs QR content, so its image validates only the
shared background/header treatment; production QR behavior is unchanged.

**Functional: pass.** Native hit tests cover Back on every Settings page and
the confirmation overlay. The controller test verifies that modal Back returns
to Device without emitting a restart/reset command. `python3 tools/render_ui_fonts.py`,
`python3 tools/check_touch_input.py`, `pio run -e esp32s3`, and
`git diff --check` pass. The shared-worktree build reports **96,780 B RAM
(29.5%) and 3,377,907 B flash (51.5%)**. Existing concurrent Home typography,
firmware-update and version edits remain included in those totals.

**Hardware: not verified; not flashed.** Final TFT darkness, header alignment,
finger comfort and QR readability still require device observation. TypeSafe's
deterministic/semantic separation was applied; no live Jev call or firmware AI
dependency was used.

## 2026-09-23 — Home weather and station-title alignment

**Vertical-centering follow-up:** after the current Home tiles moved from y=149
to y=163, the full weather composition now moves down 7 px as one unit. Its
icon, temperature, city and condition retain their internal spacing while the
group is again centered in the enlarged space between the active-station
subtitle and the tile row.

**Visual: pass in the native production renderer.** The result was inspected at
320×240 in [the centered Home fixture](evidence/2026-09-23-home-weather-centering/home-weather-centered.png).
**Functional: pass.** `python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`,
and `git diff --check` pass. The shared-worktree build reports **96,164 B RAM
(29.3%) and 3,382,199 B flash (51.6%)**. **Hardware: not verified; not
flashed.** Physical TFT alignment remains open. TypeSafe's deterministic/
semantic separation was applied; no live Jev call or runtime AI dependency was
used.

**Scope:** this user-requested Home-only pass aligns the temperature's visible
left edge with the city and condition captions at x=76, increases its Inter
Display Medium size from 28 px to 30 px, and aligns the active station subtitle
with the `Radiohead` title at x=38. The larger temperature moves its top anchor
from y=70 to y=68, retaining the existing y=94 lower boundary above the city.
The subtitle narrows from 166 px to 162 px so its x=200 right boundary and clock
clearance remain unchanged. Weather values, captions, marquee behavior, playback,
input and routes are unchanged.

**Visual: pass for native geometry.** The production C++/LovyanGFX fixture was
inspected at 320×240 with Latin and Hebrew station names:
[weather alignment](evidence/2026-09-23-home-alignment/home-weather-aligned.png)
and [Hebrew station alignment](evidence/2026-09-23-home-alignment/home-hebrew-station-aligned.png).
The fixture asserts the x=76 temperature anchor, enlarged temperature bounds,
x=38 subtitle anchor and preserved x=200 clip edge.

**Functional: pass.** `pio run -e esp32s3`, the native fixture, and
`git diff --check` pass. The shared worktree build reports **96,764 B RAM
(29.5%) and 3,377,327 B flash (51.5%)**. The host fixture's pre-existing firmware
updater stub was extended with the current status/version fields so the production
layout could compile; this has no device-firmware behavior. **Hardware: not
verified; not flashed.** Physical TFT alignment and legibility remain open.
This is deterministic layout work; no live Jev judgment or runtime AI integration
was used.

**Hebrew/Latin ordering follow-up:** the active station and its source suffix are
now drawn as two explicit adjacent runs. The Hebrew station owns the x=38 left
anchor; ` • Live Radio` is placed after the measured station width, so mixed-script
bidi handling cannot move the station to the suffix's right. Both runs retain one
clip region and marquee timeline. The exact `גלי צהל` fixture and the empty-name
fallback are asserted. Refreshed native evidence is linked above. The native
fixture, `pio run -e esp32s3`, and `git diff --check` pass; the shared worktree
build reports **96,780 B RAM (29.5%) and 3,377,911 B flash (51.5%)**. Hardware
remains unverified and the firmware was not flashed.

## 2026-09-23 — Home clock scale and matching temperature numerals

**Scope:** the Home numeral assets and production display renderer, using the
existing sunset composition. This supersedes the v3 clock metrics below; its
original supplied package remains intact as historical reference.

The clock is freshly rasterized from Inter Display SemiBold 4.1 at 40 px,
with the existing 0.375 px optical thickening and 0.25 px tracking. No old glyph
bitmap is enlarged. For the same `14:37`, total alpha bounds grow from 90×28 to
98×32 px; the main visible strokes occupy 31 px vertically (alpha >= 24).
The right ink edge remains x=301; the top moves from y=41 to y=37.

Temperature now uses Inter Display Medium at 28 px in both canvas and fallback
paths. Its numbers retain a 22 px ink height, start at x=83/y=72, and have a
separately rasterized 7×7 px degree ring starting at y=70. This moves the digits
7 px right and 4 px down from the prior normal-canvas sample. The city and
condition captions retain their existing geometry. White/RGB565 colors are
unchanged. Celsius/Fahrenheit conversion, weather freshness and clock formatting
are unchanged. Both numeral roles share a bounded 6,720-byte alpha workspace
(previous clock workspace: 6,080 bytes); drawing continues to service audio.

**Visual: pass for native geometry and font construction**, inspected through the
production C++/LovyanGFX path at 320×240:
[before, 14:37](evidence/2026-09-23-home-typography/home-before-1437.png),
[after, 14:37](evidence/2026-09-23-home-typography/home-clock-1437.png),
[negative temperature](evidence/2026-09-23-home-typography/home-long.png),
[104°F](evidence/2026-09-23-home-typography/home-fahrenheit.png),
[fallback](evidence/2026-09-23-home-typography/home-bitmap.png), and
[unavailable](evidence/2026-09-23-home-typography/home-unavailable.png).
No text/icon collision was observed. The direct-TFT path intentionally thresholds
the same masks because it cannot safely read the background for alpha blending;
its one-pixel edge differences remain visible in the fallback fixture.

**Functional: pass.** `pio run -e esp32s3`, the production native fixture checks,
and `git diff --check` pass. Fixtures cover fractional-alpha composition, clock
anchors, a one-digit hour, signed and three-digit temperatures in both render
paths, photo restoration and header refresh. Current worktree build:
**96,876 B RAM (29.6%), 3,376,463 B flash (51.5%)**. Concurrent station-subtitle
changes and the existing firmware-version edit were preserved; these totals are
a shared-worktree snapshot, not an isolated typography size comparison.

**Hardware: not verified; not flashed.** Final TFT color, stroke smoothness,
legibility and audio continuity still require device observation. P2/P3 hardware
acceptance remains open. TypeSafe guidance kept geometry and verification
deterministic; no live Jev judgment or runtime AI integration was used.

Regenerate masks with Pillow 12.1.1 using
`tools/prepare_home_clock_atlas.py --font-dir /path/to/Inter-4.1/extras/otf`.
The manifests record the upstream release URL, font hashes, rasterizer versions,
cell metrics and degree construction. The Inter license is retained in
`assets/fonts/Inter-LICENSE.txt`; ordinary builds use the committed atlas assets.


## 2026-09-22 — Finger sensitivity reopened; acquisition diagnostics

**Hardware acceptance remains failed:** the user reports excellent fingernail
response but excessive force for a finger pad. History/source inspection found
the previous pressure-gate bypass, 250 kHz same-axis acquisition, largest-cluster
filter and immediate button activation still present. This is not evidence of
which firmware is installed, nor proof that timing elsewhere has not regressed.

Fixed a concrete buffer over-read in the application wrapper: LovyanGFX 1.2.21's
ESP32 `spi::readBytes()` copies `(len + 3) & ~3` bytes to its FIFO. The 41-byte
transfer therefore needs 44 backing bytes. The buffer is now padded and zeroed;
the wire transfer remains 41 bytes. No evidence links this defect to pressure.

The existing `TOUCH_DEBUG_ENABLED=1` now also reports rejected acquisition frames:
all-rail, single-valid-sample, inconsistent-coordinate and accepted counts per
axis, plus the last frame's range and cluster sizes. The old `raw` counter is
post-filter and could not make this distinction. Mapping/event reports defer
without blocking when the ADC report has just occupied the USB output buffer.
Normal firmware retains diagnostics off. No thresholds, clock, calibration,
UI layout, touch targets, pins, persistence or vendored files changed here.

[The diagnostic protocol](touch-diagnosis.md) and
`tools/capture_touch_diagnostics.py` provide labeled idle/nail/light-pad/firm-pad
captures and explicitly exclude unrelated serial logs. A real finger comparison
is still required before choosing another sensitivity adjustment or concluding
that a panel replacement is necessary.

**Verification:** production host touch/controller tests and `git diff --check`
pass. A pseudo-terminal smoke check verified capture of both diagnostic streams
and exclusion of other logs. Normal and diagnostic ESP32-S3 builds pass in
separate build directories after the initial shared build lost object files
during concurrent work. Normal: **89,540 B RAM, 3,352,179 B flash**. Diagnostic:
**89,604 B RAM, 3,353,675 B flash** (+64 B RAM/+1,496 B flash). These are shared
worktree build snapshots including unrelated ongoing network work, not isolated
touch-feature size deltas. No visual change requires new render evidence.

**Device:** USB ESP32 detected at `/dev/cu.usbmodem2101`; neither build was flashed
in this task. Finger response, idle false activations, edges and audio continuity
are unverified. The capture needs a person applying the labeled contacts. No live
Jev call was used; this is deterministic hardware diagnosis. TypeSafe live docs
were inaccessible, so the local skill's design guidance was applied.

## 2026-09-22 — Network QR setup and configuration handoffs

Connected startup now presents a `radio.local` configuration QR for ten seconds;
the Network settings page repeats it. No saved Wi-Fi credentials, a failed
15-second connection attempt, or explicit boot setup instead presents an open
`Radio_Setup` join QR, the AP address, and the reason. Both badges retain the
sunset/navy/blue screen language, while their QR modules are pure black/white
whole pixels for camera contrast.

**Visual:** not verified on a QR-specific native fixture. **Functional:**
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and `git diff --check`
pass. **Hardware:** not flashed; physical TFT readability, phone QR decoding,
AP join, `http://radio.local` resolution, and failed-join timing remain open.

## 2026-09-21 — Settings Wi-Fi and Audio icon clarity

The Settings Wi-Fi row now reuses the same prepared Wi-Fi asset as the page
header. The Audio row now uses a conventional right-facing speaker with sound
waves, replacing the reversed horn that was harder to recognize at TFT size.
No Settings actions, hit regions, persistence, or audio behavior changed.

**Visual:** inspected the production native 320×240 fixture at
`.pio/ui_native/settings-smooth.png`. **Functional:**
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and `git diff --check`
pass. The build uses 87,012 B RAM (26.6%) and 3,294,291 B flash (50.3%).
**Hardware:** not flashed; physical TFT legibility remains unverified.

## 2026-09-21 — Audio editor: centered rows, live preview and hold repeat

The audio rows span 304 px with equal 8 px side margins and 44 px +/- targets.
Changes preview immediately without updating committed tone or writing NVS.
Save commits; Cancel and header Back restore committed tone. Newer web tone
values refresh the draft and stop any captured repeat. Holding a tone button
repeats after 450 ms, then every 120 ms on valid contact. Release, sliding off,
dim-wake and alarm consumption disarm repetition; other buttons remain single
activation per press.

**Visual:** inspected the production 320×240 fixture at
`.pio/ui_native/settings-audio-verified.png`, including -15/0/15 values.
**Functional:** controller tests cover preview, Save/Cancel/Back, all six bounds,
encoder independence, web conflicts, repeat timing, release, target departure
and timer rollover. Native layout/hit tests, `pio run -e esp32s3` and
`git diff --check` pass. The shared build uses 87,012 B RAM (26.6%) and
3,294,191 B flash (50.3%); concurrent Home edits also contribute to its size.
**Hardware:** not flashed; audible preview/reversion, held-finger comfort,
reboot persistence and audio continuity remain unverified. TypeSafe's design
guidance applies here as deterministic rules; live docs were inaccessible
and no Jev/API integration was used.

## 2026-09-21 — Audio editor width, live preview and held adjustment

Bass, Mid and Treble now use full-width 304 px coastal/navy cards with equal
8 px side margins. Values are centered in their column, including −15 and 15;
the relocated minus/plus hit regions remain 44×46 px. This reuses the existing
wide surface and retains the concept's photo, typography and blue controls.

Each adjustment sends a bounded audio-only preview. Committed `gB/gM/gT`
remain unchanged until Save, so unrelated volume/settings saves cannot persist
an unfinished preview. Cancel and header Back restore committed tone. A changed
web tone supersedes the open draft and stops its repeat, preventing stale Save
or Cancel from replacing that newer value. Preview commands are coalesced
separately from encoder commands.

A fresh press changes one step immediately. Continued contact on that same
plus/minus target repeats after 450 ms, then once per 120 ms, without catch-up
bursts. Release, leaving the original target, page changes, dim-wake consumption
or alarm consumption prevent further repeats. Other touch targets keep their
single-action behavior. Values stay within −15…15; reaching a limit queues no
more preview or redraw work.

**Visual: pass (native fixture).** Inspected the production 320×240 renderer
with minimum/zero/maximum values:
[audio settings](evidence/2026-09-21-audio-settings/settings-audio.png).
**Functional: pass (host/build).** `python3 tools/check_touch_input.py` covers
preview/commit/cancel/back, all six bounds, repeat timing/release/slide, timer
wrap, wake/alarm suppression, encoder coexistence and newer web edits.
`python3 tools/render_ui_fonts.py` passes with relocated hit-region edges/gaps.
`pio run -e esp32s3` and `git diff --check` pass: 87,012 B RAM (26.6%) and
3,294,191 B flash (50.3%) for the shared worktree, which also contains concurrent
Home/Display updates. No vendor changes or firmware binaries were added.
**Hardware: not verified.** Not flashed in this task; audible preview/revert,
held-finger comfort, reboot persistence and sustained playback during repeat
still need physical-device checks. This does not close P2/P3 hardware acceptance.

TypeSafe's deterministic/semantic separation was applied: explicit geometry,
bounds, timing and state transitions belong in C++. Live documentation access
failed; no live Jev call or firmware AI dependency was used.

## 2026-09-21 — Display auto-dimming row width

The Display editor's auto-dimming card now spans 304 px with equal 8 px side
margins. The label and timeout share one line; the right-aligned minus/plus
buttons have matching relocated 44×52 px touch targets. Timeout choices and
Save/Cancel behavior are unchanged.

**Visual:** inspected the production native 320×240 fixture at
`.pio/ui_native/settings-display-smooth.png`, plus the longest 120-second label.
**Functional:** native renderer checks and targeted dim-button hit checks pass;
`pio run -e esp32s3` and `git diff --check` pass. **Hardware:** not flashed;
physical TFT appearance and touch comfort remain unverified.

## 2026-09-21 — Home visual improvement round

Home now uses a brighter, warmer 4:3 coast treatment (92% source saturation
and a uniform 16% black veil) so the supplied sunset remains visible without
losing text contrast. Its four navigation destinations are a 72×70 px row at
`x=7/85/163/241`, `y=149`: dense blue, green, purple and slate gradients with
visible borders, 34 px icons, 11 px medium labels and the same 72×70 px touch
regions. The header uses a 20 px radio mark and an 18 px title; its subtitle
now says `Recorded Show` while recorded playback is active instead of always
claiming Live Radio. Each condition artwork's visible top aligns with the
temperature at `y=68`; city and condition use `y=94/112` baselines for easier
scanning. The dynamic source-matched clock atlas was retained; its
existing approved `14:37` alpha-overlay validation continues to cover its
placement and compositing.

**Visual:** inspected the native 320×240 production-path fixture at
`.pio/ui_native/home-smooth.png`. **Functional:** `pio run -e esp32s3`,
`python3 tools/render_ui_fonts.py`, and `git diff --check` pass. The build
reports 87,012 B RAM (26.6%) and 3,294,191 B flash (50.3%). **Hardware:** not
flashed; physical TFT color/legibility, touch target comfort and sustained
audio repaint behavior remain unverified.

## 2026-09-21 — Home tile icon alignment

The Home renderer now anchors the visible top of Live Radio, Recorded Shows,
Favorites, and Settings artwork at `y=157` (8 px below the tile top), rather
than aligning their unequal transparent 34 px PNG canvases. This gives each
icon the same top clearance and keeps the artwork centered in the open area
above the labels. Tile sizes, label positions, touch regions, and navigation
are unchanged.

**Visual:** inspected refreshed native 320×240 PSRAM-canvas and direct-TFT
fallback fixtures. **Functional:** `python3 tools/render_ui_fonts.py`,
`pio run -e esp32s3`, and `git diff --check` pass. **Hardware:** not flashed;
physical TFT alignment and legibility remain unverified.

## 2026-09-21 — Settings: concept-aligned paged list and local slice

The Home Settings tile now opens a touch-only, four-row paged list using the
same right-side rail, page thumb, translucent cards, white primitives and
chevrons as Recorded Shows. It has Wi-Fi, Display, Audio, Weather & Time and
Device rows; Wi-Fi and Weather & Time lead to an honest browser handoff showing
the current local address rather than inert controls. Audio has bounded −15…15
bass/mid/treble drafts with explicit Save/Cancel; Display has a saved automatic-
dim timeout limited to 15/30/60/120 seconds; Device provides touch calibration,
About, separately confirmed Restart and Factory Reset. Factory reset retains
touch calibration. The encoder remains global volume/mute/power throughout.

**Visual:** inspected native 320×240 production-path fixtures for Settings,
Audio, Display and Device in `.pio/ui_native/settings-*-smooth.png`; they use the
shared coastal/navy renderer and 44 px editor targets. **Functional:**
`python3 tools/check_touch_input.py`, `python3 tools/render_ui_fonts.py`,
`pio run -e esp32s3`, and `git diff --check` pass. The build reports 86,892 B
RAM (26.5%) and 3,280,667 B flash (50.1%). **Hardware:** not flashed; physical
touch targets, audio continuity during navigation/calibration, reboot persistence,
dimming, restart and factory-reset confirmation remain unverified.

## 2026-09-21 — Source-matched Home-clock asset integration

The Home clock now uses the supplied source-matched `home-clock-assets` package
as its source of truth: tight native alpha masks with declared y placement,
per-glyph advances, and a visible-ink anchor `(301,40)`. The source-derived
`1`, `3`, `4`, `7`, and colon retain their mockup tracing; the remaining supplied
glyphs remain part of the same dynamic atlas. The converter pads each source
mask into its fixed atlas cell with no local font dependency or scaling. It
verifies all supplied reference strings and the transparent 320×240 `14:37`
overlay byte-for-byte; the required Home ink bounds are x=212…301 and y=40…69.
Date, background, clock color, and compact
page-header clocks remain unchanged. At runtime, the firmware first composes
overlapping glyph-cell alpha into one bounded 128×32 mask, then blends each
final clock pixel once over the fresh Home canvas; this follows the package's
required composition rule and prevents doubled strokes at cell overlaps. The
no-PSRAM direct-TFT fallback now uses that same composed atlas once, with an
opaque 128-level alpha cutoff because TFT readback cannot reliably blend an
anti-aliased edge over the sunset background; it no longer invokes the old
`FreeSansBold24` clock renderer.

**Visual:** the native fixture's approved overlay matches all alpha and visible
pixel values exactly; physical/reference final acceptance remains open.
**Functional:** `tools/render_ui_fonts.py` and `pio run -e esp32s3` pass; the
build reports 86,876 B RAM (26.5%) and 3,274,307 B flash (50.0%). The scoped
diff whitespace check passes. **Hardware:** not flashed; TFT appearance and
sustained-audio behavior remain unverified.

## 2026-09-21 — Fixed Home-clock alpha atlas

The Home clock no longer uses a VLW font. Its reproducible recipe rasterizes
Inter SemiBold at 8×, crops actual source-ink bounds, baseline-aligns each glyph,
Lanczos-downsamples it into 8-bit alpha masks for 0–9, colon, and the
unavailable-state dash, then modestly strengthens alpha for an opaque core.
Runtime alpha-blends every glyph pixel over the freshly rendered readable RGB565
Home canvas, avoiding an assumed-background fringe or any font/sprite scaling.
The final trial measures 33 px digit ink, a 3 px opaque zero stem, 21 px tabular
digit advances, a 10 px colon advance with 4 px ink, and an 87×33 px `14:37` ink box
at x=217…304/y=42…75. Its #F5F5F5 clock color and date #D8DDE3 remain unchanged. The
Home composition uses logical anchor x=306/y=37. Compact opaque/list/Recorded Show clocks remain 18 px Roboto
Regular VLW because this atlas is intentionally Home-specific.

**Visual:** production native [14:37 fixture](../../.pio/ui_native/home-clock-1437.png)
inspected against the stated 87×33 px target; physical/reference final acceptance
remains open. **Functional:** `tools/render_ui_fonts.py` (including anti-aliasing
checks), `pio run -e esp32s3`, and `git diff --check` pass. The build reports
82,268 B RAM (25.1%) and 3,218,287 B flash (49.1%). **Hardware:** not flashed;
physical TFT appearance, repaint timing, and sustained-audio behavior remain
unverified.

## 2026-09-21 — Recorded episode player composition

The recorded episode player now uses a 115×115 rounded generic microphone/show
artwork at x=20/y=47 because no local show image is available. Its dedicated
background has a 14–20% right-increasing black veil; episode title and published
date share a right edge at x=305 with RTL-aware text layout. The progress control
is a 6 px track with an 8 px thumb at y=126, and playback centers move to
82/160/238 at y=194. The header uses a lighter 20 px regular face with separated
clock/Wi-Fi geometry. Playback, metadata fetches, persistence, and commands are
unchanged.

**Visual:** updated native 320×240 production fixture inspected. **Functional:**
`pio run -e esp32s3`, `tools/render_ui_fonts.py`, and `git diff --check` pass;
the fixture also asserts the revised progress and 82/160/238 transport targets.
**Hardware:** not flashed; physical appearance, touch targets, repaint timing,
and sustained audio remain unverified.

## 2026-09-21 — Home graphic polish

The Home-specific comparison work is tracked in
[home-graphic-polish-plan.md](home-graphic-polish-plan.md). It implements a
uniform 25% background-black veil, a 17.4% smaller regular clock, regular
condensed weather digits, 15.4% smaller medium tile labels, muted translucent
tile gradients, a low-opacity focus outline, softer #E8EEF3 Home text/icons,
and a tighter weather text block. The direct 4:3-to-320×240 resize remains
uncropped. It does not change playback, input, routes, persistence, or weather
fetching.

**Visual:** the native 320×240 production fixture was inspected. **Functional:**
`pio run -e esp32s3`, `tools/render_ui_fonts.py`, and `git diff --check` pass.
**Hardware:** this revision was not flashed; TFT appearance, touch behavior,
repaint timing, and sustained audio remain open for device verification.

## 2026-09-21 — Home graphic polish, second pass

The user-directed refinement reduces the Home clock from 38 px to 34 px and
the temperature from 30 px to 27 px, both regular condensed. It introduces a
15 px regular Home title, 40 px icons, label baselines 4–5 px higher, a
68×70 tile / 11 px gap grid, further-muted blue and green tiles, and explicit
header/weather spacing adjustments. The no-PSRAM primitive icons follow the
same larger composition.

**Visual:** the updated native 320×240 production fixture was inspected.
**Functional:** `pio run -e esp32s3`, `tools/render_ui_fonts.py`, and
`git diff --check` pass. **Hardware:** not flashed; physical TFT appearance,
touch behavior, repaint timing, and sustained audio remain unverified.

## 2026-09-20 — Four-row right-rail list refinement

The latest physical-device photo showed that the first right-rail pass still
left unused lower space and did not visually match the supplied concept. Live
Stations, Recorded Shows and Episodes now render four 46 px photo-visible,
rounded cards. Their previous bottom footer is removed. A 62 px translucent
navy rail occupies the right side, with the corrected Up chevron at the top and
Down chevron at the bottom. The numeric page count is replaced by a translucent
thumb in the middle track: its 84/42/28 px height represents one/two/three
pages, and paging moves it without selecting a row. The same
surface component also updates Favorites and compact option rows. List artwork
is a frameless 28 px mark vertically centered in its strip, and the favorite is
a filled/outlined ten-point icon instead of the old crossed-line star.

The thumb now maps the actual bounded viewport range (for example, offsets
0 → 4 → 6 for ten items shown four at a time) onto the whole track, so the
partial final page reaches its bottom position. List cards render edge-to-edge
inside a shared 46 px card / 2 px gap grid. The focused and normal card edges
now use the same opacity, and the normal translucent fill is slightly stronger,
so the bright focused first row cannot make its following gap look wider.

**Verification:** `pio run -e esp32s3`, `python3 tools/check_touch_input.py`,
`python3 tools/render_ui_fonts.py`, and `git diff --check` pass. Native 320 ×
240 production renders are retained as [Stations](evidence/2026-09-20-right-rail/stations.png),
[Recorded Shows](evidence/2026-09-20-right-rail/recorded-shows.png), and
[Recorded Shows page 2](evidence/2026-09-20-right-rail/recorded-shows-page-two.png), and
[Recorded Shows page 3](evidence/2026-09-20-right-rail/recorded-shows-page-three.png).
Static
RAM remains **66,308 B (20.2%)**; flash is **2,775,447 B (42.3%)**. Device
verification is still open: the physical image that prompted this work is
evidence of the prior three-row build, not confirmation of this revision.

## 2026-09-20 — Immediate button presses; swipe replaced with paging

The user reports an improvement after the preceding upload, but finger-pad
presses still need too long. They explicitly prioritize fast button response and
authorize replacing swipe scrolling. This supersedes the preceding gesture
refinement, rather than adding another movement/sensitivity threshold.

- First accepted on-panel contact dispatches the button action immediately.
  There is no release wait, multi-frame gesture decision or movement veto.
- A held/sliding finger produces only one action across page changes. Release
  re-arms after 80 ms without accepted contact, bridging short weak-contact gaps.
  This interval affects re-arming only, not the first press. Wake/alarm guards
  consume the first press as before; holding cannot click after the guard clears.
- Live Stations, Recorded Shows and Episodes now use three 48 px rows, plus
  Previous/Next buttons with disjoint 160 × 52 px targets. Paging is bounded and
  never selects playback. Encoder browsing still traverses all entries. Volume
  and recorded progress tracks accept position taps; dragging/swiping is disabled.
- The ADC sampling/filter, calibration, pins and persisted keys are unchanged
  from the preceding upload. Optional touch diagnostics now count presses and
  releases; normal firmware leaves diagnostics disabled.

**Verification:** `python3 tools/check_touch_input.py` passes immediate response,
held/moving contact, intermittent contact, re-arming, page navigation without
playback, last-episode access, old hidden-row rejection, loading/short/empty
episodes, dim/alarm guards and rollover. `python3 tools/render_ui_fonts.py` passes
the production renderer and footer boundary assertions. Native 320 × 240 renders
were inspected: [stations](evidence/2026-09-20-buttons/stations.png),
[shows](evidence/2026-09-20-buttons/shows.png),
[episodes](evidence/2026-09-20-buttons/episodes.png). The sunset/blue design remains;
the lower list density and paging controls are deliberate usability adaptations.

`pio run -e esp32s3` passes: **66,308 B RAM (20.2%)**, **2,745,511 B flash
(41.9%)**. Physical finger-pad response, false activation rate, rapid repeated
taps, wake-only contact, edge targets and audio continuity still require the next
device test. A successful press now uses the first coordinate; it cannot be
cancelled by sliding away. This is the requested button-first interaction tradeoff.

**Upload:** installed normal firmware on the identified ESP32-S3 at
`/dev/cu.usbmodem2101`; flash hash verification and reset passed. Device
finger-response acceptance remains pending user observation.

## 2026-09-20 — Finger-contact noise and initial target capture

The user still reports reliable fingernail input but frequent missed/delayed
finger-pad presses. Finger usability remains **fail for the preceding firmware**;
this revision is a software correction awaiting a physical finger test.

Two reproducible filtering problems were corrected:

- The ADC reducer chose the closest pair even when five other readings formed a
  different consistent group. It now takes the median of the largest group within
  the existing 150-count span. Rail rejection and the two-valid-reading minimum
  remain unchanged. The same 41-byte/250 kHz acquisition is used.
- Gesture target capture previously used the first raw screen coordinate, and
  an unfiltered second sample could permanently mark a tap as moved. The first
  three-sample median now refines the capture and movement origin. A Refine event
  cannot change the owning page or reactivate a consumed wake/alarm gesture.
  Three well-separated positions remain movement, so a quick swipe cannot turn
  into a row tap. Single-frame quick taps still work; a two-frame drag still
  cancels selection. No added hold time or pressure threshold is required.

`TOUCH_DEBUG_ENABLED=1` now observes the normal production poll, rather than
providing an unused second reader that draws on the display. Once per second it
reports poll/raw-contact/on-panel counts, begins/taps/swipes, maximum polling gap
and read duration. Output is skipped when USB is disconnected or lacks buffer
space. This option remains **off in the normal firmware**. If finger misses
persist, enable it and compare idle, nail and finger trials: zero raw contacts
points below gesture recognition, raw without on-panel points at mapping/noise,
and accepted contacts without taps points at gesture/release behavior. These
counters do not measure physical pressure; the driver's `size=1` is not pressure.

The production host checks cover the repeated-spike ADC regression, noise at
each of the first three positions, brief contacts, two-frame drag cancellation,
quick and sustained swipes, reverse scrolling, bounds, dim/alarm controller
guards and timer rollover. They pass, as does `git diff --check`. Appearance,
calibration, pins and persistence are unchanged; no new visual composition is
introduced. Physical touch sensitivity, idle false activations, calibrated edge
accuracy, wake consumption and audio continuity still need device observations.

**Build/upload:** `pio run -e esp32s3 -t upload --upload-port
/dev/cu.usbmodem2101` passed, including flash hash verification and reset. The
connected device identified as ESP32-S3 with 16 MB flash/8 MB PSRAM. Normal
firmware (diagnostics off) is installed. Static RAM is **66,332 B (20.2%)**;
flash utilization is **41.9%**. Upload success is not finger-touch acceptance;
no new physical touch or audio result has been observed in this task.

## 2026-09-19 — Same-axis acquisition for weak finger contact

Device feedback after the preceding pass: scrolling works partially, but finger
pad sensitivity remains unacceptable; a light fingernail tap is much easier.
That is consistent with a mechanical pressure/contact contribution and does not
establish how much software can recover. Sensitivity acceptance remains **fail**
for the preceding build and **not verified** for this revision.

The reader no longer alternates Y/Z1/X/Z2 on every conversion. It holds Y active
for ten conversions, then X for ten, discards the first three after each switch,
and selects a consistent pair independently from the seven remaining readings
on each axis. At 250 kHz the discarded conversions provide about 192 microseconds
of additional driven settling time per axis. The 41-byte burst takes roughly
1.31 ms of wire time, down from 1.82 ms. No delay, allocation, retry loop, pressure
threshold, pin change or persisted-calibration change is introduced.

This follows the repeated-conversion/PD0 guidance in the related
[ADS7846 datasheet, Touch Screen Settling](https://www.ti.com/document-viewer/ADS7846/datasheet).
Its benefit on this XPT2046 panel is a hypothesis requiring physical verification.
ADC rails remain excluded (128 < value <= 3968), and each axis needs two readings
within 150 counts. This retains coordinate validation without requiring the
same pair of conversions to be good on both axes.

`tools/check_touch_input.py` passes the production frame construction/decoding,
settling exclusion, independently timed valid pairs, inconsistent/insufficient
readings and all-zero/all-one rejection, plus the existing gesture/list tests.
These synthetic cases do not establish idle false-touch behavior on hardware.
The native renderer checks and `git diff --check` pass. `pio run -e esp32s3`
passes: **66,332 B RAM (20.2%)**, **2,736,411 B flash (41.8%)** (unchanged RAM,
+64 B flash versus the preceding pass).

**Hardware:** not flashed in this task. Check light finger-pad taps, idle false
activations, coordinate accuracy with saved calibration, scrolling and audio
continuity before accepting this revision. The existing gesture thresholds,
page ownership and screen appearance are unchanged by this follow-up.

## 2026-09-19 — Finger sensitivity and recorded-list scrolling correction

Reopens the earlier light-finger acceptance: the user reports that taps still
require excessive force/holding and episode swipes do not work. A confirmed
software defect was that `uiControllerSwipe()` accepted only Live Stations;
Recorded Shows and Show Episodes ignored every swipe event.

- All three lists now scroll one row as vertical movement crosses 24 px, then
  another row per 39 px of movement, with reversal and endpoint clamping. This
  is discrete row scrolling, not animated/inertial scrolling. A scrolling
  gesture cannot activate a row on release. Loading/empty episode lists ignore it.
- Touch-only SPI frequency changes from 1 MHz to 250 kHz to increase acquisition
  time for light/high-resistance contact. The existing bounded 57-byte transfer
  takes approximately 1.82 ms of wire time, every 8 ms at most. Display frequency,
  pin assignments, raw-coordinate consistency checks and calibration are retained.
  This is a hardware tuning hypothesis, not a measured pressure improvement.
  Slower acquisition is motivated by the settling guidance in
  [TI's ADS7846 controller datasheet](https://www.ti.com/document-viewer/ADS7846/datasheet);
  effectiveness on this XPT2046 panel remains unverified.
- A three-sample median once available rejects isolated coordinate spikes;
  off-panel readings become missing samples rather than cancelling the entire
  gesture. Release debounce is 28 ms and tap movement tolerance is 18 px, with
  no minimum hold duration. Actual latency depends on loop/render time.
- Contact-down captures page/target and wakes the dim display. The whole wake
  gesture is consumed. Taps must release on their original target; horizontal
  drags can operate the recorded progress track only, never select a list row.

**Functional:** `python3 tools/check_touch_input.py` passes using the production
recognizer and controller with host hardware/state substitutes: quick tap,
brief dropout, noise, held contact, directional gestures/reversal, last-episode
selection, bounds, loading/short lists, alarm/dim guards and millisecond rollover.
`python3 tools/render_ui_fonts.py`, `pio run -e esp32s3`, and `git diff --check`
pass. Build uses **66,332 B static RAM (20.2%)**, **2,736,347 B flash (41.8%)**
(+80 B RAM / +664 B flash over the preceding recorded-shows build).

**Visual:** layout/assets unchanged; native rendering and hit checks pass.
**Hardware:** not flashed or measured in this task. Acceptance remains open:
check light quick taps at center/edges, no idle false activations, swipes through
all episodes in both directions, wake without selection, progress scrubbing,
and uninterrupted playback while repeatedly scrolling. The host tests do not
establish electrical sensitivity, full-loop input latency or audio timing.

## 2026-09-19 — P5 recorded shows and playback implementation

Recorded Shows now opens panel 5, retrieves the selected show's episodes for
panel 6 through one bounded background HTTPS job, and publishes the result only
from `mediaTick()` on the existing main/audio owner. A newer request generation
invalidates an older result, so Back or selecting another show cannot replace the
visible list or start playback from a stale response. The worker never calls
`audio.loop()`, TFT, Preferences, or the Audio object.

Episode identity is Omny's documented `Id`, never the fetched-array index or a
temporary audio URL. The parser retains bounded title/date/audio fields and the
documented `DurationSeconds` only when supplied. Panels 7 and 8 expose
pause/seek only when the installed Audio library reports a usable duration and
the source is a seekable HTTP file with byte-range support; unavailable seek and all download actions
are explicit. Show favorites are persisted in the existing versioned
`favorites` namespace using a fixed program/playlist identity slot, rather than
a filtered row position. Existing station favorites migrate from version 1.

Native production fixtures were generated and inspected at 320 × 240 in
`.pio/ui_native/recorded-shows.ppm`, `show-episodes.ppm`,
`podcast-player.ppm`, and `podcast-options.ppm`; their hit assertions cover
Back, list-row, recorded transport and the full progress-track target. The
recorded header has one transparent renderer on a common vertical centerline;
the clock tick rebuilds that page instead of painting the generic opaque clock
over it. Progress repaints once per second while playing, and the track accepts
tap and horizontal-drag scrubbing. Transport artwork now comes directly from the
user-supplied SVGs. The build rasterizes them with alpha at native 64×64 replay
and 72×72 play/pause dimensions, then embeds the PNG bytes; no font, primitive,
or runtime SVG path substitutes for their geometry. The native player fixture
was inspected with transparent icon corners over the photo. `tools/render_ui_fonts.py`
and `pio run -e esp32s3` pass with **66,332 B RAM (20.2%)** and **2,745,071 B
flash (41.9%)**. **P5 is not accepted yet:** this revision has not been flashed,
so Omny response compatibility, stale-request timing, pause/seek behavior on
actual sources, display legibility and sustained audio require device checks.

**Implementation:** complete through the supplied transport-icon integration.
Hardware acceptance remains pending the recorded device checks above.

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
| Home / 1 | Native source implemented; new photo pending | Pre-correction photo failed comparison: opaque middle cards hid the sunset composition. Corrected native Home is awaiting a 320 × 240 device comparison. | Home-first live-station return compiled; no new device interaction result | Pre-correction photo only; no post-correction visual, touch or audio result |
| Live Stations / 2 | None recorded for concept rendering | Fail: current slice uses cream/olive presentation | Partial implementation; acceptance not verified | Not verified |
| Live Player / 3 | None recorded for concept rendering | Fail: current Listening composition differs | Partial implementation; acceptance not verified | Not verified |
| Volume overlay / 11 | None recorded | Not verified; concept overlay missing | Volume behavior exists; overlay not verified | Not verified |
| Confirmation / 16 | None recorded for concept rendering | Not verified | Standby confirmation exists; acceptance not verified | Not verified |

Future agents must attach evidence locations and record deviations with reasons,
updating visual, functional and hardware results separately. Do not infer physical
success from compilation, screenshots of the reference, or this table's existence.

## 2026-09-21 — Home-first live radio

The user requested that live-station selection return to Home rather than open
the separate Listening page. The controller still queues the same bounded
`SelectStation` command, then returns to Home; this applies to station-list and
favorite-station selections. The Home active-station summary is no longer a
touch target. This changes navigation only, not the playback command, station
identity, or persistence behavior.

Home now uses its 22 px title face for the active-station summary. The
temperature uses a regenerated 32 px Roboto Condensed Bold asset; its 7 px top glyph
bearing is compensated so the rendered digits align with the weather art at
y=58. The city and condition captions are moved up to y=82 and y=99. The clock
uses a taller, narrower 46 px Roboto Condensed Bold asset at y=28, preserving
room above the active station summary.

`pio run -e esp32s3`, `python3 tools/check_touch_input.py`,
`tools/render_ui_fonts.py`, and `git diff --check` pass. The firmware reports
66,308 B RAM (20.2%) and 2,782,871 B flash (42.5%). Native Home renders,
including 104°F and long-label cases, were inspected from `.pio/ui_native/`.
These are compiler/native-render results only; this revision has not been
flashed, so playback continuity and TFT appearance remain unverified on device.

## 2026-09-23 — Firmware update screen status layout

**Functional:** Device → Firmware updates now uses two wide-card rows: Check for
updates with a bounded, state-derived status caption (not checked, checking, up
to date, update available, downloading, installed/restarting, or failed), and
Update mode with its current Automatic/Manual value aligned at the right. A
third blue Update now row appears only for a checked release awaiting manual
approval, and requests the existing background updater install path. The former
About/Back footer controls and their unreachable About page were removed; the
standard header back target remains.

**Visual:** source layout inspected against the native 320 × 240 coordinate
system; status text is ellipsized to its card width and every active row has a
48 px-high touch target. The wide-card artwork itself is 42 px high, so this
screen alone now keeps the two-line text block visually centered using +4 px
and +22 px origins for the primary and small status text. No native fixture image or physical TFT
photograph was captured for this screen.

**Build:** `pio run -e esp32s3` and `git diff --check` passed. RAM is 96,764 B
(29.6%) and flash is 3,375,847 B (51.5%). **Hardware:** not verified; exercise
every status, the mode toggle, manual Update now/reboot, header back, and audio
continuity on the device.
