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

# Licensed, reproducible grayscale glyphs. Never depend on a machine's system font.
font_header = output / "ui_font_assets.h"
font_dir = project / "docs/ui/assets/fonts"
manifest = json.loads((font_dir / "manifest.json").read_text())
expected_fonts = {"small", "body", "title", "temperature", "clock", "label", "caption", "home_title"}
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
    "rewind_15": ("rewind-15.svg", 64),
    "pause": ("pause.svg", 72),
    "play": ("play.svg", 72),
    "forward_30": ("forward-30.svg", 64),
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
