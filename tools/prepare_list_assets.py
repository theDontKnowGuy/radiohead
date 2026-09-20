"""Prepare native-size translucent list-card and pager surfaces.

The ESP32 blends these RGBA PNGs into its readable PSRAM frame.  Keeping the
anti-aliased rounded corners and alpha in source assets avoids per-pixel work
in the audio/UI path and makes the visual treatment reproducible.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from subprocess import run

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "ui" / "assets" / "lists"
SIZES = {
    "card": [248, 46], "card_focused": [248, 46],
    "card_wide": [304, 42], "card_wide_focused": [304, 42],
    "pager": [50, 44], "pager_active": [50, 44], "rail": [62, 196],
    "pager_thumb_1": [44, 84], "pager_thumb_2": [44, 42], "pager_thumb_3": [44, 28],
}


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    run(["/usr/bin/swift", str(ROOT / "tools" / "prepare_list_assets.swift"), str(OUTPUT)], check=True)
    # NSImage's backing scale can preserve the 4x drawing as a 2x PNG. Normalize
    # the encoded pixels explicitly so LovyanGFX receives exact native geometry.
    for name, (width, height) in SIZES.items():
        png = OUTPUT / f"{name}.png"
        normalized = OUTPUT / f"{name}-native.png"
        run(["/usr/bin/sips", "--resampleHeightWidth", str(height), str(width), str(png),
             "--out", str(normalized)], check=True, capture_output=True)
        normalized.replace(png)
    assets = {name: {"size": size, "sha256": hashlib.sha256((OUTPUT / f"{name}.png").read_bytes()).hexdigest()}
              for name, size in SIZES.items()}
    (OUTPUT / "manifest.json").write_text(json.dumps({
        "recipe": "4x RGBA rounded rectangles; Lanczos downsample; cards navy #10243C/44% and focused blue #087BEA/77%, with matched 82% cyan edges.",
        "assets": assets,
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
