"""Make the regular PlatformIO target produce and upload the integrated image.

PlatformIO builds the Arduino objects first. The pinned native ESP-IDF build then
replaces its application binary, while serial upload uses that build's matching
bootloader, partition table, and initial OTA selection.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from SCons.Script import AlwaysBuild

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
build = project / ".pio/spotify-idf-build"
firmware = project / ".pio/build/esp32s3/firmware.bin"


def build_integrated_image(target, source, env):
    child_env = os.environ.copy()
    child_env["RADIOHEAD_SKIP_PIO_BASELINE"] = "1"
    child_env["RADIOHEAD_SPOTIFY_BUILD"] = str(build)
    subprocess.run(
        [str(project / "experiments/spotify-native/build-integrated.sh")],
        cwd=project,
        env=child_env,
        check=True,
    )

    flash = json.loads((build / "flasher_args.json").read_text())
    expected = {
        "0x0": "bootloader/bootloader.bin",
        "0x8000": "partition_table/partition-table.bin",
        "0xe000": "ota_data_initial.bin",
        "0x10000": "radiohead.bin",
    }
    if flash["flash_files"] != expected or flash["flash_settings"] != {
        "flash_mode": "dio", "flash_size": "16MB", "flash_freq": "80m"
    }:
        raise RuntimeError("Integrated flash layout changed; update the default uploader")

    image = build / "radiohead.bin"
    size = image.stat().st_size
    with image.open("rb") as image_file:
        image_magic = image_file.read(1)
    if image_magic != b"\xe9" or size > 0x640000:
        raise RuntimeError(f"Integrated image is invalid or exceeds the app slot: {size} bytes")
    shutil.copyfile(image, firmware)
    esptool = Path(env.PioPlatform().get_package_dir("tool-esptoolpy")) / "esptool.py"
    subprocess.run(
        [
            sys.executable, str(esptool), "--chip", "esp32s3", "merge-bin", "--output",
            str(project / ".pio/build/esp32s3/firmware.factory.bin"),
            "--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "16MB",
            "0x0", str(build / expected["0x0"]),
            "0x8000", str(build / expected["0x8000"]),
            "0xe000", str(build / expected["0xe000"]),
            "0x10000", str(firmware),
        ],
        cwd=project,
        check=True,
    )
    print(f"Default Spotify image: {firmware} ({size:,} bytes; {0x640000 - size:,} bytes free)")


# PlatformIO can skip firmware.bin's builder when only native component sources
# change. These aliases run after their firmware dependency on every invocation.
env.AddPostAction("buildprog", build_integrated_image)
AlwaysBuild(env.Alias("buildprog"))
env.AddPreAction("upload", build_integrated_image)

# Use the matching ESP-IDF bootloader and partition images. Flash only these
# regions, so Preferences in NVS and the other OTA slot are preserved.
env.Replace(
    UPLOADCMD=(
        f'"{sys.executable}" "$PROJECT_DIR/tools/flash_built_firmware.py" '
        '--port "$UPLOAD_PORT" --baud $UPLOAD_SPEED'
    )
)
