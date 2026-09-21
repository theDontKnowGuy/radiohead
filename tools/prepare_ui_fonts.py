"""Rebuild grayscale VLW font assets with Pillow / FreeType.

Usage: python tools/prepare_ui_fonts.py --font-dir /path/to/roboto/otf \
    --fallback-font-dir /path/to/dejavu/ttf
Normal firmware builds embed the checked-in VLW assets and need no Pillow/font installation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont, features, __version__


def generate(font_path, size, characters, fallback_path):
    font = ImageFont.truetype(str(font_path), size)
    fallback = ImageFont.truetype(str(fallback_path), size)
    ascent, descent = font.getmetrics()
    records, masks = [], []
    for code in characters:
        character = chr(code)
        # Roboto's Latin font has no Hebrew; preserve the previous glyph coverage.
        face = fallback if 0x5D0 <= code <= 0x5EA else font
        left, top, right, bottom = face.getbbox(character, anchor='ls')
        width, height = right - left, bottom - top
        mask = Image.new('L', (width, height))
        ImageDraw.Draw(mask).text((-left, -top), character, font=face, fill=255, anchor='ls')
        advance = round(face.getlength(character))
        records.append(struct.pack('>7i', code, height, width, advance, -top, left, 0))
        masks.append(mask.tobytes())
    data = struct.pack('>6i', len(records), 11, size, 0, ascent, descent) + b''.join(records + masks)
    return data, round(font.getlength(' '))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font-dir', type=Path, required=True)
    parser.add_argument('--fallback-font-dir', type=Path, required=True)
    args = parser.parse_args()
    output = Path(__file__).resolve().parents[1] / 'docs/ui/assets/fonts'
    manifest = {'family': 'Roboto 2.001101 (2014)', 'pillow': __version__,
                'freetype': features.version_module('freetype2'), 'fonts': {}}
    alphabet = sorted(set(range(33, 127)) | set(range(0x5D0, 0x5EB)) | {0xB0, 0x2026, 0xFFFD})
    roles = [('small', 11, 'Roboto-Medium.otf', 'Medium'),
             ('body', 16, 'Roboto-Regular.otf', 'Regular'),
             ('title', 22, 'Roboto-Medium.otf', 'Medium'),
             ('temperature', 32, 'RobotoCondensed-Bold.otf', 'Bold Condensed'),
             ('clock', 46, 'RobotoCondensed-Bold.otf', 'Bold Condensed'),
             ('label', 13, 'Roboto-Medium.otf', 'Medium'),
             ('caption', 13, 'Roboto-Regular.otf', 'Regular')]
    for name, size, source_name, weight in roles:
        source = args.font_dir / source_name
        fallback = args.fallback_font_dir / 'DejaVuSans.ttf'
        chars = alphabet if size <= 22 else sorted(map(ord, '-0123456789:°'))
        data, space = generate(source, size, chars, fallback)
        (output / f'{name}.vlw').write_bytes(data)
        manifest['fonts'][name] = {
            'source': source.name, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'fallback': fallback.name, 'fallback_sha256': hashlib.sha256(fallback.read_bytes()).hexdigest(),
            'pixels': size, 'weight': weight, 'space_width': space,
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
