#!/usr/bin/env bash
#
# Build and publish the GitHub release channel used by FirmwareUpdater.
#
# Edit include/FirmwareVersion.h, commit it, then run:
#
#   scripts/release.sh
#
# The version, date and notes are read only from that header, keeping the
# image's self-report, Git tag, release notes and manifest in lockstep.
set -euo pipefail

REPO="${RADIOHEAD_REPO:-theDontKnowGuy/radiohead}"
RELEASE_ENVS="${RADIOHEAD_RELEASE_ENVS:-esp32s3}"
PIO="${PIO:-pio}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGING="$ROOT/build/release"
HEADER="$ROOT/include/FirmwareVersion.h"

die() { echo "error: $*" >&2; exit 1; }

md5_of() {
    if command -v md5 >/dev/null 2>&1; then md5 -q "$1";
    else md5sum "$1" | cut -d' ' -f1; fi
}

size_of() {
    if stat -f%z "$1" >/dev/null 2>&1; then stat -f%z "$1";
    else stat -c%s "$1"; fi
}

read_define() {
    sed -n "s/^#define $1 \"\(.*\)\"$/\1/p" "$HEADER"
}

command -v gh >/dev/null 2>&1 || die "gh CLI not found"
command -v "$PIO" >/dev/null 2>&1 || die "pio not found (set PIO=... if needed)"

VERSION="$(read_define FIRMWARE_VERSION)"
RELEASED="$(read_define FIRMWARE_RELEASED)"
NOTES="$(read_define FIRMWARE_NOTES)"
[ -n "$VERSION" ] && [ -n "$RELEASED" ] && [ -n "$NOTES" ] || die "could not read release metadata from $HEADER"
echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || die "FIRMWARE_VERSION must be MAJOR.MINOR.PATCH"
echo "$RELEASED" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}$' || die "FIRMWARE_RELEASED must be YYYY-MM-DD"

TODAY="$(date -u +%Y-%m-%d)"
if [ "$RELEASED" != "$TODAY" ] && [ -z "${RADIOHEAD_ALLOW_STALE_DATE:-}" ]; then
    die "FIRMWARE_RELEASED is $RELEASED, not today's UTC date $TODAY (set RADIOHEAD_ALLOW_STALE_DATE=1 to override)"
fi

TAG="v$VERSION"
[ -z "$(git -C "$ROOT" status --porcelain)" ] || die "working tree is dirty -- commit before releasing"
git -C "$ROOT" rev-parse "$TAG" >/dev/null 2>&1 && die "tag $TAG already exists"
gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1 && die "release $TAG already exists"

rm -rf "$STAGING"
mkdir -p "$STAGING"
for env in $RELEASE_ENVS; do
    echo "==> Building $env"
    "$PIO" run --project-dir "$ROOT" -e "$env" -t clean >/dev/null
    "$PIO" run --project-dir "$ROOT" -e "$env"
    image="$ROOT/.pio/build/$env/firmware.bin"
    [ -f "$image" ] || die "no firmware image for $env"
    cp "$image" "$STAGING/radiohead-$env-$VERSION.bin"
done

MANIFEST="$STAGING/manifest.json"
{
    echo "{"
    echo "  \"version\": \"$VERSION\","
    printf '  "notes": '
    python3 -c 'import json,sys; print(json.dumps(sys.argv[1]) + ",")' "$NOTES"
    echo "  \"released\": \"$RELEASED\","
    echo "  \"builds\": {"
    first=1
    for env in $RELEASE_ENVS; do
        image="$STAGING/radiohead-$env-$VERSION.bin"
        [ "$first" -eq 1 ] || echo ","
        first=0
        printf '    "%s": {\n' "$env"
        printf '      "url": "https://github.com/%s/releases/download/%s/radiohead-%s-%s.bin",\n' "$REPO" "$TAG" "$env" "$VERSION"
        printf '      "md5": "%s",\n' "$(md5_of "$image")"
        printf '      "size": %s\n' "$(size_of "$image")"
        printf '    }'
    done
    echo
    echo "  }"
    echo "}"
} > "$MANIFEST"
python3 -m json.tool "$MANIFEST" >/dev/null || die "generated manifest is invalid"

git -C "$ROOT" tag -a "$TAG" -m "$NOTES"
git -C "$ROOT" push origin "$TAG"
gh release create "$TAG" --repo "$REPO" --title "$TAG" --notes "$NOTES" "$STAGING"/*.bin "$MANIFEST"
echo "Published $TAG. Radios check for it hourly."
