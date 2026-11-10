# TFT visual rules

Use [uiconcept.png](uiconcept.png) as the design reference for the 320×240 ILI9341.
Keep the **coastal sunset, translucent-looking navy surfaces, white text, blue active
states and artwork**. The old cream/olive prototype is superseded.
Current feature removals and encoder behavior come from the [controls spec](configuration-and-controls-plan.md).

| Screen | Keep recognizable |
| --- | --- |
| Home | Weather left, clock right; four labelled tiles: blue Live Radio, green Recorded Shows, purple Favorites, slate Settings |
| Lists | Back/title/time/Wi-Fi, artwork and primary/secondary text, selected blue row, separate favorite; four 46 px rows and right paging rail |
| Player | Artwork and metadata, clear real state and touch transport; knob owns volume/mute |
| Volume | Centered navy floating panel, speaker, blue setting bar/value; no runtime blur |
| Confirmation | Light rounded dialog, Cancel first, exact action named |

Use real values or honest unavailable states; sample frequencies/names/weather
are not requirements. Preserve Hebrew/Latin/numbers with native text layout.
Missing artwork gets a consistent placeholder. Do not replace the composition
with a text-only player or put the old layout over a photo and call it complete.

Targets must be at least 44×44, non-overlapping. Reduce list density or secondary
copy before shrinking text. Keep rendering/hit testing in the same coordinates.
Prepare photo tint, softening and assets off-device; restore photo beneath changed
regions. RGB565 may need simpler details, but not a different visual direction.

## Clean background handoff

The supplied source is now `docs/bg1.png`; older “background missing” notes are stale.
Keep the original and reproducible 320×240 derivatives. Preserve visible coastal
detail, with navy treatment for legibility. Images contain no baked clock/text/buttons;
these remain dynamic. Record crop/tint/format/provenance in the [asset notes](assets-manifest.md).
A temporary navy fallback must not be counted as visual acceptance.

## Corrected implementation sequence and evidence

1. Keep useful input/playback work; finish missing P0/P1 measurements.
2. P2 supplies production C++ Home/list/player/volume/dialog fixtures with background,
   artwork, Hebrew, long/missing text, favorite/selection and connecting/error states.
3. Inspect at native 320×240 against reference panels; enlarged images are additional aids.
4. P3 wires the same components to real state. Keep all four Home destinations and
   truthful unavailable states. P7 refines this work; it does not introduce the design.

Record **visual, functional and hardware** results separately in [status](implementation-status.md),
with evidence path and reason for deviations. A browser mockup, build or whole-screen
reference bitmap does not prove native rendering. P2/P3 remain partial until required
physical appearance, input, memory/timing and sustained-audio checks are recorded.
