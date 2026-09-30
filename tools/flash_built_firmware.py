#!/usr/bin/env python3
"""Flash the already built default image without rebuilding it.

Run with PlatformIO's Python: ~/.platformio/penv/bin/python
tools/flash_built_firmware.py --port /dev/cu.usbmodem11201
"""

import argparse
import hashlib
import json
import shlex
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".pio/spotify-idf-build"
FIRMWARE = ROOT / ".pio/build/esp32s3/firmware.bin"
FLASH_FILES = {
    "0x0": "bootloader/bootloader.bin",
    "0x8000": "partition_table/partition-table.bin",
    "0xe000": "ota_data_initial.bin",
    "0x10000": "radiohead.bin",
}
FLASH_SETTINGS = {"flash_mode": "dio", "flash_size": "16MB", "flash_freq": "80m"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ESP32 serial port")
    parser.add_argument("--baud", type=int, default=460800)
    parser.add_argument("--dry-run", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    manifest = json.loads((BUILD / "flasher_args.json").read_text())
    if manifest.get("flash_files") != FLASH_FILES or manifest.get("flash_settings") != FLASH_SETTINGS:
        raise SystemExit("Build flash layout differs from the expected Radiohead layout")
    if not FIRMWARE.is_file() or not (BUILD / "radiohead.bin").is_file():
        raise SystemExit("Built firmware is missing; run pio run -e esp32s3 first")
    digest = lambda path: hashlib.sha256(path.read_bytes()).digest()
    if digest(FIRMWARE) != digest(BUILD / "radiohead.bin"):
        raise SystemExit("Default firmware differs from the integrated build; rebuild before flashing")

    command = [
        sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
        "--baud", str(args.baud), "--before", "default-reset", "--after", "hard-reset",
        "write-flash", "-z", "--flash-mode", "dio", "--flash-freq", "80m",
        "--flash-size", "16MB",
    ]
    for offset, name in FLASH_FILES.items():
        command.extend((offset, str(FIRMWARE if offset == "0x10000" else BUILD / name)))
    if args.dry_run:
        print(shlex.join(command))
        return
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
