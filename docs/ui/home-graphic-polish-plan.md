# Home graphic polish plan

## Scope

This is the tracked implementation plan for the Home-screen comparison feedback.
It applies to the production native renderer at 320 × 240, its reproducible
assets, and the production LovyanGFX fixture. It does not authorize changes to
audio, input, routes, persistence, weather fetching, or the supplied background
source image.

## Progress

| # | Instruction | Implementation state | Acceptance state |
| --- | --- | --- | --- |
| 1 | Calm the sunset with a 20–30% black overlay over the complete background. | Done: `prepare_home_assets.py` applies a uniform 25% black veil after modest desaturation. | Native fixture inspected: pass. Physical TFT pending. |
| 2 | Reduce the clock about 15–20% so it remains important but is not the main object. | Superseded by a fixed-size 34 px atlas: Regular non-condensed glyphs retain the current clock footprint with stable 2–3 px visual strokes. | Native fixture inspected: pass. Physical TFT pending. |
| 3 | Use lighter/thinner typography for Internet Radio, clock, weather temperature, and labels. | Done: Home title is 15 px regular/#E8EEF3; clock is a 34 px Roboto Regular alpha atlas/#F5F5F5; temperature is 27 px regular condensed; tile labels are 11 px medium. | Native fixture inspected: pass. Physical TFT pending. |
| 4 | Mute tile saturation/glow; remove the bright cyan blue outline and use a thin low-opacity border. | Done: all four tile gradients are darker/less saturated; their edge and focus border are low-opacity blue-gray. | Native fixture inspected: pass. Physical TFT pending. |
| 5 | Keep the intended header hierarchy: title upper-left; date then Wi-Fi upper-right; clock directly below date, with breathing room. | Done: date moves 4 px left, Wi-Fi 5 px right, and the 34 px clock is set below the date. | Native fixture inspected: pass. Physical TFT pending. |
| 6 | Treat weather as one compact block: icon left, temperature beside it, city then condition directly below. | Done: weather art moves 2 px right; text moves 4 px left; the condition caption is y=96. | Native fixture inspected: pass. Physical TFT pending. |
| 7 | Make every button identical in width, height, radius, border, vertical position, and spacing. | Done: strict 4-column grid: x=7,86,165,244; 68×70 tiles; y=154; 11 px gaps; shared prepared geometry. | Native geometry assertions: pass. Physical TFT pending. |
| 8 | Increase tile/background separation using a colored gradient over a 15–25% dark translucent body. | Done: prepared tile fills bake a 22% dark layer and retain slight transparency over the photo. | Native fixture inspected: pass. Physical TFT pending. |
| 9 | Reduce button-label size around 10–15%, especially Recorded Shows. | Done: label asset changes from 13 px to 11 px (15.4% smaller). | Native width/legibility assertions: pass. Physical TFT pending. |
| 10 | Match vertical spacing inside each button: icon in upper third, generous gap, label in lower third. | Done: shared 40 px icon canvas at y+4; labels move 4–5 px upward in the one shared tile renderer. | Native fixture inspected: pass. Physical TFT pending. |
| 11 | Soften icons and inactive text to light gray/white rather than pure white. | Done: prepared Home icons and runtime labels use #E8EEF3. | Native fixture inspected: pass. Physical TFT pending. |
| 12 | Scale the 4:3 supplied background directly to 320×240 with no crop. | Done: the asset recipe uses a direct 320×240 Lanczos resize; no crop/zoom stage exists. | Deterministic recipe and native framing: pass. Physical TFT pending. |

## 2026-09-21 implementation revision

The physical-screen comparison reopened the following prior choices. Home now
uses a 16% veil at 92% saturation, 72×70 tiles at `x=7/85/163/241`, `y=149`,
34 px icons, and a dedicated 11 px Home label font so list typography remains
unchanged. The tile gradients and borders are stronger category cues. The
brand mark is 20 px and the title uses the dedicated 18 px Home title font.
Weather city/condition baselines are `y=94/112`, while each weather asset's
visible ink aligns with the temperature at `y=68`; the source-matched dynamic
clock atlas is retained because its reference overlay already validates its
ink placement and alpha composition.

## Verification required

Run `python3 tools/prepare_home_assets.py`, regenerate the documented font
assets, then run `pio run -e esp32s3`, `python3 tools/render_ui_fonts.py`, and
`git diff --check`. Review `.pio/ui_native/home-smooth.png` at native 320×240
against `docs/ui/uiconcept.png`. The final visual and hardware acceptance remains
open until the resulting firmware is viewed on the physical TFT during normal
audio playback.

## 2026-09-21 verification record

