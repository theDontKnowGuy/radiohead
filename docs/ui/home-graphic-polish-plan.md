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
| 2 | Reduce the clock about 15–20% so it remains important but is not the main object. | Done: 46 px bold condensed → 34 px regular condensed. The first reduction was 17.4%; the later feedback pass reduced the new size another 10.5%. | Native fixture inspected: pass. Physical TFT pending. |
| 3 | Use lighter/thinner typography for Internet Radio, clock, weather temperature, and labels. | Done: Home title is 15 px regular/#E8EEF3; clock is 34 px regular condensed; temperature is 27 px regular condensed; tile labels are 11 px medium. | Native fixture inspected: pass. Physical TFT pending. |
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
