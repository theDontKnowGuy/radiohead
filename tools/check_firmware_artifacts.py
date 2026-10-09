#!/usr/bin/env python3
"""Verify real exported images and rejection of unsafe/stale flash artifacts."""
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from firmware_artifacts import EXPORT_FILES, digest, validate_export

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / ".pio/build/esp32s3"
validate_export(source)
command = subprocess.check_output(
    [sys.executable, str(ROOT / "tools/flash_built_firmware.py"),
     "--port", "unused", "--dry-run"], text=True)
assert "write-flash" in command and "erase" not in command
assert all(f" {offset} " in command for offset in EXPORT_FILES)
assert " 0x9000 " not in command and " 0x650000 " not in command and " 0xc90000 " not in command

with tempfile.TemporaryDirectory(prefix="radiohead-artifacts-") as temp:
    output = Path(temp) / "artifacts"
    shutil.copytree(source, output, ignore=shutil.ignore_patterns("*.o", "*.a", "src", "lib*"))
    original = json.loads((output / "artifacts.json").read_text())

    def rejected(label):
        try:
            validate_export(output)
        except (ValueError, OSError, KeyError):
            print(f"PASS: {label}")
        else:
            raise AssertionError(f"Accepted unsafe export: {label}")

    manifest = json.loads(json.dumps(original))
    manifest["flash_files"]["0x9000"] = "firmware.bin"
    (output / "artifacts.json").write_text(json.dumps(manifest))
    rejected("write to NVS")
    (output / "artifacts.json").write_text(json.dumps(original))
    boot = output / "bootloader/bootloader.bin"
    boot.write_bytes(b"bad bootloader")
    rejected("corrupted bootloader")
    shutil.copyfile(source / "bootloader/bootloader.bin", boot)
    factory = output / "firmware.factory.bin"
    data = bytearray(factory.read_bytes())
    data[0x10000 + 512] ^= 1
    factory.write_bytes(data)
    manifest = json.loads(json.dumps(original))
    manifest["sha256"]["firmware.factory.bin"] = digest(factory)
    (output / "artifacts.json").write_text(json.dumps(manifest))
    rejected("factory/application mismatch even with refreshed hash")
    shutil.copyfile(source / "firmware.factory.bin", factory)
    (output / "artifacts.json").write_text(json.dumps(original))
    app = output / "firmware.bin"
    with app.open("ab") as stream:
        stream.write(b"stale")
    rejected("stale application hash")
    with app.open("wb") as stream:
        stream.write(b"\xe9")
        stream.truncate(0x640001)
    rejected("application exceeds OTA slot")
print("PASS: native/factory artifacts and settings-preserving upload command")
