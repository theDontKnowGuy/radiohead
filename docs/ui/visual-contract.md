# Radiohead UI: visual contract

**User direction, confirmed 2026-09-19:** build a UI visually similar to
[uiconcept.png](uiconcept.png). The user can provide a clean background image.
This is a faithful native adaptation of that design to the 2.8-inch TFT, not an
invitation to propose a different style. Keep it beautiful and restrained.

Read this before the [implementation guide](agent-implementation-guide.md).
The image governs appearance; the guide governs architecture, behavior and
hardware adaptations. The old `interaction-plan.md` and `radio-ui.html` are
superseded historical artifacts. The current cream/olive renderer is functional
groundwork, not an approved visual baseline.

## What must remain recognizable

| Reference | Required visual result at 320 × 240 landscape |
| --- | --- |
| Shared appearance | Coastal city at sunset visible behind content; dark navy treatment for legibility; white text; blue active states; restrained rounded surfaces and consistent white icons. Preserve the photograph's character rather than darkening it into a nearly flat fill. |
| 1 — Home | Weather at left, prominent clock at right, four labeled icon tiles along the bottom: Live Radio blue, Recorded Shows green, Favorites purple, Settings slate. Keep the composition; omit the decorative slogan if space is tight. |
| 2 — Live Stations | Back/title/time/connection header; artwork at left; primary and secondary text; blue selected row; separate star at right. Use three touchable rows rather than squeezing in the sheet's five. |
| 3 — Live Player | Artwork at left, LIVE/status and station/programme text at right, favorite indicator, circular previous/stop/next controls with a blue ring on the central control, bottom volume slider. Keep artwork and transport as the visual anchors. |
| 11 — Volume | Dark rounded floating panel over a softened background, speaker icon, blue slider and readable value. Prepare softening off-device; no runtime blur. |
| 16 — Confirmation | Light rounded dialog over a darkened/softened page, clear title, gray Cancel and action-colored confirmation. Default focus is Cancel. Use a real supported action; download deletion remains a later capability. |
| Text and assets | Native Hebrew and mixed Latin/numbers, readable hierarchy, real supplied station/show artwork where available. Missing artwork gets a consistent neutral placeholder, not a replacement for the whole artwork-led layout. |

The remaining panels inherit this same visual language. Do not replace Home with
a text-only Listening screen, replace the player transport with a Stations/Mute/Menu
footer, or substitute cream/olive colors. A photograph behind the old layout alone
does not reproduce the concept.

## Adaptations allowed for the actual TFT

- Keep layout, rendering and hit testing on one 320 × 240 coordinate system.
  Use the implementation guide's geometry; it accounts for this landscape panel.
- Enlarge touch regions to at least 44 × 44 px, separate adjacent actions, reduce
  list density, and shorten or ellipsize secondary text before shrinking it.
- Simplify tiny highlights, shadows and icon detail that disappear in RGB565.
  Precompose darkening/softening; restore the photo under dirty regions.
- Adjust photo crop, contrast and type size after native-size inspection. Record
  deviations with their physical usability or rendering reason.
- Show actual data or an honest unavailable state. Sample names, clock, weather,
  frequency and volume numbers in the sheet are not production requirements.

These constraints justify adaptation, not a new visual direction. Optimize drawing
and asset preparation before discarding the design. Keep bounded buffers, dirty
updates and audio servicing as specified in the guide.

## Clean background handoff

The user has offered a clean image; **it has not yet been supplied as a separate
asset**. Prefer that image over generating a different scene. Suggested source path:
`docs/ui/assets/background-source.png` (or `.jpg`). This path is a convention, not
a claim that the file exists.

Request the coastal sunset image without text, buttons, logos, borders or screen
captions. An opaque sRGB PNG or high-quality JPEG, ideally 4:3 and 1280 × 960 or
larger, gives room to crop; a clean 320 × 240 image is sufficient if its crop is
already final. Keep the original. Prepare 320 × 240 variants off-device, recording
crop, darkening, format and provenance in the asset manifest.

Keep the provided image visible through the intended translucent-looking surfaces.
The firmware may use precomposed opaque surfaces to obtain that appearance cheaply.
Use a reusable background asset, with real text and controls drawn separately;
do not restore the removed whole-screen image presentation mode.

While waiting, agents may implement geometry, font handling, icons, background
restoration and input. A temporary navy fill must be labeled as a development
placeholder. Missing final background means visual acceptance is pending; it does
not justify shipping another aesthetic or marking P2 complete.

## Corrected implementation sequence and evidence

1. Preserve and audit the useful input/controller/playback work. Report the exact
   P0/P1 checks still missing. Do not rewrite working behavior for cosmetic reasons.
2. Reopen P2. Prepare assets and reusable components. First produce Home, Live
   Stations and Live Player fixtures from the production C++ layout/font/drawing
   path, plus Volume and Confirmation states. Include a long mixed Hebrew/Latin
   title, selected row, favorite, missing artwork and a connecting/error state.
3. Compare each fixture with its corresponding reference panel at **native
   320 × 240**, with an enlarged copy only as an additional aid. Record appearance,
   legibility and touch-target results. A browser mockup or the concept painted as
   one bitmap is not evidence that the native renderer reproduces it.
4. Complete P3 by wiring the accepted P2 compositions to real state and actions.
   Keep all four Home destinations visible; destinations awaiting P4–P6 may show
   an explicit unavailable message. Never route an unrelated Menu action to the
   station list and count it as completed navigation.
5. Continue P4 onward using the same components. P7 refines an already faithful UI;
   it does not introduce the background, artwork, Hebrew or concept layout for
   the first time.

For each screen, record the reference panel, fixture/device evidence location,
known differences and reason, and separate **visual**, **functional**, and
**hardware** results in [implementation-status.md](implementation-status.md).
Use pass / fail / not verified, rather than a single vague “implemented.”
Visual similarity requires human inspection; a successful compile proves none of
it. Evidence must show the production render path, not just an asset preview.

P2 closes only when these native compositions meet the contract and font,
restoration, timing and memory checks in the guide pass. P3 closes only when that
appearance survives real metadata/input/web updates and device playback checks.
If hardware is unavailable, report code/fixture work as ready for device testing
and leave hardware acceptance open. Do not claim measurements or user approval
that did not occur. This evidence checkpoint does not require repeatedly asking
permission to continue already authorized implementation.