The latest production firmware builds successfully: 66,388 B RAM (20.3%) and
2,787,287 B flash (42.5%). `tools/render_ui_fonts.py` passes its production
LovyanGFX geometry, text-width, hit-test, restoration, RGB565-transfer, and
anti-aliasing checks; the updated native Home image was inspected at 320×240.
`git diff --check` also passes. No device was flashed for this change, so the
physical TFT appearance, touch behavior, and sustained-audio repaint behavior
remain unverified.

## 2026-09-21 second-pass refinement

| Feedback | Progress | Native result |
| --- | --- | --- |
| Clock still too dominant | Done: 38 px regular condensed → 34 px regular condensed (10.5% smaller); moved lower and aligned below the date. | Pass; physical TFT pending. |
| Temperature too large | Done: 30 px regular condensed → 27 px regular condensed (10% smaller). | Pass; physical TFT pending. |
| Icons too small / labels too low | Done: icons are 40×40 (11.1% larger); label baselines move 4–5 px upward. The fallback primitives use the same 40 px composition. | Pass; physical TFT pending. |
| Tile gaps too tight | Done: shared grid changes from 70 px tiles / 8 px gaps to 68 px tiles / 11 px gaps. Every target remains 68×70 px. | Native hit-grid assertions pass; physical touch test pending. |
| Blue/green still bright | Done: blue and green source gradients are darkened a further ~10%; purple is unchanged. | Pass; physical TFT pending. |
| Header/weather groups need spacing | Done: date moves 4 px left, Wi-Fi 5 px right, clock moves below the date, weather art moves 2 px right, and weather text moves 4 px left. | Pass; physical TFT pending. |
| Title marginally heavy | Done: Home uses a dedicated 15 px regular font instead of the 16 px body face (6.25% smaller). | Pass; physical TFT pending. |

The second-pass build and checks are included in the verification record above.

## 2026-09-21 shared clock typography pass

| Feedback | Progress | Acceptance state |
| --- | --- | --- |
| Clock strokes are wider/heavier than the target. | Superseded: the 32 px Light Condensed cut was too narrow/thin on the RGB565 TFT. | Replaced by the non-condensed correction below. |
| Date/clock hierarchy and separation need refinement. | Done: the date is #D8DDE3, the clock #F5F5F5, and the clock origin moves 5 px lower for a clearer caption-to-clock gap. | Native fixture inspected: pass. Physical TFT pending. |
| A reusable visual change should apply to every screen. | Superseded: compact page clocks no longer use Light Condensed. | Replaced by the non-condensed correction below. |

The post-change production build passes at 66,556 B RAM (20.3%) and 2,981,083 B
flash (45.5%). `tools/render_ui_fonts.py` passes the LovyanGFX font, geometry,
restoration, and anti-aliasing checks (291 intermediate grayscale glyph pixels),
and `git diff --check` passes. The change has not been flashed, so physical TFT
appearance and sustained-audio repaint behavior remain pending.

## 2026-09-21 non-condensed clock correction

| Feedback | Progress | Acceptance state |
| --- | --- | --- |
| The large clock is too narrow and hairline-thin. | Superseded: Home no longer uses any VLW clock face. | Replaced by the fixed atlas below. |
| Avoid scaled smooth text. | Superseded: Home no longer uses a smooth-font clock draw. | Replaced by the fixed atlas below. |
| A TFT-safe shared clock treatment should apply across pages. | Done: compact opaque/list/Recorded Show clocks change from Light Condensed to 18 px non-condensed Roboto Regular, retaining their existing geometry and color. | Native Home and Recorded Show fixtures inspected: pass. Physical TFT pending. |

The corrected build passes at 66,556 B RAM (20.3%) and 2,986,375 B flash
(45.6%). `tools/render_ui_fonts.py` passes the LovyanGFX font, geometry,
restoration, and anti-aliasing checks (291 intermediate grayscale glyph pixels),
and `git diff --check` passes. The change has not been flashed, so physical TFT
appearance and sustained-audio repaint behavior remain pending.

## 2026-09-21 fixed Home-clock alpha atlas

| Feedback | Progress | Acceptance state |
| --- | --- | --- |
| Stop changing the Home clock between VLW families. | Done: Home no longer loads or draws a VLW clock; the unused `clock.vlw` role is removed. | Native fixture inspected: pass. Physical TFT pending. |
| Use an offline 4×, high-quality downsampled atlas. | Done: `prepare_home_clock_atlas.py` renders non-condensed Roboto Regular at 4×, Lanczos-downsamples to 24×34 8-bit alpha cells, and records exact source/output hashes. | Native alpha-mask assertions pass. Physical TFT pending. |
| Blend over actual background pixels with 2–3 px strokes. | Done: at runtime each atlas pixel alpha-blends #F5F5F5 with the current readable RGB565 Home canvas. The 34 px Regular source yields approximately 2–3 px visible stems without any runtime scaling. | Native Home fixture inspected: pass. Physical TFT pending. |

