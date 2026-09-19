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
expected_fonts = {"small", "body", "title", "temperature", "clock", "label", "caption"}
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
