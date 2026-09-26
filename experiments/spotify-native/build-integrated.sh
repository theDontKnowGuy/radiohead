#!/bin/sh
# Build the opt-in S2 image with the pinned native candidate and the local radio.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
candidate=${RADIOHEAD_SPOTIFY_CANDIDATE:-"$repo/.pio/spotify-candidate"}
case "$candidate" in
    /*) ;;
    *) candidate="$repo/$candidate" ;;
esac
idf=${IDF_PATH:-"$HOME/.platformio/packages/framework-espidf"}
python=${IDF_PYTHON_ENV_PATH:-"$HOME/.platformio/penv/.espidf-5.5.5"}/bin/python
arduino="$HOME/.platformio/packages/framework-arduinoespressif32"
build="$repo/.pio/spotify-idf-build"

cd "$repo"
pio run -e esp32s3
if [ ! -d "$candidate" ]; then
    "$repo/experiments/spotify-native/prepare.sh" "$candidate"
fi
test "$(git -C "$candidate" rev-parse HEAD)" = 9c51b087d488b8f7a516f77582f1fc965c3cb643
test "$(git -C "$candidate/lib/cspot" rev-parse HEAD)" = 37b650a526625773a5a0e0a90b57527f914e7641
grep -q 'ACTIVATE' "$candidate/lib/cspot/cspot/include/SpircHandler.h"
test -d "$arduino"
test -d "$repo/.pio/libdeps/esp32s3/LovyanGFX"
test -d "$repo/.pio/libdeps/esp32s3/ArduinoJson"
mkdir -p "$repo/.pio/spotify-components"
ln -sfn "$arduino" "$repo/.pio/spotify-components/arduino"

export RADIOHEAD_IDF_INTEGRATED=1
export RADIOHEAD_CSPOT_DIR="$candidate/lib/cspot/cspot"
export RADIOHEAD_LOVYAN_DIR="$repo/.pio/libdeps/esp32s3/LovyanGFX"
export RADIOHEAD_ARDUINOJSON_DIR="$repo/.pio/libdeps/esp32s3/ArduinoJson"
export IDF_PATH="$idf"
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-"$HOME/.platformio"}"
export IDF_PYTHON_ENV_PATH="${IDF_PYTHON_ENV_PATH:-"$HOME/.platformio/penv/.espidf-5.5.5"}"
export IDF_PYTHON_CHECK_CONSTRAINTS=no
PATH="$HOME/.platformio/tools/tool-cmake/bin:$HOME/.platformio/tools/tool-ninja:$HOME/.platformio/tools/toolchain-xtensa-esp-elf/bin:$PATH"
export PATH
"$python" "$idf/tools/idf.py" -B "$build" build
cmp "$build/partition_table/partition-table.bin" "$repo/.pio/build/esp32s3/partitions.bin"
echo "Integrated image: $build/radiohead.bin"