The atlas build passes at 66,476 B RAM (20.3%) and 2,989,887 B flash (45.6%).
`tools/render_ui_fonts.py` verifies the production native fixture, including more
than 100 non-binary atlas alpha pixels; `git diff --check` passes. Physical TFT
appearance and sustained-audio repaint behavior remain pending.

## 2026-09-21 Inter geometry trial — visual acceptance open

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Keep the alpha-mask/RGB565 composition path. | Kept unchanged; only the offline atlas recipe and test fixture changed. | Pass. |
| Inter Regular, 8× source, cropped ink, common baseline. | Done: Inter Regular source crops are baseline-aligned in 22×48 cells, then Lanczos-downsampled to 8-bit alpha. | Native fixture inspected. |
| Requested atlas geometry. | Done: digit ink height 38 px; zero stem 3 px; digit advance 20 px; colon advance 11 px; `09:58` layout width 91 px, ending at x=280. | Deterministic manifest measurement: pass. |
| Direct similarity to the concept clock. | The generated [09:58 fixture](../../.pio/ui_native/home-clock-0958.png) is retained for comparison. In the scaled contact-sheet reference, Home `14:37` has a 28 px ink height and 81 px width before mapping back to its native 320×240 panel. The 38×91 atlas trial is therefore intentionally **not accepted as visually matched** until the user reviews the native fixture/reference tradeoff. | Open; no physical TFT claim. |

## 2026-09-21 Inter Medium proportion correction

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Shorter, proportionally wider/heavier glyphs without changing runtime composition. | Done: only the atlas recipe changes to Inter Medium, 33 px digit ink, 18 px maximum digit ink width inside a 20 px tabular advance, and a 10 px colon. | Native fixture inspected: pass. Physical TFT pending. |
| Opaque core with restrained anti-aliasing. | Done: post-Lanczos alpha strengthening retains a 3 px fully opaque zero-stem core with a narrow anti-aliased edge transition. | Deterministic alpha measurement: pass. Physical TFT pending. |
| Final `14:37` ink bounding box. | Done: x=195…278, y=44…77, **83×33 px**; its logical right alignment remains x=280. | Native production [fixture](../../.pio/ui_native/home-clock-1437.png) inspected: pass. Physical TFT pending. |

The final atlas build passes at 66,492 B RAM (20.3%) and 3,150,355 B flash
(48.1%). `tools/render_ui_fonts.py` renders and preserves the native 320×240
fixture; `git diff --check` passes. This visual fixture meets the stated glyph
measurements, but physical TFT visual acceptance remains open.

## 2026-09-21 Final Home-clock geometry adjustment

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Reduce visible glyph height by approximately 4–5%. | Done: 32 px visible digit ink is the nearest whole-pixel result to the requested reduction from 33 px. The baseline and Home clock origin remain unchanged. | Native fixture: pass. Physical TFT pending. |
| Widen digits/advances by approximately 5%. | Done: tabular digit advance is 21 px (from 20 px); widest digit ink remains 18 px so the 3 px opaque Medium-weight stem is preserved rather than becoming heavier. | Deterministic atlas measurement: pass. |
| Preserve colon, right alignment, vertical position, font and composition. | Done: Inter Medium, 8× Lanczos source, alpha-mask/RGB565 blending, 10 px colon advance, `kRight = 280`, and `kTop = 39` are unchanged. | Source review: pass. |
| Render the requested native comparison. | Done: [14:37 fixture](../../.pio/ui_native/home-clock-1437.png) renders at 320×240. Its deterministic final ink box is x=191…277, y=45…77, **86×32 px**. | Native fixture inspected: pass. |

`tools/render_ui_fonts.py` passes with 291 fractional alpha pixels in the atlas;
`pio run -e esp32s3` passes at 82,268 B RAM (25.1%) and 3,215,795 B flash (49.1%),
and `git diff --check` passes. No device flash was performed, so visual acceptance
on the physical TFT remains pending.

## 2026-09-21 Home-clock weight and top-right anchor correction

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Increase visual weight without increasing height. | Done: only the Inter source face changes from Medium to installed Inter SemiBold (weight 600). Visible digit height remains 32 px; the final zero-stem opaque core is 4 px, giving a solid 3–4 px core with one-pixel anti-aliased edges. | Native fixture inspected. Physical TFT pending. |
| Re-anchor beneath the date and toward the right. | Done: atlas composition remains unchanged, but Home's anchor moves 6 px up and 5 px right, from `(280,39)` to `(285,33)`. | Source and fixture: pass. |
| Preserve all non-clock Home elements. | Done: date, weather, buttons, font family, background and runtime alpha/RGB565 path are untouched. | Scoped diff: pass. |
| Render and compare exact `14:37`. | Done: [native fixture](../../.pio/ui_native/home-clock-1437.png) has a measured ink box of x=196…282, y=39…71 (**86×32 px**). [Direct comparison](../../.pio/ui_native/home-clock-1437-comparison.png) places it beside the 320×240-normalized Home reference; their clock/date vertical relationship now aligns closely. | Native visual inspection: ready for user review; physical/reference acceptance remains open. |

