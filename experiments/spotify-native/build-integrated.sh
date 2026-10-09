#!/bin/sh
# Compatibility entry point: the complete firmware now has one native build.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
exec "${PYTHON:-python3}" "$repo/tools/build_firmware.py" "$@"
