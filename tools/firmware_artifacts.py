"""Shared artifact checks for the native exporter and settings-preserving uploader."""
import hashlib
import json
import struct
from pathlib import Path

FLASH_FILES = {
    "0x0": "bootloader/bootloader.bin",
    "0x8000": "partition_table/partition-table.bin",
    "0xe000": "ota_data_initial.bin",
    "0x10000": "radiohead.bin",
}
FLASH_SETTINGS = {"flash_mode": "dio", "flash_size": "16MB", "flash_freq": "80m"}
APP_SLOT_SIZE = 0x640000
EXPORT_FILES = {offset: ("firmware.bin" if offset == "0x10000" else name)
                for offset, name in FLASH_FILES.items()}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_layout(directory: Path, files: dict, settings: dict) -> None:
    if files not in (FLASH_FILES, EXPORT_FILES) or settings != FLASH_SETTINGS:
        raise ValueError("Flash layout differs from the expected Radiohead layout")
    for name in files.values():
        if not (directory / name).is_file():
            raise ValueError(f"Missing build artifact: {name}")
    with (directory / files["0x0"]).open("rb") as stream:
        if stream.read(1) != b"\xe9":
            raise ValueError("Invalid bootloader image")
    if (directory / files["0x0"]).stat().st_size > 0x8000:
        raise ValueError("Bootloader overlaps the partition table")
    app = directory / files["0x10000"]
    with app.open("rb") as stream:
        magic = stream.read(1)
    if magic != b"\xe9" or not 0 < app.stat().st_size <= APP_SLOT_SIZE:
        raise ValueError("Firmware is invalid or exceeds the OTA slot")
    table = (directory / files["0x8000"]).read_bytes()
    if len(table) != 3072:
        raise ValueError("Unexpected partition table size")
    expected = [
        (1, 2, 0x9000, 0x5000, b"nvs"),
        (1, 0, 0xe000, 0x2000, b"otadata"),
        (0, 16, 0x10000, APP_SLOT_SIZE, b"app0"),
        (0, 17, 0x650000, APP_SLOT_SIZE, b"app1"),
        (1, 130, 0xc90000, 0x360000, b"spiffs"),
        (1, 3, 0xff0000, 0x10000, b"coredump"),
    ]
    for index, entry in enumerate(expected):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from(
            "<HBBII16sI", table, index * 32)
        if magic != 0x50aa or (kind, subtype, offset, size, label.rstrip(b"\0")) != entry or flags:
            raise ValueError("Partition table changed; NVS/OTA/filesystem compatibility needs review")
    # ESP-IDF emits an MD5 entry followed by erased table space.
    if table[192:208] != b"\xeb\xeb" + b"\xff" * 14 or table[208:224] != hashlib.md5(table[:192]).digest():
        raise ValueError("Partition table checksum is invalid")
    if table[224:] != b"\xff" * (len(table) - 224):
        raise ValueError("Unexpected extra partitions or table size")


def validate_export(directory: Path) -> dict:
    manifest = json.loads((directory / "artifacts.json").read_text())
    validate_layout(directory, manifest["flash_files"], manifest["flash_settings"])
    required = set(EXPORT_FILES.values()) | {"firmware.factory.bin", "firmware.elf", "partitions.bin"}
    if set(manifest["sha256"]) != required or manifest["flash_files"] != EXPORT_FILES:
        raise ValueError("Incomplete exported artifact manifest")
    for name, sha256 in manifest["sha256"].items():
        if digest(directory / name) != sha256:
            raise ValueError(f"Exported artifact differs from its manifest: {name}; rebuild before flashing")
    if digest(directory / "partitions.bin") != digest(directory / EXPORT_FILES["0x8000"]):
        raise ValueError("Partition export differs from the flash table")
    factory = (directory / "firmware.factory.bin").read_bytes()
    for offset, name in EXPORT_FILES.items():
        start = int(offset, 16)
        data = (directory / name).read_bytes()
        if factory[start:start + len(data)] != data:
            raise ValueError(f"Factory image differs from the flash artifact: {name}")
    return manifest
