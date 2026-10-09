"""Prepare the supplied Wi-Fi icon geometry and its reduced-arc variants.

The user's 612x612 reference is traced into three rounded cubic strokes and a
circle. Native assets use white ink on transparency, without the white square.
Run independently to avoid regenerating unrelated Home assets.
"""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw, __version__

ROOT = Path(__file__).resolve().parents[1]
SCALE = 16
SIZE = (24, 19)
# Outer, middle, inner curves in the supplied reference's coordinates.
CURVES = (
    ((99, 238), (213, 124), (399, 124), (513, 238)),
    ((159, 298), (241, 216), (371, 216), (453, 298)),
    ((219, 357), (267, 309), (345, 309), (393, 357)),
)
DOT = (306, 438, 46)
STROKE = 49
FIT = 22 / 463


def pixel(point):
    x, y = point
    return ((12 + (x - 306) * FIT) * SCALE, (1 + (y - 128) * FIT) * SCALE)


def wifi_image(arcs):
    image = Image.new('RGBA', (SIZE[0] * SCALE, SIZE[1] * SCALE))
    draw = ImageDraw.Draw(image)
    ink = (255, 255, 255, 255)
    radius = STROKE * FIT * SCALE / 2
    for curve in CURVES[3 - arcs:]:
        points = []
        for step in range(97):
            t = step / 96
            weights = ((1 - t)**3, 3 * (1 - t)**2 * t, 3 * (1 - t) * t**2, t**3)
            points.append(pixel(tuple(sum(w * p[axis] for w, p in zip(weights, curve))
                                      for axis in (0, 1))))
        draw.line(points, fill=ink, width=round(2 * radius), joint='curve')
        for x, y in (points[0], points[-1]):
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=ink)
    x, y = pixel(DOT[:2])
    r = DOT[2] * FIT * SCALE
    draw.ellipse((x - r, y - r, x + r, y + r), fill=ink)
    return image.resize(SIZE, Image.Resampling.LANCZOS)


def prepare_wifi_assets(out):
    out.mkdir(parents=True, exist_ok=True)
    for name, arcs in (('wifi', 3), ('wifi_fair', 2), ('wifi_weak', 1), ('wifi_dot', 0)):
        wifi_image(arcs).save(out / f'{name}.png', optimize=True)
    return {
        'reference': 'User-supplied 612x612 three-arc Wi-Fi icon, 2026-10-09; traced geometry',
        'recipe': 'Rounded cubic strokes and persistent circle; white ink on transparency; '
                  '24x19 at 16x supersampling, Lanczos; remove outer arcs without moving the dot',
        'pillow': __version__,
    }


if __name__ == '__main__':
    out = ROOT / 'docs/ui/assets/home'
    recipe = prepare_wifi_assets(out)
    path = out / 'manifest.json'
    manifest = json.loads(path.read_text())
    manifest['wifi_recipe'] = recipe
    for name in ('wifi', 'wifi_fair', 'wifi_weak', 'wifi_dot'):
        asset = out / f'{name}.png'
        manifest['assets'][name] = {
            'size': list(SIZE), 'sha256': hashlib.sha256(asset.read_bytes()).hexdigest(),
        }
    path.write_text(json.dumps(manifest, indent=2) + '\n')

