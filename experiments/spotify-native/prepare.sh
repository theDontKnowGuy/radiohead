#!/bin/sh
# Prepare the pinned standalone S1 native candidate outside the firmware tree.
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 EMPTY_OUTPUT_DIRECTORY" >&2
    exit 2
fi

output=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -e "$output" ]; then
    echo "output already exists: $output" >&2
    exit 2
fi

git clone --recurse-submodules https://github.com/VitaliBorys/cspot-waveshare-esp32s3.git "$output"
git -C "$output" checkout --detach 9c51b087d488b8f7a516f77582f1fc965c3cb643
git -C "$output" submodule update --init --recursive

test "$(git -C "$output/lib/cspot" rev-parse HEAD)" = 37b650a526625773a5a0e0a90b57527f914e7641
test "$(git -C "$output/lib/cspot/cspot/bell" rev-parse HEAD)" = ead27050f63aba369631ec99ad68322685aff5a6
git -C "$output" apply "$script_dir/patches/waveshare-radiohead.patch"
git -C "$output/lib/cspot" apply "$script_dir/patches/cspot-radiohead.patch"
git -C "$output/lib/cspot/cspot/bell" apply "$script_dir/patches/bell-idf55.patch"
git -C "$output" apply "$script_dir/patches/wifi-recovery-waveshare.patch"
git -C "$output/lib/cspot" apply "$script_dir/patches/wifi-recovery-cspot.patch"
git -C "$output/lib/cspot/cspot/bell" apply "$script_dir/patches/wifi-recovery-bell.patch"
git -C "$output/lib/cspot" apply "$script_dir/patches/wifi-recovery-session.patch"

echo "Native candidate prepared in $output"
echo "Build with ESP-IDF 5.5.5. The candidate reads Wi-Fi and Spotify credentials from NVS."
echo "Never flash it using the upstream partition table or a full-chip erase."
