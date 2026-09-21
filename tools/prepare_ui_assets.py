"""Prepare the committed UI source image for the firmware build.

The generated header stays in .pio; only the source image and this reproducible
conversion recipe belong in the repository.
"""

import hashlib
import json
from pathlib import Path
from subprocess import run

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
source = project / "docs" / "bg1.png"
output = project / ".pio" / "ui_assets"
png = output / "background_320x240.png"
header = output / "ui_background_asset.h"

if not source.is_file():
    raise RuntimeError("Missing required UI source image: docs/bg1.png")

output.mkdir(parents=True, exist_ok=True)
header_is_current = header.exists() and "const unsigned char ui_background_png[]" in header.read_text(encoding="utf-8")
if not header_is_current or header.stat().st_mtime < source.stat().st_mtime:
    run(["/usr/bin/sips", "--resampleHeightWidth", "240", "320", str(source), "--out", str(png)], check=True)
    generated = run(
        ["/usr/bin/xxd", "-i", "-n", "ui_background_png", str(png)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    generated = generated.replace("unsigned char ui_background_png[]", "const unsigned char ui_background_png[]")
    generated = generated.replace("unsigned int ui_background_png_len", "const unsigned int ui_background_png_len")
    header.write_text(generated, encoding="utf-8")

env.Append(CPPPATH=[str(output)])

# The browser configuration shell is served from program flash.  Keeping the
# stylesheet and coast photograph as separate HTTP resources prevents each
# settings response from allocating or transmitting a 150 KB inline payload.
web_config_dir = project / "docs" / "ui" / "web-configuration"
web_asset_sources = {
    "ui_web_configuration_css": web_config_dir / "radiohead.css",
    "ui_web_configuration_coast_jpg": web_config_dir / "assets" / "coast.jpg",
}
web_asset_header = output / "ui_web_assets.h"
web_asset_lines = ["#pragma once\n#include <stdint.h>\n"]
for symbol, asset_path in web_asset_sources.items():
    if not asset_path.is_file():
        raise RuntimeError(f"Missing required web configuration asset: {asset_path}")
    data = asset_path.read_bytes()
    rows = [", ".join(f"0x{byte:02x}" for byte in data[index:index + 16])
            for index in range(0, len(data), 16)]
    web_asset_lines.append(
        f"const uint8_t {symbol}[] = {{\n" + ",\n".join(rows) + "\n};\n"
        f"constexpr uint32_t {symbol}_len = {len(data)};\n")
web_asset_content = "\n".join(web_asset_lines)
if not web_asset_header.exists() or web_asset_header.read_text() != web_asset_content:
    web_asset_header.write_text(web_asset_content, encoding="utf-8")

# Licensed, reproducible grayscale glyphs. Never depend on a machine's system font.
font_header = output / "ui_font_assets.h"
font_dir = project / "docs/ui/assets/fonts"
manifest = json.loads((font_dir / "manifest.json").read_text())
expected_fonts = {"small", "body", "title", "temperature", "header_clock", "label", "home_label", "caption", "home_title", "recorded_header"}
if set(manifest["fonts"]) != expected_fonts:
    raise RuntimeError("Missing UI font assets; see tools/prepare_ui_fonts.py")
arrays = ["#pragma once\n#include <stdint.h>\n"]
for name, info in sorted(manifest["fonts"].items()):
    data = (font_dir / f"{name}.vlw").read_bytes()
    if hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise RuntimeError(f"UI font hash mismatch: {name}; regenerate font assets")
    lines = [", ".join(f"0x{byte:02x}" for byte in data[i:i+16]) for i in range(0, len(data), 16)]
    arrays.append(f"const uint8_t ui_font_{name}[] = {{\n" + ",\n".join(lines) + "\n};\n")
    arrays.append(f"constexpr uint16_t ui_font_{name}_space_width = {info['space_width']};\n")
font_content = "\n".join(arrays)
if not font_header.exists() or font_header.read_text() != font_content:
    font_header.write_text(font_content, encoding="utf-8")

# Home's off-device color treatment and anti-aliased geometric assets.
home_dir = project / "docs/ui/assets/home"
home_manifest = json.loads((home_dir / "manifest.json").read_text())
if hashlib.sha256(source.read_bytes()).hexdigest() != home_manifest["source_sha256"]:
    raise RuntimeError("Home background source changed; run tools/prepare_home_assets.py")
arrays = ["#pragma once\n#include <stdint.h>\n"]
for name, info in sorted(home_manifest["assets"].items()):
    data = (home_dir / f"{name}.png").read_bytes()
    if hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise RuntimeError(f"Home asset hash mismatch: {name}")
    lines = [", ".join(f"0x{byte:02x}" for byte in data[i:i+16]) for i in range(0,len(data),16)]
    arrays.append(f"const uint8_t ui_home_{name}[] = {{\n" + ",\n".join(lines) + "\n};\n")
content = "\n".join(arrays)
home_header = output / "ui_home_assets.h"
if not home_header.exists() or home_header.read_text() != content:
    home_header.write_text(content, encoding="utf-8")

# The Home clock is a purpose-built 8-bit alpha atlas, not a VLW font. It is
# composited onto the current readable RGB565 frame so anti-aliased edge pixels
# use the actual sunset beneath them instead of an assumed solid background.
clock_manifest_path = home_dir / "clock_atlas.json"
clock_binary_path = home_dir / "clock_atlas.bin"
clock_manifest = json.loads(clock_manifest_path.read_text())
clock_data = clock_binary_path.read_bytes()
if hashlib.sha256(clock_data).hexdigest() != clock_manifest["sha256"]:
    raise RuntimeError("Home clock atlas hash mismatch; run tools/prepare_home_clock_atlas.py")
glyphs = clock_manifest["glyphs"]
cell_width, cell_height = clock_manifest["cell_size"]
if (clock_manifest["alpha_bits"], clock_manifest["scale"], len(clock_data)) != (
        8, 8, len(glyphs) * cell_width * cell_height):
    raise RuntimeError("Invalid Home clock atlas geometry")
clock_anchor = clock_manifest["ink_anchor"]
if not (0 <= clock_anchor["right_x"] <= 320 and 0 <= clock_anchor["top_y"] <= 240):
    raise RuntimeError("Invalid Home clock anchor")
clock_lines = [", ".join(f"0x{byte:02x}" for byte in clock_data[index:index + 16])
               for index in range(0, len(clock_data), 16)]
clock_content = "#pragma once\n#include <stdint.h>\n"
clock_content += f"constexpr uint8_t ui_home_clock_glyph_count = {len(glyphs)};\n"
clock_content += f"constexpr uint8_t ui_home_clock_cell_width = {cell_width};\n"
clock_content += f"constexpr uint8_t ui_home_clock_cell_height = {cell_height};\n"
clock_content += f"constexpr int16_t ui_home_clock_ink_right = {clock_anchor['right_x']};\n"
clock_content += f"constexpr int16_t ui_home_clock_ink_top = {clock_anchor['top_y']};\n"
clock_content += "constexpr uint8_t ui_home_clock_advances[] = {" + ", ".join(
    str(advance) for advance in clock_manifest["advances"]) + "};\n"
clock_content += "const uint8_t ui_home_clock_alpha[] = {\n" + ",\n".join(clock_lines) + "\n};\n"
clock_header = output / "ui_home_clock_atlas.h"
if not clock_header.exists() or clock_header.read_text() != clock_content:
    clock_header.write_text(clock_content, encoding="utf-8")

# Episode-player assets are separately composed: its metadata needs a darker
# photo treatment and no show artwork has been supplied for this device yet.
podcast_dir = project / "docs" / "ui" / "assets" / "podcast"
podcast_manifest = json.loads((podcast_dir / "manifest.json").read_text())
if hashlib.sha256(source.read_bytes()).hexdigest() != podcast_manifest["source_sha256"]:
    raise RuntimeError("Podcast background source changed; run tools/prepare_podcast_assets.py")
podcast_arrays = ["#pragma once\n#include <stdint.h>\n"]
for name, info in sorted(podcast_manifest["assets"].items()):
    asset = podcast_dir / f"{name}.png"
    data = asset.read_bytes()
    if hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise RuntimeError(f"Podcast asset hash mismatch: {name}; run tools/prepare_podcast_assets.py")
    lines = [", ".join(f"0x{byte:02x}" for byte in data[index:index+16]) for index in range(0, len(data), 16)]
    podcast_arrays.append(f"const uint8_t ui_podcast_{name}[] = {{\n" + ",\n".join(lines) + "\n};\n")
podcast_content = "\n".join(podcast_arrays)
podcast_header = output / "ui_podcast_assets.h"
if not podcast_header.exists() or podcast_header.read_text() != podcast_content:
    podcast_header.write_text(podcast_content, encoding="utf-8")

# List cards and the right-side up/down pager are alpha PNGs. They blend only
# into the readable UI canvas; fallback drawing in display.cpp remains opaque.
list_dir = project / "docs" / "ui" / "assets" / "lists"
list_manifest = json.loads((list_dir / "manifest.json").read_text())
list_arrays = ["#pragma once\n#include <stdint.h>\n"]
for name, info in sorted(list_manifest["assets"].items()):
    asset = list_dir / f"{name}.png"
    data = asset.read_bytes()
    if hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise RuntimeError(f"List asset hash mismatch: {name}; run tools/prepare_list_assets.py")
    lines = [", ".join(f"0x{byte:02x}" for byte in data[i:i+16]) for i in range(0, len(data), 16)]
    list_arrays.append(f"const uint8_t ui_list_{name}[] = {{\n" + ",\n".join(lines) + "\n};\n")
list_content = "\n".join(list_arrays)
list_header = output / "ui_list_assets.h"
if not list_header.exists() or list_header.read_text() != list_content:
    list_header.write_text(list_content, encoding="utf-8")

# Recorded-player transport artwork is user supplied as SVG. LovyanGFX decodes
# the rasterized native-size PNG into the readable UI canvas; it never needs an
# SVG renderer on the ESP32.
player_sources = {
    "rewind_15": ("rewind-15.svg", 58),
    "pause": ("pause.svg", 72),
    "play": ("play.svg", 72),
    "forward_30": ("forward-30.svg", 58),
}
player_source_dir = project / "docs" / "ui" / "icons"
player_output_dir = output / "player_icons"
player_output_dir.mkdir(exist_ok=True)
player_header = output / "ui_player_assets.h"
asset_script = project / "tools" / "prepare_ui_assets.py"
player_arrays = ["#pragma once\n#include <stdint.h>\n"]
for symbol, (filename, size) in player_sources.items():
    source_svg = player_source_dir / filename
    if not source_svg.is_file():
        raise RuntimeError(f"Missing recorded-player icon: {source_svg}")
    native_png = player_output_dir / f"{symbol}_{size}.png"
    if (not native_png.exists() or native_png.stat().st_mtime < source_svg.stat().st_mtime or
            native_png.stat().st_mtime < asset_script.stat().st_mtime):
        # AppKit preserves the SVG's alpha; Quick Look thumbnails flatten it to
        # white and would leave square blocks over the photographic background.
        run(["/usr/bin/swift", str(project / "tools" / "rasterize_svg.swift"),
             str(source_svg), str(native_png), str(size)], check=True)
    data = native_png.read_bytes()
    lines = [", ".join(f"0x{byte:02x}" for byte in data[i:i+16])
             for i in range(0, len(data), 16)]
    player_arrays.append(
        f"const uint8_t ui_player_{symbol}[] = {{\n" + ",\n".join(lines) + "\n};\n")
player_content = "\n".join(player_arrays)
if not player_header.exists() or player_header.read_text() != player_content:
    player_header.write_text(player_content, encoding="utf-8")
