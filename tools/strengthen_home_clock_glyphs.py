"""Apply the calibrated middle weight to the corrected Home-clock masks.

The source masks stay at their native dimensions.  Each alpha pixel becomes a
50/50 blend of its source coverage and a one-pixel max-alpha dilation, adding
stroke mass without scaling, moving, or expanding a glyph's visible bounds.
"""

import json
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "docs" / "ui" / "assets" / "home" / "home-clock-assets"
FILENAMES = [f"{glyph}.png" for glyph in "0123456789"] + ["colon.png", "hyphen.png"]
COLOR = (245, 245, 245)


manifest_path = PACKAGE / "manifest.json"
manifest = json.loads(manifest_path.read_text())
rendering = manifest["rendering"]
if "weight_adjustment" in rendering:
    raise RuntimeError("Clock source package already has a weight adjustment")

for name in FILENAMES:
    path = PACKAGE / "glyphs" / name
    source = Image.open(path).convert("RGBA")
    alpha = source.getchannel("A")
    strengthened = Image.blend(alpha, alpha.filter(ImageFilter.MaxFilter(3)), 0.5)
    result = Image.new("RGBA", source.size, COLOR + (0,))
    result.putalpha(strengthened)
    result.save(path)

rendering["antialiasing"] = (
    "Corrected native-scale source-matched masks with a calibrated 50% "
    "in-cell max-alpha expansion for the physical TFT's perceived weight."
)
rendering["weight_adjustment"] = {
    "method": "50% blend with one-pixel in-cell max-alpha dilation",
    "effect": "increases stroke mass without rescaling, moving, or expanding glyph bounds",
    "source_package": "corrected source-matched format-v2 package",
}
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
