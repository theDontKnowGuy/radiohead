# UI asset manifest

| Asset | Source | Build output | Use | Notes |
| --- | --- | --- | --- | --- |
| Sunset coast background | `docs/bg1.png` supplied by user | `.pio/ui_assets/background_320x240.png`, embedded C array | Base layer on concept fixtures | Source is 1448×1086 sRGB PNG; resampled directly to 320×240 (same 4:3 aspect ratio) with macOS `sips`. The build script emits a PNG byte array using `xxd`; LovyanGFX decodes it once for a full-page render. |
| Toned Home background | Same supplied `docs/bg1.png` | `assets/home/background.png` → `.pio/ui_assets/ui_home_assets.h` | Home only | Direct 4:3 Lanczos resize with no crop, saturation 0.80, then a uniform 25% black veil. The original source is retained. |
| Home tiles, focus, icons and weather | Original geometric recipes in `tools/prepare_home_assets.py` | `assets/home/*.png` → `.pio/ui_assets/ui_home_assets.h` | Reusable Home components | Four 68×70 restrained, darkened, slightly translucent gradient tiles; a 72×74 low-opacity focus mask; four 40×40 #E8EEF3 icons; 24×19 Wi-Fi; eight 64×56 weather assets. RGBA edges are prepared at 4× and filtered to native size. No text/clock is baked into these assets. |
| Home clock atlas | `tools/prepare_home_clock_atlas.py` with Inter Medium | `assets/home/clock_atlas.{bin,json}` → `.pio/ui_assets/ui_home_clock_atlas.h` | Home clock only | Twelve 22×44 glyph cells (0–9, colon, unavailable-state dash), rendered at 8×, cropped to actual ink bounds, baseline-aligned, Lanczos-downsampled, and alpha-strengthened for an opaque core. Digits use 20 px tabular advances, a 10 px colon, 33 px visible ink, and a 3 px opaque core; `14:37` measures 83×33 px. Firmware blends each alpha pixel with the current native RGB565 canvas, so its edges use the actual sunset rather than a fixed compositing color. |
| Episode player | `tools/prepare_podcast_assets.py` | `assets/podcast/*.png` → `.pio/ui_assets/ui_podcast_assets.h` | Recorded episode player | Direct 4:3 background with a 14–20% right-increasing black veil; a 115×115 rounded generic microphone/show image is used until actual local show artwork is supplied. |
| Recorded-player transport | User-supplied `icons/{rewind-15,pause,play,forward-30}.svg` | Native 58×58 replay and 72×72 play/pause PNGs → `.pio/ui_assets/ui_player_assets.h` | Recorded player controls | `tools/rasterize_svg.swift` uses AppKit to rasterize each SVG at its final displayed dimensions with alpha intact. This preserves the supplied icon geometry while avoiding a runtime SVG renderer. |
| List cards and right pager | `tools/prepare_list_assets.py` | `assets/lists/*.png` → `.pio/ui_assets/ui_list_assets.h` | Every list's rounded translucent rows and its Up/Down controls | 4× RGBA geometry, Lanczos-resampled at native size. Navy cards retain the sunset; focused cards use the same blue language. |

The background has no UI text or controls. It is source material only; production
labels, artwork placeholders, controls, and dark surfaces are drawn separately.
No generated header is committed. RGB565 ordering remains a physical-device check.

### Home asset regeneration

```sh
python3 tools/prepare_home_assets.py
python3 tools/prepare_home_clock_atlas.py --font /path/to/Inter-Medium.otf
```

### Episode-player asset regeneration

```sh
python3 tools/prepare_podcast_assets.py
```

### List surface regeneration

```sh
python3 tools/prepare_list_assets.py
```

The list recipe generates 248×46 cards for four-row paged lists, 304×42 cards for
compact option lists, 50×44 right-side pager controls, 57×190 and 57×142 translucent
rounded rails for four-row and Favorites three-row lists, and 44 px-wide page thumbs
in 84/42/28 px heights for one/two/three pages. Normal and focused cards share the same edge opacity so the focused first
row cannot visually widen its following gap. These assets contain
only translucent surface/border pixels—labels, chevrons, artwork, and favorite
state remain dynamic firmware drawing.

Requires Pillow only for asset preparation. The checked-in PNG assets and their
manifest are consumed by normal firmware builds without Pillow. The manifest
records sizes, Pillow version and source/output hashes; the build checks them.
All runtime drawing is at native dimensions. PNG transparency blends into the
readable PSRAM canvas, never the TFT. If that canvas is unavailable, Home uses
the opaque toned background and native primitive/bitmap fallback. Icons are
original programmatic geometry, not downloaded weather-provider artwork.

The Home background uses a uniform 25% black veil after modest desaturation. This
is artwork treatment, not white-balance compensation; it preserves the 4:3 source
composition while giving the type and controls clear priority.

## Smooth typography assets (2026-09-19)

`assets/fonts/{small,body,home_title,recorded_header,title,temperature,header_clock,label,caption}.vlw` are intentional font
assets, embedded by `tools/prepare_ui_assets.py`. Normal builds need neither a
system font nor Pillow. Generated C headers remain under `.pio/ui_assets`.

Current Latin source: Roboto Regular/Medium plus Roboto Condensed Regular
2.001101 (2014), from TeX Live's
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
The Home clock is intentionally excluded from the VLW roles; it uses the
separate native-size alpha atlas above. Compact headers use non-condensed Regular
at 18 px. Glyph coverage alone is not Hebrew bidi/niqqud support. The source OTFs/TTFs are
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
