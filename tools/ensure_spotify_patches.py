#!/usr/bin/env python3
"""Apply the token-refresh crash fix to new and already prepared candidates."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / 'experiments/spotify-native/patches/token-refresh-cspot.patch'


def ensure_token_refresh_patch(candidate):
    repository = Path(candidate) / 'lib/cspot'
    command = ['git', '-C', str(repository), 'apply']
    check = subprocess.run(command + ['--check', str(PATCH)], capture_output=True)
    if check.returncode == 0:
        subprocess.run(command + [str(PATCH)], check=True)
        return
    applied = subprocess.run(command + ['--reverse', '--check', str(PATCH)], capture_output=True)
    if applied.returncode != 0:
        raise RuntimeError('Spotify token-refresh patch conflicts with candidate changes; '
                           'preserve the checkout and reconcile before building')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: ensure_spotify_patches.py CANDIDATE_DIRECTORY')
    ensure_token_refresh_patch(Path(sys.argv[1]).resolve())
