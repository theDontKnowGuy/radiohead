"""Rebuild the Home-clock source package with the approved heavier native weight.

This is a one-time, deterministic conversion from the corrected format-v2
glyph masks.  It expands each alpha mask by exactly one 320x240 logical pixel
and regenerates every composition reference from those masks.  The renderer
therefore receives masks only; no captured background or literal clock string
can leak into a dynamic time update.
"""

import json
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "docs" / "ui" / "assets" / "home" / "home-clock-assets"
GLYPHS = "0123456789:-"
FILENAMES = {**{glyph: f"{glyph}.png" for glyph in "0123456789"},
             ":": "colon.png", "-": "hyphen.png"}
COLOR = (245, 245, 245)


def bounds(image):
    result = image.getchannel("A").getbbox()
    if result is None:
        raise RuntimeError("Clock glyph unexpectedly has no visible pixels")
    return list(result)


def build_cell(image, glyph):
    """Return the source glyph placed in its padded atlas cell."""
    spec = manifest["glyph_cells"]["glyphs"][glyph]
    cell = Image.new("RGBA", (cell_width, cell_height))
    cell.alpha_composite(image, (spec["draw_x_from_cell_left"],
                                 spec["draw_y_from_cell_top"]))
    return cell


def compose(text):
    width = sum(manifest["glyph_cells"]["glyphs"][glyph]["advance"] for glyph in text)
    result = Image.new("RGBA", (width, cell_height))
    pen = 0
    for glyph in text:
        result.alpha_composite(cells[glyph], (pen, 0))
        pen += manifest["glyph_cells"]["glyphs"][glyph]["advance"]
    return result


manifest_path = PACKAGE / "manifest.json"
manifest = json.loads(manifest_path.read_text())
if manifest.get("format_version") != 2:
    raise RuntimeError("This converter only accepts the corrected, unweighted v2 package")

# Rebuild every individual dynamic glyph before recomposing any acceptance
# images.  A max-alpha filter is a true one-pixel alpha dilation: it thickens
# strokes without changing color, rendering text, or baking a time into an
# image.
for glyph in GLYPHS:
    path = PACKAGE / "glyphs" / FILENAMES[glyph]
    original = Image.open(path).convert("RGBA")
    padded = Image.new("RGBA", (original.width + 2, original.height + 2))
    padded.alpha_composite(original, (1, 1))
    alpha = padded.getchannel("A").filter(ImageFilter.MaxFilter(3))
    rebuilt = Image.new("RGBA", padded.size, COLOR + (0,))
    rebuilt.putalpha(alpha)
    rebuilt.save(path)

    spec = manifest["glyph_cells"]["glyphs"][glyph]
    spec["cell_size"] = list(rebuilt.size)
    spec["draw_x_from_cell_left"] = 1
    spec["draw_y_from_cell_top"] = spec.get("draw_y_from_cell_top", 0) + 1
    spec["ink_bounds_in_cell"] = bounds(rebuilt)

cells_data = manifest["glyph_cells"]
cell_width = max(spec["cell_size"][0] + spec["draw_x_from_cell_left"]
                 for spec in cells_data["glyphs"].values()) + 1
cell_height = max(spec["cell_size"][1] + spec["draw_y_from_cell_top"]
                  for spec in cells_data["glyphs"].values()) + 1
cells_data["width"] = cell_width
cells_data["height"] = cell_height
cells_data["baseline_y"] = 30

cells = {}
for glyph in GLYPHS:
    image = Image.open(PACKAGE / "glyphs" / FILENAMES[glyph]).convert("RGBA")
    cells[glyph] = build_cell(image, glyph)

anchor = manifest["home_clock_ink_anchor"]
for text, details in manifest["reference_strings"].items():
    reference = compose(text)
    reference.save(PACKAGE / details["file"])
    ink = bounds(reference)
    details["canvas_size"] = list(reference.size)
    details["ink_bounds"] = ink
    details["intended_ink_bounds_at_home_anchor"] = [
        anchor["right_x"] - ink[2], anchor["top_y"], anchor["right_x"],
        anchor["top_y"] + ink[3] - ink[1],
    ]

reference = compose("14:37")
ink = bounds(reference)
overlay = Image.new("RGBA", (320, 240))
overlay.alpha_composite(reference, (anchor["right_x"] - ink[2],
                                    anchor["top_y"] - ink[1]))
overlay.save(PACKAGE / manifest["acceptance"]["overlay"])
manifest["acceptance"]["overlay_ink_bounds"] = [
    anchor["right_x"] - ink[2], anchor["top_y"], anchor["right_x"],
    anchor["top_y"] + ink[3] - ink[1],
]
manifest["rendering"]["antialiasing"] = (
    "Corrected native-scale masks are expanded outward by one 320x240 logical "
    "pixel with max-alpha dilation, preserving their source outline and "
    "anti-aliased edge coverage."
)
manifest["rendering"]["weight_adjustment"] = {
    "method": "one-pixel max-alpha dilation",
    "reason": "Match the visibly denser clock strokes in the supplied mockup without scaling its layout.",
    "source_package": "corrected source-matched format-v2 package",
}
manifest["format_version"] = 3
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

print(f"Rebuilt {len(GLYPHS)} clock glyphs as {cell_width}x{cell_height} atlas cells")
