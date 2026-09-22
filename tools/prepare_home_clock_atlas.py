"""Convert the approved native Home-clock glyph package into an alpha atlas.

The supplied PNG masks are the design source of truth. They are copied into
the firmware's compact 8-bit alpha atlas without rescaling or font rendering.
"""

import hashlib
import json
from pathlib import Path
import sys

from PIL import Image, __version__


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "ui" / "assets" / "home"
SOURCE = OUTPUT / "home-clock-assets"
GLYPHS = "0123456789:-"
FILENAMES = {**{glyph: f"{glyph}.png" for glyph in "0123456789"},
             ":": "colon.png", "-": "hyphen.png"}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def alpha_bounds(image):
    bounds = image.getchannel("A").getbbox()
    return list(bounds) if bounds else [0, 0, 0, 0]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def compose(text, images, advances, cell_height, right, top):
    # A glyph can intentionally overhang its advance.  Size the temporary
    # composition for that visible final overhang instead of clipping it at
    # the sum of advances (for example the right edge of 18:30's final zero).
    pen = 0
    visible_right = 0
    for glyph in text:
        visible_right = max(visible_right, pen + alpha_bounds(images[glyph])[2])
        pen += advances[GLYPHS.index(glyph)]
    width = max(pen, visible_right)
    result = Image.new("RGBA", (width, cell_height))
    pen = 0
    for glyph in text:
        result.alpha_composite(images[glyph], (pen, 0))
        pen += advances[GLYPHS.index(glyph)]
    overlay = Image.new("RGBA", (320, 240))
    overlay.alpha_composite(result, (right - width, top))
    return result, overlay


