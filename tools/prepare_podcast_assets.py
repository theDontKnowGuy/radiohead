"""Prepare the episode-player backdrop and generic show artwork at native size."""

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, __version__

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/ui/assets/podcast"
S = 4
OUT.mkdir(parents=True, exist_ok=True)


def box(values):
    return tuple(round(value * S) for value in values)


source = ROOT / "docs/bg1.png"
background = Image.open(source).convert("RGB").resize((320, 240), Image.Resampling.LANCZOS)
# Episode metadata sits over the upper/right skyline. Darken this player-specific
# treatment by 14–20%, increasing toward the text edge while retaining the photo.
for y in range(240):
    for x in range(320):
        alpha = 0.14 + 0.06 * x / 319
        pixel = background.getpixel((x, y))
        background.putpixel((x, y), tuple(round(component * (1 - alpha)) for component in pixel))
background.save(OUT / "background.png", optimize=True)

art = Image.new("RGBA", (115 * S, 115 * S))
draw = ImageDraw.Draw(art)
mask = Image.new("L", art.size)
ImageDraw.Draw(mask).rounded_rectangle(box((0, 0, 114.75, 114.75)), radius=box((8,))[0], fill=255)
for y in range(115 * S):
    ratio = y / (115 * S - 1)
    top, bottom = (30, 74, 100), (8, 30, 48)
    color = tuple(round(a + (b - a) * ratio) for a, b in zip(top, bottom)) + (255,)
    draw.line((0, y, 115 * S, y), fill=color)
art.putalpha(mask)

# A neutral show/microphone mark: recognizably audio artwork without claiming a
# show-specific image. Thin blue radio waves keep the portrait-like tile alive.
muted = (232, 238, 243, 255)
blue = (102, 189, 242, 210)
for radius in (29, 37):
    draw.arc(box((57 - radius, 55 - radius, 57 + radius, 55 + radius)), 220, 320, fill=blue, width=box((1.5,))[0])
    draw.arc(box((57 - radius, 55 - radius, 57 + radius, 55 + radius)), 40, 140, fill=blue, width=box((1.5,))[0])
draw.rounded_rectangle(box((46, 27, 68, 65)), radius=box((10,))[0], fill=muted)
draw.rounded_rectangle(box((50, 31, 64, 61)), radius=box((6,))[0], fill=(113, 155, 179, 255))
draw.arc(box((37, 39, 77, 79)), 20, 160, fill=muted, width=box((3,))[0])
draw.line((57 * S, 79 * S, 57 * S, 91 * S), fill=muted, width=box((3,))[0])
draw.line((43 * S, 91 * S, 71 * S, 91 * S), fill=muted, width=box((3,))[0])
draw.rounded_rectangle(box((.6, .6, 114.1, 114.1)), radius=box((7.5,))[0], outline=(202, 225, 239, 75), width=S)
art.resize((115, 115), Image.Resampling.LANCZOS).save(OUT / "artwork_placeholder.png", optimize=True)

assets = {}
for path in sorted(OUT.glob("*.png")):
    with Image.open(path) as image:
        assets[path.stem] = {"size": list(image.size), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
(OUT / "manifest.json").write_text(json.dumps({
    "pillow": __version__,
    "background_source": "docs/bg1.png",
    "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "recipe": "direct 320x240 resize; 14–20% black episode-player veil increasing to the right; 115px rounded generic microphone/show graphic at 4x",
    "assets": assets,
}, indent=2) + "\n")
