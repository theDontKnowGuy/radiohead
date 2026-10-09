#!/usr/bin/env python3
"""Prepare pinned dependencies/assets, build once with ESP-IDF, export matching images."""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from firmware_artifacts import (APP_SLOT_SIZE, EXPORT_FILES, FLASH_FILES,
                                FLASH_SETTINGS, digest, validate_export, validate_layout)

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    return subprocess.run([str(arg) for arg in command], cwd=ROOT, check=True, **kwargs)


def resolve_path(value):
    path = Path(value).expanduser()
    return (ROOT / path).resolve() if not path.is_absolute() else path.resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default=os.environ.get("RADIOHEAD_SPOTIFY_BUILD", ".pio/idf-build"))
    parser.add_argument("--output-dir", default=".pio/build/esp32s3")
    parser.add_argument("--prepared", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    build, output = resolve_path(args.build_dir), resolve_path(args.output_dir)
    core = resolve_path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    pio = os.environ.get("PIO", shutil.which("pio") or str(core / "penv/bin/pio"))
    if not args.prepared:
        # Package installation only: this never invokes an application compiler.
        run([pio, "pkg", "install", "-e", "esp32s3"])
    idf = resolve_path(os.environ.get("IDF_PATH") or core / "packages/framework-espidf")
    arduino = core / "packages/framework-arduinoespressif32"
    lovyan = ROOT / ".pio/libdeps/esp32s3/LovyanGFX"
    arduinojson = ROOT / ".pio/libdeps/esp32s3/ArduinoJson"
    for path, version in ((idf, "3.50505"), (arduino, "3.3.11"),
                          (lovyan, "1.2.21"), (arduinojson, "7.4.3")):
        metadata = path / ("library.json" if path in (lovyan, arduinojson) else "package.json")
        if json.loads(metadata.read_text())["version"] != version:
            raise RuntimeError(f"Expected pinned version {version} in {path}")
    candidate = resolve_path(os.environ.get("RADIOHEAD_SPOTIFY_CANDIDATE", ".pio/spotify-candidate"))
    if not candidate.exists():
        run([ROOT / "experiments/spotify-native/prepare.sh", candidate])
    revisions = ((candidate, "9c51b087d488b8f7a516f77582f1fc965c3cb643"),
                 (candidate / "lib/cspot", "37b650a526625773a5a0e0a90b57527f914e7641"),
                 (candidate / "lib/cspot/cspot/bell", "ead27050f63aba369631ec99ad68322685aff5a6"))
    for path, revision in revisions:
        actual = run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        if actual != revision:
            raise RuntimeError(f"Unexpected candidate revision in {path}: {actual}")
    run([sys.executable, ROOT / "tools/prepare_ui_assets.py"])
    # ESP-IDF names an external component after its directory basename.
    component = ROOT / ".pio/spotify-components/arduino"
    component.parent.mkdir(parents=True, exist_ok=True)
    if component.is_symlink():
        if component.resolve() != arduino.resolve():
            component.unlink()
    if not component.exists():
        component.symlink_to(arduino, target_is_directory=True)
    python_env = resolve_path(os.environ.get("IDF_PYTHON_ENV_PATH", core / "penv/.espidf-5.5.5"))
    python = python_env / "bin/python"
    child = os.environ.copy()
    child.update(IDF_PATH=str(idf), IDF_TOOLS_PATH=str(core),
                 IDF_PYTHON_ENV_PATH=str(python_env), IDF_PYTHON_CHECK_CONSTRAINTS="no",
                 RADIOHEAD_ARDUINO_DIR=str(component), RADIOHEAD_CSPOT_DIR=str(candidate / "lib/cspot/cspot"),
                 RADIOHEAD_LOVYAN_DIR=str(lovyan), RADIOHEAD_ARDUINOJSON_DIR=str(arduinojson))
    tool_paths = []
    for tool, suffix in (("tool-cmake", "bin"), ("tool-ninja", ""), ("toolchain-xtensa-esp-elf", "bin")):
        path = core / "tools" / tool
        if not path.exists():
            path = core / "packages" / tool
        tool_paths.append(str(path / suffix))
    child["PATH"] = os.pathsep.join(tool_paths + [child.get("PATH", "")])
    if not python.exists():
        # Provision ESP-IDF's own Python environment on a newly installed toolchain.
        python_env.mkdir(parents=True, exist_ok=True)
        run([sys.executable, idf / "tools/idf_tools.py", "install-python-env"], env=child)
    build.mkdir(parents=True, exist_ok=True)
    run([python, idf / "tools/idf.py", "-B", build, "-D", f"SDKCONFIG={build / 'sdkconfig'}",
         "-D", f"SDKCONFIG_DEFAULTS={ROOT / 'sdkconfig.defaults'}", "build", "size"], env=child)
    flash = json.loads((build / "flasher_args.json").read_text())
    validate_layout(build, flash["flash_files"], flash["flash_settings"])
    output.mkdir(parents=True, exist_ok=True)
    # Remove the validity marker before copying; an interrupted export cannot be flashed.
    (output / "artifacts.json").unlink(missing_ok=True)
    for offset, name in FLASH_FILES.items():
        target = output / EXPORT_FILES[offset]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / name, target)
    shutil.copyfile(build / "radiohead.elf", output / "firmware.elf")
    shutil.copyfile(build / FLASH_FILES["0x8000"], output / "partitions.bin")
    command = [python, "-m", "esptool", "--chip", "esp32s3", "merge-bin", "--output",
               output / "firmware.factory.bin", "--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "16MB"]
    for offset, name in EXPORT_FILES.items():
        command.extend((offset, output / name))
    run(command, env=child)
    names = set(EXPORT_FILES.values()) | {"firmware.factory.bin", "firmware.elf", "partitions.bin"}
    manifest = {"flash_files": EXPORT_FILES, "flash_settings": FLASH_SETTINGS,
                "sha256": {name: digest(output / name) for name in sorted(names)}}
    (output / "artifacts.json").write_text(json.dumps(manifest, indent=2) + "\n")
    validate_export(output)
    size = (output / "firmware.bin").stat().st_size
    print(f"Native Spotify image: {output / 'firmware.bin'} ({size:,} bytes; {APP_SLOT_SIZE - size:,} bytes free)")


if __name__ == "__main__":
    main()