## 2026-09-21 Home-clock geometry-only refinement

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Keep the current weight and rendering. | Done: Inter SemiBold source, 8× Lanczos atlas generation, alpha strengthening, 4 px opaque core, and runtime RGB565 blending are unchanged. | Scoped source review: pass. |
| Reduce height and widen figures/advance. | Done: visible digit ink is 31 px (32 → 31, 3.1% shorter); maximum digit ink is 19 px (18 → 19, 5.6% wider); tabular advance is 22 px (21 → 22, 4.8% wider—the nearest native-pixel result). | Deterministic atlas measurement: pass. |
| Reduce and space the colon. | Done: colon advance stays 10 px while its ink is reduced from 5 px to 4 px, giving a smaller mark and more surrounding whitespace. | Atlas bounds: pass. |
| Shift right without changing vertical placement. | Done: only the horizontal Home anchor moves x=285 → x=289; y=33 is unchanged. | Source and fixture: pass. |
| Test exact `14:37` against the mockup. | Done: [native fixture](../../.pio/ui_native/home-clock-1437.png) has final ink bounds x=197…286, y=40…71 (**89×31 px**). [Direct comparison](../../.pio/ui_native/home-clock-1437-comparison.png) is regenerated beside the normalized reference Home panel. | Native visual review complete; physical TFT pending. |

## 2026-09-21 Home hierarchy and native-comparison pass

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Clock geometry, mass and fixed-value evidence. | Done: current Inter SemiBold alpha atlas is 33 px high with 20 px widest digit body, a 3 px solid stem core and 87×33 px `14:37` ink bounds at x=217…304/y=42…75. The renderer is unchanged. | Native fixture: pass. Physical TFT pending. |
| Clock association with the top-right block. | Done: logical anchor is x=306/y=37; date/Wi-Fi remain above it and its visible top is y=42. | Native fixture: pass. |
| Product branding and current-station hierarchy. | Done: Home replaces `Radiohead>>` with a 16 px radio mark + regular `Radiohead`; dynamic station + `• Live Radio` is a dim subtitle. The competing center-right station label is removed. | Native fixture: pass. |
| Weather composition and hierarchy. | Done: the dynamic icon/text unit moves left/down; its 28 px Roboto Medium temperature is stronger, while city/condition remain secondary and aligned beneath it. | Native fixture: pass. |
| Equal, separated, restrained navigation tiles. | Done: all tiles use one 66×70 recipe, 13 px gaps, a common baseline/radius, 44 px higher icons, 10 px labels, and a further ~6% color-value reduction. | Native fixture: pass. Physical TFT pending. |
| Preserve neutral white treatment. | Done: no global white/white-balance change was made; Home icons retain #E8EEF3 and clock text retains #F5F5F5. | Scoped source review: pass. |
| Exact 320×240 test content and direct comparison. | Done: fixture fixes `Radiohead`, `NPR 24 • Live Radio`, `Mon, 21 Sep`, and `14:37`. [Side-by-side comparison](../../.pio/ui_native/home-polish-comparison.png) uses the native production renderer beside the normalized Home reference. | Native visual review: ready for user review. |

`pio run -e esp32s3` passes at 82,268 B RAM (25.1%) and 3,218,287 B flash
(49.1%); `tools/render_ui_fonts.py` and `git diff --check` pass. Hardware/physical
TFT acceptance remains open.

## 2026-09-23 weather typography follow-up

| Requirement | Result | Acceptance state |
| --- | --- | --- |
| Make the temperature one weight bolder. | Done: the dedicated 30 px Inter atlas moves from Medium to SemiBold without changing its size or top/left ink anchor. | Native fixture inspected: pass. Physical TFT pending. |
| Match the temperature-to-city and city-to-condition spacing. | Done: the city and condition origins move down 3 px to `y=97/115`; their measured visible-ink gaps are both 7 px. | Native pixel assertion and fixture: pass. Physical TFT pending. |

`pio run -e esp32s3` passes at 96,780 B RAM (29.5%) and 3,377,911 B flash
(51.5%). `tools/render_ui_fonts.py` and `git diff --check` pass. No device was
flashed, so physical TFT appearance remains unverified.
