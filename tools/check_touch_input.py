"""Compile production touch recognition and UI controller with host device stubs."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/touch_native'
out.mkdir(parents=True, exist_ok=True)
exe = out / 'touch'
subprocess.run([
    'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
    f'-I{root / "tools/native_input"}', f'-I{root / "tools/native_ui"}',
    f'-I{root / "include"}', str(root / 'src/ui_controller.cpp'),
    str(root / 'tools/native_input/touch.cpp'), '-o', str(exe),
], check=True)
subprocess.run([str(exe)], check=True)
