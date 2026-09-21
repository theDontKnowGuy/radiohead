"""Exercise the production podcast reader with ArduinoJson and split TLS data."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/podcast_native'
out.mkdir(parents=True, exist_ok=True)
exe = out / 'reader'
subprocess.run([
    'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
    f'-I{root / "tools/native_podcast"}', f'-I{root / "include"}',
    f'-I{root / ".pio/libdeps/esp32s3/ArduinoJson/src"}',
    str(root / 'tools/native_podcast/reader.cpp'), '-o', str(exe),
], check=True)
subprocess.run([str(exe)], check=True)
