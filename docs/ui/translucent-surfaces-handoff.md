# Translucent station-list surfaces: implementation handoff

Prepared 2026-09-20. **Instructions only; this task implements no firmware changes.**
The user's latest example shows a sunset photograph visible through rounded navy
rows, a stronger translucent blue selected row, thin blue borders, and opaque
white labels/artwork. Reproduce that surface treatment at the native 320 × 240
size. The photograph itself remains opaque; the surfaces over it have alpha.

## Feasibility and evidence

This is supported by the current rendering architecture:

- `src/display.cpp` already allocates a 320 × 240 RGB565 PSRAM sprite in
  `initCanvas()` (153,600 bytes), exposes it through `canvas()`, and sends finished
  pixels to the TFT through `presentCanvas()` in eight-row stripes.
- Transparent Home PNG icons already blend onto this readable sprite. Installed
  LovyanGFX 1.2.21's `LGFXBase.cpp`, `png_draw_alpha_callback()`, reads destination
  pixels and blends PNG alpha. No new display library or RGBA framebuffer is needed.
- `renderStations()` restores `drawBackground()` before each render. Its focused
  row currently has an opaque blue fill; unfocused rows have only dividers.
- `docs/bg1.png` is the supplied background consumed by `tools/prepare_ui_assets.py`.
  Older prose saying the background is still missing is stale. Preserve this asset
  and Home's separate prepared background.

This establishes a feasible software path, not measured redraw performance or
physical visual acceptance. The TFT receives opaque RGB565 pixels; never rely on
physical-panel readback to obtain the pixels needed for alpha blending.

## Scope and prerequisites

Read `AGENTS.md`, the required `radiohead-firmware` and `typesafe-ai` skills,
`visual-contract.md`, `agent-implementation-guide.md`, and the latest entries in
`implementation-status.md`. Inspect `uiconcept.png` and the user's newer example
if available in the implementing session. Inspect status and overlapping diffs:
there are existing uncommitted input, renderer, fixture and documentation changes.

Start with Live Stations only. The newer user-approved direction supersedes the
earlier footer controls: use four 46 px rows distributed through the former
footer area, independent favorite targets, and
a right-side translucent rail with top Up and bottom Down targets. Do not copy the
example's six-row density or floating Edit control. Keep artwork, text,
favorite state, navigation, playback, and focus semantics unchanged. In particular,
the current blue highlight follows `stationFocus`; do not silently make it follow
the playing station. Other pages can adopt the component in a separate change.

All blending and restoration are deterministic. Visual comparison requires human
judgment; Jev is unnecessary for this change. No AI dependency, credential or live
judgment is needed. Live TypeSafe documentation could not be fetched during this
planning task; the local skill's design guidance was used.

## Recommended implementation

1. Prepare two small reusable RGBA PNG surfaces off-device: normal navy and focused
   blue. Match the existing station body rectangle: **304 × 46 px, radius 7**, drawn
   at `(8, rowY + 1)`. Keep pixels outside rounded corners fully transparent.
   Prepare anti-aliased edges and any restrained vertical gradient off-device.
   Starting values for visual tuning: navy `#10243C` at 25–40% opacity; focused
   blue `#087BEA` at 65–80%. These are proposed values, not sampled or approved
   colors. Alpha 0 is transparent and alpha 255 is opaque. Keep the sunset visible.
2. Add a reproducible asset-generation recipe and manifest under the existing
   `tools/` and `docs/ui/assets/` conventions. Record color, opacity, dimensions,
   source/provenance and hashes. Extend `tools/prepare_ui_assets.py` to emit a
   generated header under `.pio/ui_assets/`. Do not bake the photo, labels, logos,
   favorites or selected station into a screenshot asset. Do not commit generated
   build headers or firmware binaries.
3. In `src/display.cpp`, add one reusable surface painter with a focused/normal
   variant. When `uiFrameReady`, draw the RGBA asset through `canvas().drawPng()`;
   blend directly over the restored background in the existing sprite. Draw a
   subtle normal outline and brighter blue/cyan focus outline, then artwork,
   text and stars. Keep labels and artwork fully opaque. Do not apply alpha to a
   completed row containing text. Replace the current focused `fillRoundRect`
   and unfocused divider branch in `renderStations()` with this painter.
