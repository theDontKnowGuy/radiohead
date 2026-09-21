"""Convert the approved native Home-clock glyph package into an alpha atlas.

The supplied PNG masks are the design source of truth. They are copied into
the firmware's compact 8-bit alpha atlas without rescaling or font rendering.
"""

import hashlib
import json
from pathlib import Path

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
    width = sum(advances[GLYPHS.index(glyph)] for glyph in text)
    result = Image.new("RGBA", (width, cell_height))
    pen = 0
    for glyph in text:
        result.alpha_composite(images[glyph], (pen, 0))
        pen += advances[GLYPHS.index(glyph)]
    overlay = Image.new("RGBA", (320, 240))
    overlay.alpha_composite(result, (right - width, top))
    return result, overlay


def main():
    source_manifest_path = SOURCE / "manifest.json"
    require(source_manifest_path.is_file(), f"Missing approved Home-clock package: {source_manifest_path}")
    source_manifest = json.loads(source_manifest_path.read_text())
    require(source_manifest.get("format_version") == 1, "Unsupported Home-clock package format")
    require(source_manifest.get("canvas") == {"width": 320, "height": 240, "units": "pixels"},
            "Home-clock package must target exactly 320x240")
    require(source_manifest.get("clock_color", {}).get("hex") == "#F5F5F5",
            "Unexpected Home-clock color")

    cells = source_manifest["glyph_cells"]
    cell_width, cell_height = cells["width"], cells["height"]
    require(isinstance(cell_width, int) and isinstance(cell_height, int), "Invalid glyph cell geometry")
    anchor = source_manifest["home_clock_anchor"]
    right, top = anchor["right_x"], anchor["top_y"]
    require(anchor.get("alignment") == "right/top" and 0 <= right <= 320 and 0 <= top <= 240,
            "Invalid Home-clock anchor")

    images, alpha = {}, bytearray()
    advances, bounds, hashes = [], [], {}
    for glyph in GLYPHS:
        filename = FILENAMES[glyph]
        path = SOURCE / "glyphs" / filename
        require(path.is_file(), f"Missing glyph source: {path}")
        image = Image.open(path).convert("RGBA")
        require(image.size == (cell_width, cell_height), f"Unexpected glyph size: {path}")
        pixels = image.load()
        for y in range(cell_height):
            for x in range(cell_width):
                red, green, blue, opacity = pixels[x, y]
                require(opacity == 0 or (red, green, blue) == (245, 245, 245),
                        f"Glyph color differs from #F5F5F5: {path}")
        expected = cells["glyphs"][glyph]["ink_bounds_in_cell"]
        measured = alpha_bounds(image)
        require(measured == expected, f"Glyph ink bounds differ from manifest: {path}")
        images[glyph] = image
        alpha.extend(image.getchannel("A").tobytes())
        advances.append(cells["glyphs"][glyph]["advance"])
        bounds.append(measured)
        hashes[filename] = sha256(path)

    references = source_manifest["reference_strings"]
    reference_hashes = {}
    for text, details in references.items():
        reference = SOURCE / details["file"]
        require(reference.is_file(), f"Missing reference string: {reference}")
        composed, _ = compose(text, images, advances, cell_height, right, top)
        expected = Image.open(reference).convert("RGBA")
        require(composed.size == tuple(details["canvas_size"]), f"Reference geometry differs: {reference}")
        require(composed.tobytes() == expected.tobytes(), f"Glyph composition differs: {reference}")
        reference_hashes[details["file"]] = sha256(reference)

    overlay_path = SOURCE / source_manifest["acceptance"]["overlay"]
    _, overlay = compose("14:37", images, advances, cell_height, right, top)
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
        "anchor": {"right_x": right, "top_y": top},
        "measurements": {
            "sample_14_37_bounds": source_manifest["reference_strings"]["14:37"]["intended_ink_bounds_at_home_anchor"],
            "approved_overlay_14_37_sha256": sha256(overlay_path),
        },
        "bytes": len(alpha),
        "sha256": hashlib.sha256(alpha).hexdigest(),
        "recipe": "Approved Avenir Next Medium glyph masks are copied directly from the supplied native 320x240 Home-clock package; no font is loaded or scaled during conversion.",
    }
    (OUTPUT / "clock_atlas.json").write_text(json.dumps(generated, indent=2) + "\n")


if __name__ == "__main__":
    main()
