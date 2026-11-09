#!/usr/bin/env python3
"""Apply required runtime fixes to new and already prepared Spotify candidates."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / 'experiments/spotify-native/patches/token-refresh-cspot.patch'


def ensure_patch(repository, patch):
    command = ['git', '-C', str(repository), 'apply']
    check = subprocess.run(command + ['--check', str(patch)], capture_output=True)
    if check.returncode == 0:
        subprocess.run(command + [str(patch)], check=True)
        return
    applied = subprocess.run(command + ['--reverse', '--check', str(patch)], capture_output=True)
    if applied.returncode != 0:
        raise RuntimeError(f'Spotify {patch.stem} patch conflicts with candidate changes; '
                           'preserve the checkout and reconcile before building')


def ensure_token_refresh_patch(candidate):
    ensure_patch(Path(candidate) / 'lib/cspot', PATCH)


def ensure_spotify_patches(candidate):
    ensure_token_refresh_patch(candidate)
    ensure_patch(Path(candidate) / 'lib/cspot/cspot/bell',
                 PATCH.parent / 'stream-cancel-bell.patch')
    ensure_patch(Path(candidate) / 'lib/cspot',
                 PATCH.parent / 'stream-cancel-cspot.patch')
    ensure_patch(Path(candidate) / 'lib/cspot',
                 PATCH.parent / 'ap-connection-recovery-cspot.patch')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: ensure_spotify_patches.py CANDIDATE_DIRECTORY')
    ensure_spotify_patches(Path(sys.argv[1]).resolve())
