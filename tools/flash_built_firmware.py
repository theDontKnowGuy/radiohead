#!/usr/bin/env python3
"""Flash the already built default image without rebuilding it.

Run with PlatformIO's Python: ~/.platformio/penv/bin/python
tools/flash_built_firmware.py --port /dev/cu.usbmodem11201
"""

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

from firmware_artifacts import validate_export


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ESP32 serial port")
    parser.add_argument("--baud", type=int, default=460800)
    parser.add_argument("--artifacts-dir", type=Path, default=ROOT / ".pio/build/esp32s3")
    parser.add_argument("--dry-run", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    try:
        manifest = validate_export(args.artifacts_dir)
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f"Invalid exported build: {error}; run python3 tools/build_firmware.py") from error

    command = [
        sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
        "--baud", str(args.baud), "--before", "default-reset", "--after", "hard-reset",
        "write-flash", "-z", "--flash-mode", "dio", "--flash-freq", "80m",
        "--flash-size", "16MB",
    ]
    for offset, name in manifest["flash_files"].items():
        command.extend((offset, str(args.artifacts_dir / name)))
    if args.dry_run:
        print(shlex.join(command))
        return
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
