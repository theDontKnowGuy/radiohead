"""Compile production touch recognition and UI controller with host device stubs."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/touch_native'
out.mkdir(parents=True, exist_ok=True)
exe = out / 'touch'
# Compile the production input bridge without hardware command execution.
source = (root / 'src/ui_runtime.cpp').read_text()
start = source.index('void serviceUiInput(')
body = source.index('{', start)
end, depth = body + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
bridge = out / 'ui_input.cpp'
bridge.write_text('''#include "ui_runtime.h"
#include "ui_controller.h"
#include "device_control.h"
extern unsigned long lastInteraction;
UiTarget uiHitTest(const UiRenderState&, int16_t, int16_t);
''' + source[start:end])
subprocess.run([
    'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
    f'-I{root / "tools/native_input"}', f'-I{root / "tools/native_ui"}',
    f'-I{root / "include"}', str(root / 'src/ui_controller.cpp'),
    str(root / 'tools/native_input/touch.cpp'), str(bridge), '-o', str(exe),
], check=True)
subprocess.run([str(exe)], check=True)
