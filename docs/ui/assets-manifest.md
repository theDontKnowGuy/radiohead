# UI asset manifest

| Asset | Source | Build output | Use | Notes |
| --- | --- | --- | --- | --- |
| Sunset coast background | `docs/bg1.png` supplied by user | `.pio/ui_assets/background_320x240.png`, embedded C array | Base layer on concept fixtures | Source is 1448×1086 sRGB PNG; resampled directly to 320×240 (same 4:3 aspect ratio) with macOS `sips`. The build script emits a PNG byte array using `xxd`; LovyanGFX decodes it once for a full-page render. |
| Toned Home background | Same supplied `docs/bg1.png` | `assets/home/background.png` → `.pio/ui_assets/ui_home_assets.h` | Home only | Lanczos resize, saturation 0.84, smooth 16–23% navy veil; original source retained. |
| Home tiles, focus, icons and weather | Original geometric recipes in `tools/prepare_home_assets.py` | `assets/home/*.png` → `.pio/ui_assets/ui_home_assets.h` | Reusable Home components | Four 70×70 gradient tiles; 74×74 focus mask; four 36×36 icons; 24×19 Wi-Fi; eight 64×56 weather assets. RGBA edges are prepared at 4× and filtered to native size. No text/clock is baked into these assets. |

The background has no UI text or controls. It is source material only; production
labels, artwork placeholders, controls, and dark surfaces are drawn separately.
No generated header is committed. RGB565 ordering remains a physical-device check.

### Home asset regeneration

```sh
python3 tools/prepare_home_assets.py
```

Requires Pillow only for asset preparation. The checked-in PNG assets and their
manifest are consumed by normal firmware builds without Pillow. The manifest
records sizes, Pillow version and source/output hashes; the build checks them.
All runtime drawing is at native dimensions. PNG transparency blends into the
readable PSRAM canvas, never the TFT. If that canvas is unavailable, Home uses
the opaque toned background and native primitive/bitmap fallback. Icons are
original programmatic geometry, not downloaded weather-provider artwork.

The Home background veil uses RGB(8,17,37), smoothly moving from 23% at the top
to 16% around the horizon and 20% at the bottom. This is artwork treatment, not
white-balance compensation; the user confirmed physical white is correct.

## Smooth typography assets (2026-09-19)

`assets/fonts/{small,body,title,temperature,clock,label,caption}.vlw` are intentional font
assets, embedded by `tools/prepare_ui_assets.py`. Normal builds need neither a
system font nor Pillow. Generated C headers remain under `.pio/ui_assets`.

Current Latin source: Roboto Regular/Medium/Bold 2.001101 (2014), from TeX Live's
`opentype/google/roboto` directory, upstream <https://github.com/google/roboto>.
The actual OTF metadata identifies Apache License 2.0; see
`assets/fonts/LICENSE-Roboto.txt` and `NOTICE-Roboto.txt`. The Hebrew alphabet
uses DejaVu Sans regular 2.34 from TeX Live as a fallback, upstream
<https://dejavu-fonts.github.io/>. Its original Bitstream/DejaVu/Arev notices are
preserved in `assets/fonts/LICENSE-DejaVu.txt`.

`assets/fonts/manifest.json` records source/fallback hashes, Pillow/FreeType
versions, weights, pixel sizes, measured word-space advances and output hashes.
The build validates each VLW against that manifest. To regenerate:

```sh
python3 tools/prepare_ui_fonts.py --font-dir /path/to/roboto/otf --fallback-font-dir /path/to/dejavu/ttf
```

The VLW files store big-endian metrics and 8-bit grayscale glyph coverage, not
RGB/subpixel artwork. Small/body/title/label/caption include printable ASCII, the Hebrew
alphabet, degree, ellipsis and replacement symbols; numeric sizes contain the
digits, minus, colon and degree. The loader overrides VLW's estimated word space
with the font's measured advance; measurement and drawing use the same value.
Glyph coverage alone is not Hebrew bidi/niqqud support. The source OTFs/TTFs are
not embedded in firmware. Fonts are rasterized at their final sizes; there is no
runtime enlargement of a tiny bitmap.

Composition uses a 16-bit readable sprite. Its buffer stores `lgfx::swap565_t`;
the matching typed `pushImage` overload preserves channel/byte order. The native
fixture tests red, green and blue transfer, but physical ordering remains a
device check. Allocation failure keeps the previous bitmap-font drawing path.

After a firmware build, reproduce native C++ evidence with:

```sh
python3 tools/render_ui_fonts.py
```

This requires clang/clang++; Pillow is optional for PPM-to-PNG conversion. It
uses real LovyanGFX drawing into RAM with no SDL window or display readback.
Outputs remain in `.pio/ui_native`; inspected snapshots are copied to
`evidence/2026-09-19-fonts/` (first DejaVu pass) and
`evidence/2026-09-19-typeface/` (Roboto refinement). These are evidence, never
whole-screen firmware assets.

## Fixture builds

The normal build keeps production/controller behavior. To inspect a native P2
fixture, append one build define temporarily: `-DUI_P2_FIXTURE=1` (Home), `2`
(Stations), `3` (Live Player), `11` (Volume), or `16` (Confirmation). These all
use the production `display.cpp` component and asset path; no reference screenshot
is ever displayed as a page.
