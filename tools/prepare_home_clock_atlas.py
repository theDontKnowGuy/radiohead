"""Build the native Home-clock alpha atlas from Inter Medium.

Each source glyph is rendered at 8×, cropped to actual ink, aligned to a common
baseline and normalized into the requested tabular geometry before one final
Lanczos reduction to its native 8-bit alpha mask. Runtime never scales it.
"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, __version__


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "ui" / "assets" / "home"
GLYPHS = "0123456789:-"
SCALE = 8
SOURCE_SIZE = 48
TARGET_DIGIT_HEIGHT = 33
TARGET_DIGIT_ADVANCE = 20
TARGET_DIGIT_INK_WIDTH = 18
TARGET_COLON_ADVANCE = 10
CELL_WIDTH = 22
CELL_HEIGHT = 44
BASELINE = 38
RENDER_TOP = 39
RENDER_RIGHT = 280


def glyph_mask(font, glyph):
    """Return one high-resolution ink crop and its source baseline relation."""
    baseline = 56 * SCALE
    canvas = Image.new("L", (64 * SCALE, 72 * SCALE))
    ImageDraw.Draw(canvas).text((0, baseline), glyph, font=font, fill=255, anchor="ls")
    bounds = canvas.getbbox()
    if bounds is None:
        raise RuntimeError(f"No visible ink for Home-clock glyph {glyph!r}")
    return canvas.crop(bounds), bounds, baseline


def downsample_ink(crop, width, height):
    """Keep source design work at 8×, then make the final alpha only once."""
    high_size = (width * SCALE, height * SCALE)
    normalized = crop.resize(high_size, Image.Resampling.LANCZOS)
    return strengthen_alpha(normalized.resize((width, height), Image.Resampling.LANCZOS))


def strengthen_alpha(mask):
    """Preserve one anti-aliased edge pixel while restoring an opaque stem core."""
    def strengthen(alpha):
        if alpha < 18:
            return 0
        if alpha >= 224:
            return 255
        return min(255, round((alpha - 18) * 255 / 206))
    return mask.point(strengthen)


def ink_bounds(mask):
    bounds = mask.getbbox()
    if bounds is None:
        return [0, 0, 0, 0]
    return list(bounds)


def stem_width(mask):
    """Measure the left/right vertical stems of zero at its midline."""
    y = mask.height // 2
    row = [mask.getpixel((x, y)) >= 192 for x in range(mask.width)]
    runs, start = [], None
    for x, solid in enumerate(row + [False]):
        if solid and start is None:
            start = x
        elif not solid and start is not None:
            runs.append(x - start)
            start = None
    return min(runs) if runs else 0


def opaque_stem_width(mask):
    y = mask.height // 2
    row = [mask.getpixel((x, y)) == 255 for x in range(mask.width)]
    runs, start = [], None
    for x, solid in enumerate(row + [False]):
        if solid and start is None:
            start = x
        elif not solid and start is not None:
            runs.append(x - start)
            start = None
    return min(runs) if runs else 0


def sample_bounds(text, advances, bounds):
    pen = RENDER_RIGHT - sum(advances[GLYPHS.index(glyph)] for glyph in text)
    left, top, right, bottom = None, None, None, None
    for glyph in text:
        index = GLYPHS.index(glyph)
        x0, y0, x1, y1 = bounds[index]
        if x1 > x0 and y1 > y0:
            left = pen + x0 if left is None else min(left, pen + x0)
            top = RENDER_TOP + y0 if top is None else min(top, RENDER_TOP + y0)
            right = pen + x1 if right is None else max(right, pen + x1)
            bottom = RENDER_TOP + y1 if bottom is None else max(bottom, RENDER_TOP + y1)
        pen += advances[index]
    return [left, top, right, bottom, right - left, bottom - top]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True,
                        help="path to Inter-Medium.otf")
    args = parser.parse_args()
    if not args.font.is_file():
        raise RuntimeError(f"Missing required Home-clock source font: {args.font}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype(str(args.font), SOURCE_SIZE * SCALE)
    source_glyphs = {glyph: glyph_mask(font, glyph) for glyph in GLYPHS}
    digit_crops = [source_glyphs[glyph][0] for glyph in "0123456789"]
    source_width = max(crop.width for crop in digit_crops)
    source_height = max(crop.height for crop in digit_crops)
    # The 18 px widest ink with a 20 px tabular advance keeps Inter's figures
    # visibly broader than the preceding 38 px trial while leaving a crisp
    # unshared edge around each digit.
    scale_x = (TARGET_DIGIT_INK_WIDTH * SCALE) / source_width
    scale_y = (TARGET_DIGIT_HEIGHT * SCALE) / source_height
    alpha = bytearray()
    advances = []
    x_offsets, y_offsets, output_bounds = [], [], []
    for glyph in GLYPHS:
        source, bounds, source_baseline = source_glyphs[glyph]
        target_width = max(1, round(source.width * scale_x / SCALE))
        target_height = max(1, round(source.height * scale_y / SCALE))
        final = downsample_ink(source, target_width, target_height)
        advance = TARGET_COLON_ADVANCE if glyph == ":" else (
            TARGET_DIGIT_ADVANCE if glyph.isdigit() else max(1, round(target_width)))
        x_offset = (advance - target_width) // 2
        # The crop's lower edge is its source baseline relation; translate it
        # to the shared final baseline so all tabular digits sit identically.
        source_descent = source_baseline - bounds[3]
        y_offset = BASELINE - round(source_descent * scale_y / SCALE) - target_height
        if x_offset < 0 or x_offset + target_width > CELL_WIDTH or y_offset < 0 or y_offset + target_height > CELL_HEIGHT:
            raise RuntimeError(f"Home-clock glyph {glyph!r} does not fit its atlas cell")
        cell = Image.new("L", (CELL_WIDTH, CELL_HEIGHT))
        cell.paste(final, (x_offset, y_offset))
        alpha.extend(cell.tobytes())
        advances.append(advance)
        x_offsets.append(x_offset)
        y_offsets.append(y_offset)
        output_bounds.append(ink_bounds(cell))

    binary = OUTPUT / "clock_atlas.bin"
    binary.write_bytes(alpha)
    manifest = {
        "source": args.font.name,
        "source_sha256": hashlib.sha256(args.font.read_bytes()).hexdigest(),
        "pillow": __version__,
        "glyphs": GLYPHS,
        "scale": SCALE,
        "filter": "Lanczos",
        "alpha_bits": 8,
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "advances": advances,
        "x_offsets": x_offsets,
        "y_offsets": y_offsets,
        "ink_bounds": output_bounds,
        "measurements": {
            "digit_ink_height": TARGET_DIGIT_HEIGHT,
            "widest_digit_ink_width": TARGET_DIGIT_INK_WIDTH,
            "digit_advance": TARGET_DIGIT_ADVANCE,
            "colon_advance": TARGET_COLON_ADVANCE,
            "zero_vertical_stem": stem_width(Image.frombytes("L", (CELL_WIDTH, CELL_HEIGHT), bytes(alpha[:CELL_WIDTH * CELL_HEIGHT]))),
            "zero_opaque_stem": opaque_stem_width(Image.frombytes("L", (CELL_WIDTH, CELL_HEIGHT), bytes(alpha[:CELL_WIDTH * CELL_HEIGHT]))),
            "sample_10_17_bounds": sample_bounds("10:17", advances, output_bounds),
            "sample_14_37_bounds": sample_bounds("14:37", advances, output_bounds),
        },
        "bytes": len(alpha),
        "sha256": hashlib.sha256(alpha).hexdigest(),
        "recipe": "Inter Medium is rendered at 8x, cropped to source ink bounds, baseline-aligned, normalized to tabular geometry, Lanczos-downsampled, and alpha-strengthened for an opaque stem core.",
    }
    (OUTPUT / "clock_atlas.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
