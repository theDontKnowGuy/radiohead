# UI assets and regeneration

Sources stay in `docs/ui/assets`, `docs/ui/icons` and `docs/bg1.png`.
Generated headers/build images stay in `.pio/ui_assets`; do not commit them.
Manifests beside assets record source/output hashes, dimensions, versions and licensing.

| Asset | Recipe / output / details |
| --- | --- |
| Coastal photo | `prepare_ui_assets.py`: supplied 1448×1086 `docs/bg1.png` → 320×240 PNG, no baked controls |
| Home | `prepare_home_assets.py`: direct 4:3 resize, saturation .92, 16% veil; four 74×74 tiles, 38×38 icons, 20 px brand, 24×19 Wi-Fi, eight 64×56 weather assets |
| Clock | `prepare_home_clock_atlas.py`: Inter Display SemiBold 4.1, 40 px raster/36×42 straight-alpha cells; committed `assets/home/clock_atlas.{bin,json}` embedded by ordinary builds |
| Recorded player | `prepare_podcast_assets.py`: 14–20% veil and 115×115 generic show artwork until real artwork exists |
| Transport/header | SVGs rasterized with `rasterize_svg.swift` at 58×58 side/72×72 central transport, 24×24 Back; supplied Wi-Fi PNGs |
| Lists | `prepare_list_assets.py`: 248×46 paged cards, 304×42 compact cards, 50×44 pager, 57×190/142 rails, 44 px thumbs with 84/42/28 heights |
| Fonts | `prepare_ui_fonts.py`: committed VLW roles/metrics; Roboto 2.001101 Latin, DejaVu Sans 2.34 Hebrew fallback; clock uses separate atlas |

Normal builds need Pillow, macOS `sips`, `xxd` and Swift for the asset pipeline,
but no installed source fonts. Regeneration commands from the project root:

```sh
python3 tools/prepare_home_assets.py
python3 tools/prepare_home_clock_atlas.py --font-dir /path/to/Inter-4.1/extras/otf
python3 tools/prepare_podcast_assets.py
python3 tools/prepare_list_assets.py
python3 tools/prepare_ui_fonts.py --font-dir /path/to/roboto/otf --fallback-font-dir /path/to/dejavu/ttf
```

Keep `assets/fonts/Inter-LICENSE.txt`, `LICENSE-Roboto.txt`, `NOTICE-Roboto.txt`
and `LICENSE-DejaVu.txt`. VLW holds big-endian metrics and grayscale glyph coverage;
loader uses measured word-space advances. Hebrew glyphs alone do not implement bidi/niqqud.
The older `home-clock-assets/` package is historical, not a regeneration input.

## Runtime and checks

The readable 16-bit sprite uses `lgfx::swap565_t` and matching typed `pushImage`.
No alpha blending reads the physical TFT. Missing canvas uses opaque primitives/
bitmap fallback; fonts are native size, never enlarged at runtime.

November 9 crashes identified unchecked LovyanGFX alpha-PNG DMA allocation.
Production now draws **40 predecoded flash RGBA assets** without allocations:
11 transport/Back/Wi-Fi, 18 Home and 11 list surfaces. Build-time Pillow decoding
keeps exact PNG pixels, limits geometry to 320×240/run counts to 16 bits.
Opaque photos retain PNG. Clipping/readable/fallback drawing was checked under
ASan/UBSan with heap forbidden, and pixel-compared on four RGB565 backgrounds.

```sh
python3 tools/check_ui_alpha_icons.py
python3 tools/render_ui_fonts.py
```

Native harness needs clang/clang++; Pillow converts PPM to PNG. Outputs go to
`.pio/ui_native`, retained evidence to `evidence/`. It uses production C++ drawing,
not a mock screen. Physical colors, text and sustained repaint/audio remain device checks.
`UI_P2_FIXTURE` values 1/2/3/11/16 select Home/Stations/Live/Volume/Confirmation
for deliberate development builds; ordinary firmware keeps real behavior.