def main():
    refresh_references = "--refresh-references" in sys.argv[1:]
    source_manifest_path = SOURCE / "manifest.json"
    require(source_manifest_path.is_file(), f"Missing approved Home-clock package: {source_manifest_path}")
    source_manifest = json.loads(source_manifest_path.read_text())
    require(source_manifest.get("format_version") in (2, 3), "Unsupported Home-clock package format")
    require(source_manifest.get("canvas") == {"width": 320, "height": 240, "units": "pixels"},
            "Home-clock package must target exactly 320x240")
    require(source_manifest.get("clock_color", {}).get("hex") == "#F5F5F5",
            "Unexpected Home-clock color")

    cells = source_manifest["glyph_cells"]
    cell_width, cell_height = cells["width"], cells["height"]
    require(isinstance(cell_width, int) and isinstance(cell_height, int), "Invalid glyph cell geometry")
    anchor = source_manifest["home_clock_ink_anchor"]
    right, top = anchor["right_x"], anchor["top_y"]
    require(anchor.get("anchor_rule", "").startswith("Compose glyphs first") and
            0 <= right <= 320 and 0 <= top <= 240,
            "Invalid Home-clock anchor")

    images, alpha = {}, bytearray()
    advances, bounds, hashes = [], [], {}
    for glyph in GLYPHS:
        filename = FILENAMES[glyph]
        path = SOURCE / "glyphs" / filename
        require(path.is_file(), f"Missing glyph source: {path}")
        image = Image.open(path).convert("RGBA")
        glyph_spec = cells["glyphs"][glyph]
        source_size = tuple(glyph_spec.get("cell_size", (cell_width, cell_height)))
        draw_x = glyph_spec.get("draw_x_from_cell_left", 0)
        draw_y = glyph_spec.get("draw_y_from_cell_top", 0)
        require(image.size == source_size, f"Unexpected glyph size: {path}")
        require(0 < source_size[0] <= cell_width and 0 < source_size[1] <= cell_height and
                0 <= draw_x and draw_x + source_size[0] <= cell_width and
                -source_size[1] < draw_y and draw_y < cell_height,
                f"Invalid glyph placement: {path}")
        pixels = image.load()
        for y in range(source_size[1]):
            for x in range(source_size[0]):
                red, green, blue, opacity = pixels[x, y]
                require(opacity == 0 or (red, green, blue) == (245, 245, 245),
                        f"Glyph color differs from #F5F5F5: {path}")
        expected = glyph_spec["ink_bounds_in_cell"]
        measured = alpha_bounds(image)
        require(measured == expected, f"Glyph ink bounds differ from manifest: {path}")
        atlas_cell = Image.new("RGBA", (cell_width, cell_height))
        atlas_cell.alpha_composite(image, (draw_x, draw_y))
        images[glyph] = atlas_cell
        alpha.extend(atlas_cell.getchannel("A").tobytes())
        advances.append(glyph_spec["advance"])
        bounds.append(alpha_bounds(atlas_cell))
        hashes[filename] = sha256(path)

    references = source_manifest["reference_strings"]
    reference_hashes = {}
    for text, details in references.items():
        reference = SOURCE / details["file"]
        require(reference.is_file(), f"Missing reference string: {reference}")
        composed, _ = compose(text, images, advances, cell_height, right, top)
        if refresh_references:
            ink = alpha_bounds(composed)
            details["canvas_size"] = list(composed.size)
            details["ink_bounds"] = ink
            details["intended_ink_bounds_at_home_anchor"] = [
                right - ink[2], top, right, top + ink[3] - ink[1]]
            composed.save(reference)
        expected = Image.open(reference).convert("RGBA")
        require(composed.size == tuple(details["canvas_size"]), f"Reference geometry differs: {reference}")
        require(composed.tobytes() == expected.tobytes(), f"Glyph composition differs: {reference}")
        reference_hashes[details["file"]] = sha256(reference)

    overlay_path = SOURCE / source_manifest["acceptance"]["overlay"]
    composed_14_37, _ = compose("14:37", images, advances, cell_height, right, top)
    ink_14_37 = alpha_bounds(composed_14_37)
    overlay = Image.new("RGBA", (320, 240))
    overlay.alpha_composite(composed_14_37, (right - ink_14_37[2], top - ink_14_37[1]))
    if refresh_references:
        overlay.save(overlay_path)
        source_manifest["acceptance"]["overlay_ink_bounds"] = [
            right - ink_14_37[2], top, right, top + ink_14_37[3] - ink_14_37[1]]
        source_manifest_path.write_text(json.dumps(source_manifest, indent=2) + "\n")
    expected_overlay = Image.open(overlay_path).convert("RGBA")
    require(overlay.tobytes() == expected_overlay.tobytes(), "14:37 overlay differs from approved reference")
    reference_hashes[source_manifest["acceptance"]["overlay"]] = sha256(overlay_path)

    binary = OUTPUT / "clock_atlas.bin"
    binary.write_bytes(alpha)
    generated = {
        "source": "home-clock-assets",
        "source_manifest_sha256": sha256(source_manifest_path),
        "source_glyph_sha256": hashes,
        "source_reference_sha256": reference_hashes,
        "pillow": __version__,
        "glyphs": GLYPHS,
        "scale": 8,
        "filter": "source masks supplied at native resolution",
        "alpha_bits": 8,
        "cell_size": [cell_width, cell_height],
        "advances": advances,
        "ink_bounds": bounds,
        "ink_anchor": {"right_x": right, "top_y": top},
        "measurements": {
            "sample_14_37_bounds": source_manifest["reference_strings"]["14:37"]["intended_ink_bounds_at_home_anchor"],
            "approved_overlay_14_37_sha256": sha256(overlay_path),
        },
        "bytes": len(alpha),
        "sha256": hashlib.sha256(alpha).hexdigest(),
        "recipe": "Approved source-matched glyph masks are copied directly from the supplied native 320x240 Home-clock package; no font is loaded or scaled during conversion. Runtime placement uses the package's visible-ink anchor.",
    }
    (OUTPUT / "clock_atlas.json").write_text(json.dumps(generated, indent=2) + "\n")


if __name__ == "__main__":
    main()
