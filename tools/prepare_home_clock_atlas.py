"""Pack the supplied v3 Home-clock alpha masks into the firmware atlas.

The package's grayscale masks are the source of truth. This tool decodes their
PNG scanlines directly; it never loads a font, scales, dilates, recolors, or
otherwise regenerates a glyph.
"""

import hashlib
import json
from pathlib import Path
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "ui" / "assets" / "home"
SOURCE = OUTPUT / "home-clock-assets"
GLYPHS = "0123456789:-"
FILENAMES = {**{glyph: f"{glyph}.png" for glyph in "0123456789"},
             ":": "colon.png", "-": "hyphen.png"}
ADVANCE_SCALE = 10000


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def unfilter(raw, width, height, bytes_per_pixel):
    stride = width * bytes_per_pixel
    require(len(raw) == height * (stride + 1), "Unexpected PNG payload size")
    rows = bytearray(height * stride)
    for y in range(height):
        source = y * (stride + 1)
        destination = y * stride
        filter_type = raw[source]
        for x in range(stride):
            value = raw[source + 1 + x]
            left = rows[destination + x - bytes_per_pixel] if x >= bytes_per_pixel else 0
            up = rows[destination - stride + x] if y else 0
            up_left = rows[destination - stride + x - bytes_per_pixel] if y and x >= bytes_per_pixel else 0
            if filter_type == 0:
                pass
            elif filter_type == 1:
                value = (value + left) & 0xFF
            elif filter_type == 2:
                value = (value + up) & 0xFF
            elif filter_type == 3:
                value = (value + ((left + up) >> 1)) & 0xFF
            elif filter_type == 4:
                prediction = left + up - up_left
                left_distance = abs(prediction - left)
                up_distance = abs(prediction - up)
                up_left_distance = abs(prediction - up_left)
                nearest = left if left_distance <= up_distance and left_distance <= up_left_distance else (
                    up if up_distance <= up_left_distance else up_left)
                value = (value + nearest) & 0xFF
            else:
                raise RuntimeError(f"Unsupported PNG filter {filter_type}")
            rows[destination + x] = value
    return bytes(rows)


def read_png(path):
    data = path.read_bytes()
    require(data.startswith(b"\x89PNG\r\n\x1a\n"), f"Not a PNG: {path}")
    offset = 8
    header = None
    payload = bytearray()
    while offset < len(data):
        length, = struct.unpack_from(">I", data, offset)
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        require(offset + 12 + length <= len(data), f"Truncated PNG: {path}")
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            payload.extend(chunk)
        offset += length + 12
        if kind == b"IEND":
            break
    require(header is not None, f"Missing PNG header: {path}")
    width, height, depth, color_type, compression, filter_method, interlace = header
    require(depth == 8 and compression == 0 and filter_method == 0 and interlace == 0,
            f"Unsupported PNG encoding: {path}")
    channels = {0: 1, 6: 4}.get(color_type)
    require(channels is not None, f"Unsupported PNG color type: {path}")
    return width, height, channels, unfilter(zlib.decompress(payload), width, height, channels)


def main():
    manifest_path = SOURCE / "manifest.json"
    require(manifest_path.is_file(), f"Missing Home-clock package: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest["version"] == "v3-heavier", "Expected the supplied v3-heavier clock package")
    require(manifest["canvas_px"] == [320, 240], "Clock package must target 320x240")
    require(manifest["clock_color_rgb"] == "#F5F5F5", "Unexpected Home-clock color")
    require(manifest["glyph_cell_px"] == [32, 38], "Unexpected Home-clock cell size")
    require(manifest["common_baseline_y_px"] == 33, "Unexpected Home-clock baseline")
    require(manifest["runtime_tracking_px"] == 0.25, "Unexpected Home-clock tracking")
    anchor = manifest["clock_anchor"]
    require(anchor == {"right_x_exclusive": 301, "top_y": 41}, "Unexpected Home-clock anchor")

    advances = manifest["per_glyph_font_advance_px"]
    advance_units = []
    alpha = bytearray()
    alpha_hashes = {}
    rgba_hashes = {}
    for glyph in GLYPHS:
        filename = FILENAMES[glyph]
        alpha_path = SOURCE / "glyphs_alpha" / filename
        rgba_path = SOURCE / "glyphs_rgba" / filename
        require(alpha_path.is_file() and rgba_path.is_file(), f"Missing supplied glyph: {glyph}")
        width, height, channels, mask = read_png(alpha_path)
        require((width, height, channels) == (32, 38, 1), f"Invalid alpha mask: {alpha_path}")
        rgba_width, rgba_height, rgba_channels, rgba = read_png(rgba_path)
        require((rgba_width, rgba_height, rgba_channels) == (32, 38, 4), f"Invalid RGBA glyph: {rgba_path}")
        for pixel in range(width * height):
            red, green, blue, opacity = rgba[pixel * 4:pixel * 4 + 4]
            require(opacity == 0 or (red, green, blue) == (245, 245, 245),
                    f"Glyph color differs from #F5F5F5: {rgba_path}")
            require(mask[pixel] == opacity, f"Alpha mask differs from RGBA source: {glyph}")
        alpha.extend(mask)
        alpha_hashes[filename] = sha256(alpha_path)
        rgba_hashes[filename] = sha256(rgba_path)
        advance_units.append(round(advances[glyph] * ADVANCE_SCALE))

    require(len(alpha) == len(GLYPHS) * 32 * 38, "Invalid packed atlas size")
    tracking_units = round(manifest["runtime_tracking_px"] * ADVANCE_SCALE)
    output_binary = OUTPUT / "clock_atlas.bin"
    output_binary.write_bytes(alpha)
    generated = {
        "source": "home-clock-assets/v3-heavier",
        "source_manifest_sha256": sha256(manifest_path),
        "glyphs": GLYPHS,
        "alpha_source": "glyphs_alpha (supplied 8-bit grayscale masks)",
        "rgba_source": "glyphs_rgba (supplied #F5F5F5 straight-alpha validation copies)",
        "source_alpha_sha256": alpha_hashes,
        "source_rgba_sha256": rgba_hashes,
        "alpha_bits": 8,
        "cell_size": [32, 38],
        "baseline_y": manifest["common_baseline_y_px"],
        "color": manifest["clock_color_rgb"],
        "advance_scale": ADVANCE_SCALE,
        "advance_units": advance_units,
        "tracking_units": tracking_units,
        "ink_anchor": {"right_x": anchor["right_x_exclusive"], "top_y": anchor["top_y"]},
        "bytes": len(alpha),
        "sha256": hashlib.sha256(alpha).hexdigest(),
        "recipe": "The supplied v3-heavier alpha masks are packed without resampling or modification. Runtime composes them with manifest advances/tracking, aligns visible ink to the manifest anchor, and blends #F5F5F5 with straight alpha.",
    }
    (OUTPUT / "clock_atlas.json").write_text(json.dumps(generated, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