4. Guard the memory-canvas path explicitly: `canvas()` returns the physical TFT
   when initialization fails. For `!uiFrameReady`, retain legible opaque native
   fills/borders and bitmap text. Also handle `drawPng()` failure with an opaque
   fallback over the same rectangle. Never attempt panel alpha/readback as a
   fallback. Preserve the existing sprite transfer's `lgfx::swap565_t` handling.
5. Keep the header photograph visible. A subtle header tint is optional only if
   native-size inspection demonstrates a legibility need. If included, draw it
   once after background restoration and before header text. Keep Previous/Next
   geometry and behavior; any matching footer surface is secondary to the rows.

Use the existing PNG alpha implementation first. If its measured cost is too
high, precompose row backgrounds off-device against the photo for each of the
three fixed row positions and both surface states. Such opaque crops must match
their screen coordinates and background version; a crop from the first row cannot
be reused at another vertical position. Keep text and controls dynamic. Avoid a
custom per-pixel blender or additional full-frame cache unless measurements justify it.

## Restoration and audio constraints

- Draw in this order: clean photo → translucent surface → border → artwork/text
  → favorite/focus indication → any overlay. Applying alpha repeatedly onto an
  already composited row progressively changes its color and is incorrect.
- Start with the existing event-driven full composition. A focus change, paging,
  favorite toggle or title update must rebuild from the clean photo. The current
  `src/main.cpp` clock path already redraws all of Live Stations; preserve that.
  Do not route its clock through the generic opaque `drawHeaderClock()` path.
- If dirty rectangles are added, restore the complete old/new content area and
  reconstruct every intersecting layer, including rounded edges. Overlay dismissal
  must restore current underlying state. Do not erase text with a solid rectangle.
- Keep the existing transfer stripes and audio servicing. Service audio between
  background/row composition steps as needed, and measure decode/composition gaps
  as well as transfer gaps. Keep web/input servicing responsive; do not call
  reentrant handlers from inside painting. Do not add continuous full-frame
  animation, runtime blur, runtime asset scaling or network asset fetching.
- Reuse the existing 150 KiB canvas. Account separately for PNG decoder scratch,
  runtime heap/PSRAM and flash. Do not infer runtime allocation costs from the
  PlatformIO static RAM report or put large buffers on the loop stack.

## Verification and completion

Extend the existing production-render fixture in `tools/native_ui/home.cpp` and
`tools/render_ui_fonts.py`; do not create an independent mock renderer. Ensure any
new helper/header is included in the harness's extracted production code and
dependency list. Verify:

- Background detail remains visible inside both surface variants, rounded corners
  expose the unchanged background, and labels/stars remain sharp and readable.
- Switching focus A → B → A, repeated identical renders, favorite toggles, paging,
  long-to-short titles and clock changes produce the same final pixels as a fresh
  render of the same state, with no cumulative tint or stale content.
- Normal/focused/favorite-focused states, empty lists, missing artwork, and existing
  Hebrew/mixed-text examples render without regressions. Report existing Hebrew
  layout limitations separately; this surface change does not resolve them.
- Allocation/asset failure fallback is legible and does not request TFT readback.
  Existing hit-target tests still pass; visual surfaces do not alter hit regions.

Run `pio run -e esp32s3`, `python3 tools/render_ui_fonts.py`, and `git diff --check`.
Record RAM/flash deltas, inspect native **320 × 240** production images against the
reference, and retain evidence under `docs/ui/evidence/` using project conventions.
Inspect the final diff for unrelated edits, secrets and generated artifacts.

On hardware, verify contrast and color over the sunset, rapid focus/paging/favorite
updates, minute rollover, touch/encoder response, and sustained streaming without
new audio interruptions. Measure composition/transfer time, longest audio-service
gap, free/largest internal heap blocks and PSRAM usage against the baseline.
Record visual, functional and hardware results separately in
`implementation-status.md`. If hardware is unavailable, leave those checks open;
neither feasibility nor a passing build closes P2/P3 acceptance.
