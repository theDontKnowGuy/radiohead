"""Run native production Home/font fixtures using the pinned LovyanGFX sources.

Run after pio run -e esp32s3. Requires clang/clang++ and Python; Pillow is
optional (converts PPM evidence to PNG). No SDL installation/window is needed.
"""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
lgfx = root / '.pio/libdeps/esp32s3/LovyanGFX/src'
out = root / '.pio/ui_native'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/display.cpp').read_text()
# Extract the actual complete production layout, not a separately drawn mockup.
start = source.index('constexpr uint16_t kNavy')
end = source.index('}  // namespace', start)
hit_start = source.index('UiTarget uiHitTest(')
hit_end = source.index('void renderRadioUi(', hit_start)
(out / 'home_layout.inc').write_text(source[start:end] + source[hit_start:hit_end])
includes = [f'-I{path}' for path in [lgfx, root / 'include', root / 'tools/native_ui', root / '.pio/ui_assets', out]]
cpp = [lgfx / f'lgfx/v1/{name}.cpp' for name in ['LGFXBase', 'LGFX_Sprite', 'lgfx_fonts']]
cpp += list((lgfx / 'lgfx/v1/misc').glob('*.cpp'))
cpp += [lgfx / 'lgfx/v1/panel/Panel_Device.cpp', root / 'src/display_fonts.cpp', root / 'src/ui_text.cpp', root / 'tools/native_ui/home.cpp']
c = list((lgfx / 'lgfx/utility').glob('*.c'))
# lgfx_fonts.cpp's font definitions reference these tables even without a panel.
c += list((lgfx / 'lgfx/Fonts').rglob('*.c'))
c += list((lgfx / 'lgfx/v1/lv_font').glob('*.c'))
objects = []
for index, path in enumerate(c + cpp):
    obj = out / f'{index}-{path.stem}.o'
    objects.append(str(obj))
    deps = [path, root / 'include/display_fonts.h', root / 'include/ui_controller.h', root / 'include/ui_text.h', root / '.pio/ui_assets/ui_font_assets.h']
    if path.name == 'home.cpp':
        deps += [out / 'home_layout.inc', root / '.pio/ui_assets/ui_background_asset.h', root / '.pio/ui_assets/ui_home_assets.h', root / '.pio/ui_assets/ui_player_assets.h']
    if obj.exists() and all(obj.stat().st_mtime > dep.stat().st_mtime for dep in deps):
        continue
    command = ['clang++', '-std=c++17'] if path.suffix == '.cpp' else ['clang']
    subprocess.run(command + ['-O2', '-ffunction-sections', '-fdata-sections', '-Wno-deprecated-declarations', '-Wno-vla-cxx-extension'] + includes + ['-c', str(path), '-o', str(obj)], check=True)
exe = out / 'home'
subprocess.run(['clang++', '-Wl,-dead_strip', *objects, '-o', str(exe)], check=True)
subprocess.run([str(exe), str(out)], check=True, env={**os.environ, 'TZ': 'UTC'})
assert (out / 'home-smooth.ppm').read_bytes() == (out / 'home-restored.ppm').read_bytes()
assert (out / 'header-updated.ppm').read_bytes() == (out / 'header-fresh.ppm').read_bytes()
try:
    from PIL import Image
except ImportError:
    print(f'PPM evidence: {out} (install Pillow for PNG copies)')
else:
    for path in out.glob('*.ppm'):
        Image.open(path).save(path.with_suffix('.png'))
    print(f'PNG evidence: {out}')
