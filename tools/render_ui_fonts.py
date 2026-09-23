"""Run native production Home/font fixtures using the pinned LovyanGFX sources.

Run after pio run -e esp32s3. Requires clang/clang++ and Python; Pillow is
optional (converts PPM evidence to PNG). No SDL installation/window is needed.
"""
from pathlib import Path
import os
import re
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
layout = source[start:end]
# QR networking is tested in the firmware build. The Home fixture deliberately
# substitutes its handoff renderer so it can keep compiling the production Home
# layout without pulling Wi-Fi/app-state headers into its host-only seam.
layout = re.sub(r'// NETWORK_QR_BEGIN.*?// NETWORK_QR_END\n', '', layout, flags=re.DOTALL)
(out / 'home_layout.inc').write_text(layout + source[hit_start:hit_end])
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
        deps += [out / 'home_layout.inc', root / '.pio/ui_assets/ui_background_asset.h', root / '.pio/ui_assets/ui_header_assets.h', root / '.pio/ui_assets/ui_home_assets.h', root / '.pio/ui_assets/ui_home_clock_atlas.h', root / '.pio/ui_assets/ui_home_temperature_atlas.h', root / '.pio/ui_assets/ui_podcast_assets.h', root / '.pio/ui_assets/ui_list_assets.h', root / '.pio/ui_assets/ui_player_assets.h']
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
    clock_manifest = __import__('json').loads(
        (root / 'docs/ui/assets/home/clock_atlas.json').read_text())
    clock_alpha = (root / 'docs/ui/assets/home/clock_atlas.bin').read_bytes()
    clock_glyphs = clock_manifest['glyphs']
    cell_width, cell_height = clock_manifest['cell_size']
    advances = clock_manifest['advance_units']
    advance_scale = clock_manifest['advance_scale']
    tracking = clock_manifest['tracking_units']
    colon_side_spacing = clock_manifest.get('colon_side_spacing_units', 0)
    anchor = clock_manifest['ink_anchor']
    pen = 0
    clock_width = 0
    for glyph in '14:37':
        if glyph == ':':
            pen += colon_side_spacing
        glyph_x = (pen + advance_scale // 2) // advance_scale
        clock_width = max(clock_width, glyph_x + cell_width)
        pen += advances[clock_glyphs.index(glyph)] + tracking
        if glyph == ':':
            pen += colon_side_spacing
    clock_string = Image.new('RGBA', (clock_width, cell_height))
    pen = 0
    for glyph in '14:37':
        glyph_index = clock_glyphs.index(glyph)
        start = glyph_index * cell_width * cell_height
        mask = Image.frombytes('L', (cell_width, cell_height),
                               clock_alpha[start:start + cell_width * cell_height])
        glyph_image = Image.new('RGBA', (cell_width, cell_height), (245, 245, 245, 0))
        glyph_image.putalpha(mask)
        if glyph == ':':
            pen += colon_side_spacing
        glyph_x = (pen + advance_scale // 2) // advance_scale
        clock_string.alpha_composite(glyph_image, (glyph_x, 0))
        pen += advances[glyph_index] + tracking
        if glyph == ':':
            pen += colon_side_spacing
    clock_overlay = Image.new('RGBA', (320, 240))
    clock_ink_bounds = clock_string.getchannel('A').getbbox()
    assert clock_ink_bounds is not None
    clock_overlay.alpha_composite(clock_string, (anchor['right_x'] - clock_ink_bounds[2],
                                                  anchor['top_y'] - clock_ink_bounds[1]))
    # Verify the regenerated outline atlas keeps the requested visible-ink anchor.
    clock_bounds = clock_overlay.getchannel('A').getbbox()
    assert clock_bounds[2] == anchor['right_x']
    assert clock_bounds[1] == anchor['top_y']
    for path in out.glob('*.ppm'):
        Image.open(path).save(path.with_suffix('.png'))
    print(f'PNG evidence: {out}; regenerated 14:37 atlas uses the manifest ink anchor')
