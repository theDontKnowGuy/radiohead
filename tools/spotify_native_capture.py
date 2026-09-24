#!/usr/bin/env python3
"""Capture S1 serial evidence and optionally provision app keys once over USB.

Keep the log and credential file outside git. This program performs no Spotify
network requests; it is only a serial recorder and first-boot provisioner.
"""

import argparse
import os
import time
from pathlib import Path

import serial


def load_private_credentials(path: Path):
    values = dict(line.split("=", 1) for line in path.read_text().splitlines() if "=" in line)
    names = ("SPOTIFY_CLIENT_ID", "SPOTIFY_CLIENT_SECRET")
    if any(not values.get(name) for name in names):
        raise ValueError("private credential file is missing a required value")
    return [values[name].strip() for name in names]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--duration", type=int, default=7200, help="seconds")
    parser.add_argument("--private-env", type=Path, help="first boot provisioning only")
    parser.add_argument("--reset-on-open", action="store_true", help="restart device after capture opens")
    args = parser.parse_args()

    secrets = load_private_credentials(args.private_env) if args.private_env else []
    descriptor = os.open(args.log, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    end = time.monotonic() + args.duration
    line_count = 0
    provisioned = False
    reset_done = False
    with os.fdopen(descriptor, "w", buffering=1) as output:
        while time.monotonic() < end:
            try:
                with serial.Serial(args.port, 115200, timeout=1) as radio:
                    if args.reset_on_open and not reset_done:
                        radio.dtr = False
                        radio.rts = True
                        time.sleep(0.2)
                        radio.rts = False
                        time.sleep(0.2)
                        reset_done = True
                    opened_at = time.monotonic()
                    while time.monotonic() < end:
                        raw = radio.readline()
                        line = raw.decode("utf-8", "replace").rstrip("\r\n") if raw else ""
                        if secrets and not provisioned and (
                            "Waiting for private USB serial Spotify app provisioning" in line
                            or time.monotonic() - opened_at >= 10
                        ):
                            message = (
                                "SPOTIFY_CLIENT_ID=" + secrets[0] + "\n"
                                "SPOTIFY_CLIENT_SECRET=" + secrets[1] + "\n"
                            )
                            radio.write(message.encode())
                            radio.flush()
                            provisioned = True
                        if not raw:
                            continue
                        for secret in secrets:
                            line = line.replace(secret, "[REDACTED]")
                        output.write(f"{time.time():.3f} {line}\n")
                        line_count += 1
            except (OSError, serial.SerialException):
                time.sleep(1)
    print(f"captured_lines={line_count} first_boot_provision_sent={provisioned}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("capture interrupted")
