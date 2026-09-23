"""Regenerate Home numeral atlases from Inter 4.1 outlines (never old bitmaps).

Requires Pillow 12.1.1. Download/extract the upstream Inter-4.1.zip release, then:
  python tools/prepare_home_clock_atlas.py --font-dir /path/to/extras/otf
Builds only embed the committed masks and require neither Pillow nor font files.
The original supplied v3 package remains untouched as historical source evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont, features, __version__

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'docs/ui/assets/home'
SCALE = 8
ADVANCE_SCALE = 10000


def generate(font_dir, role, size, weight, cell, baseline, anchor, thickening):
    source = font_dir / f'InterDisplay-{weight}.otf'
    font = ImageFont.truetype(str(source), size * SCALE)
    degree_size = 30 if role == 'temperature' else 0
    glyphs = '0123456789:-' if role == 'clock' else '0123456789°-'
    masks, advances = [], []
    for glyph in glyphs:
        face = font
        # A separate outline rasterization gives the degree a small, clean ring.
        if glyph == '°':
            face = ImageFont.truetype(str(source), degree_size * SCALE)
        mask = Image.new('L', (cell[0] * SCALE, cell[1] * SCALE))
        ImageDraw.Draw(mask).text((2 * SCALE, baseline * SCALE), glyph,
                                  font=face, fill=255, anchor='ls')
        if thickening:
            mask = mask.filter(ImageFilter.MaxFilter(2 * thickening + 1))
        mask = mask.resize(cell, Image.Resampling.LANCZOS)
        # Suppress only the faint Lanczos ringing outside the optical edge.
        mask = mask.point(lambda alpha: 0 if alpha < 8 else alpha)
        if glyph == '°':
            cropped = mask.crop(mask.getbbox())
            mask = Image.new('L', cell)
            # Match the mockup: the larger ring shares the digits' visible top
            # and leaves three clear pixels after the final numeral.
            mask.paste(cropped, (5, 6))
        masks.append(mask)
        advances.append(round(face.getlength(glyph) / SCALE * ADVANCE_SCALE))
    data = b''.join(mask.tobytes() for mask in masks)
    manifest = {
        'source': 'https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip',
        'font': source.name, 'font_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'license': 'SIL Open Font License 1.1; ../fonts/Inter-LICENSE.txt',
        'native_size_px': size, 'weight': weight,
        'pillow': __version__, 'freetype': features.version_module('freetype2'),
        'supersampling': SCALE, 'optical_thickening_px_per_side': thickening / SCALE,
        'glyphs': glyphs, 'alpha_bits': 8, 'cell_size': cell, 'baseline_y': baseline,
        'color': '#F5F5F5' if role == 'clock' else '#FFFFFF',
        'advance_scale': ADVANCE_SCALE, 'advance_units': advances,
        'tracking_units': 2500 if role == 'clock' else 0,
        'ink_anchor': anchor, 'bytes': len(data),
        'sha256': hashlib.sha256(data).hexdigest(),
        'recipe': 'Rasterize font outlines at 8x, apply specified optical weight, Lanczos downsample to native cells; discard alpha below 8. No existing atlas is scaled.',
    }
    if role == 'temperature':
        degree_mask = masks[glyphs.index('°')]
        degree_bounds = degree_mask.getbbox()
        manifest['degree'] = {'native_size_px': degree_size,
                              'ink_left_in_cell': degree_bounds[0],
                              'ink_top_in_cell': degree_bounds[1],
                              'intended_gap_after_digit_px': 3,
                              'runtime_character': '*',
                              'ink_size_px': list(degree_mask.crop(degree_bounds).size)}
    (OUTPUT / f'{role}_atlas.bin').write_bytes(data)
    (OUTPUT / f'{role}_atlas.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(role, {glyph: mask.getbbox() for glyph, mask in zip(glyphs, masks)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font-dir', type=Path, required=True)
    args = parser.parse_args()
    generate(args.font_dir, 'clock', 40, 'SemiBold', [36, 42], 36,
             {'right_x': 301, 'top_y': 40}, 3)
    generate(args.font_dir, 'temperature', 30, 'SemiBold', [28, 34], 29,
             {'left_x': 82, 'top_y': 70}, 0)


if __name__ == '__main__':
    main()
