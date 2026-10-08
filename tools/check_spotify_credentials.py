"""Exercise production Spotify persistence without real credentials or hardware."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/spotify_credentials_native'
out.mkdir(parents=True, exist_ok=True)
exe = out / 'credentials'
subprocess.run([
    'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
    '-fsanitize=address,undefined',
    f'-I{root / "tools/native_spotify"}', f'-I{root / "include"}',
    str(root / 'tools/native_spotify/credentials.cpp'),
    str(root / 'src/settings_spotify.cpp'), '-o', str(exe),
], check=True)
subprocess.run([str(exe)], check=True)
