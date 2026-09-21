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
