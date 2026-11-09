#!/usr/bin/env python3
"""Run production icon compositing with heap access forbidden and guarded pixels.

Generate assets first with tools/prepare_ui_assets.py. Native visual parity is
checked separately by tools/render_ui_fonts.py --alpha-icons-only.
"""
from pathlib import Path
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/display.cpp').read_text()
definition = re.search(r'void drawAlphaIcon\([^;{]+\)\s*\{', source)
assert definition
function = source[definition.start() + len('void drawAlphaIcon('):].split('\nvoid drawWiFiSignal(', 1)[0]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <new>
#include "ui_alpha_icons.h"
static bool denyAllocation = false;
void* operator new(size_t size) {
    assert(!denyAllocation);
    if (void* result = std::malloc(size)) return result;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
namespace lgfx {
struct rgb565_t {
    uint16_t raw;
    explicit rgb565_t(uint16_t value) : raw(value) {}
    uint8_t R8() const { return ((raw >> 11) << 3) | (raw >> 13); }
    uint8_t G8() const { const auto value = (raw >> 5) & 63; return (value << 2) | (value >> 4); }
    uint8_t B8() const { const auto value = raw & 31; return (value << 3) | (value >> 2); }
};
uint32_t color888(uint8_t r, uint8_t g, uint8_t b) { return (r << 16) | (g << 8) | b; }
}
bool uiFrameReady = true;
static unsigned services = 0, reads = 0, writes = 0;
void serviceUiAudio() { ++services; }
struct TestCanvas {
    uint32_t before = 0x12345678;
    uint16_t pixels[320 * 240] = {};
    uint32_t after = 0x87654321;
    uint16_t readPixel(int x, int y) {
        assert(uiFrameReady); ++reads;
        return x < 0 || y < 0 || x >= 320 || y >= 240 ? 0 : pixels[y * 320 + x];
    }
    void drawPixel(int x, int y, uint32_t color) {
        ++writes;
        if (x >= 0 && y >= 0 && x < 320 && y < 240)
            pixels[y * 320 + x] = ((color >> 8) & 0xF800) | ((color >> 5) & 0x7E0) | ((color >> 3) & 31);
    }
    // No PNG API: decoding an icon here would reintroduce the crash path.
} target;
TestCanvas& canvas() { return target; }
static void* forbiddenAllocation() { assert(false && "icon requested heap storage"); return nullptr; }
#define malloc(...) forbiddenAllocation()
#define calloc(...) forbiddenAllocation()
#define realloc(...) forbiddenAllocation()
#define heap_alloc_dma(...) forbiddenAllocation()
void drawAlphaIcon(''' + function + r'''
#undef malloc
#undef calloc
#undef realloc
#undef heap_alloc_dma
int main() {
    const UiAlphaIcon* icons[] = {
        &ui_alpha_player_rewind_15, &ui_alpha_player_forward_30,
        &ui_alpha_player_pause, &ui_alpha_player_play,
        &ui_alpha_player_previous_track, &ui_alpha_player_next_track,
        &ui_alpha_back_chevron, &ui_alpha_wifi, &ui_alpha_wifi_fair,
        &ui_alpha_wifi_weak, &ui_alpha_wifi_dot,
        &ui_alpha_list_card,
        &ui_alpha_list_card_focused,
        &ui_alpha_list_card_wide,
        &ui_alpha_list_card_wide_focused,
        &ui_alpha_list_pager,
        &ui_alpha_list_pager_active,
        &ui_alpha_list_pager_thumb_1,
        &ui_alpha_list_pager_thumb_2,
        &ui_alpha_list_pager_thumb_3,
        &ui_alpha_list_rail,
        &ui_alpha_list_rail_favorites,
        &ui_alpha_home_brand_radio,
        &ui_alpha_home_focus,
        &ui_alpha_home_icon_favorites,
        &ui_alpha_home_icon_radio,
        &ui_alpha_home_icon_settings,
        &ui_alpha_home_icon_shows,
        &ui_alpha_home_tile_favorites,
        &ui_alpha_home_tile_radio,
        &ui_alpha_home_tile_settings,
        &ui_alpha_home_tile_shows,
        &ui_alpha_home_weather_clear,
        &ui_alpha_home_weather_cloudy,
        &ui_alpha_home_weather_mist,
        &ui_alpha_home_weather_partly,
        &ui_alpha_home_weather_rain,
        &ui_alpha_home_weather_snow,
        &ui_alpha_home_weather_storm,
        &ui_alpha_home_weather_unknown,
    };
    for (const auto* icon : icons) {
        assert(icon->width > 0 && icon->width <= 320 && icon->height > 0 && icon->height <= 240);
        unsigned pixels = 0;
        for (unsigned n = 0; n < icon->runCount; ++n) {
            assert(icon->runs[n].count > 0);
            pixels += icon->runs[n].count;
        }
        assert(pixels == icon->width * icon->height);
        for (bool readable : {true, false}) {
            uiFrameReady = readable;
            for (int repeat = 0; repeat < 100; ++repeat) {
                reads = writes = services = 0;
                denyAllocation = true;
                drawAlphaIcon(*icon, 52, 164);
                // Exercise clipping on all four display edges too.
                drawAlphaIcon(*icon, -10, -10);
                drawAlphaIcon(*icon, 315, 235);
                denyAllocation = false;
                assert(writes > 0 && services == 3 * (icon->height / 8));
                assert(readable ? reads > 0 : reads == 0);
                assert(target.before == 0x12345678 && target.after == 0x87654321);
            }
        }
    }
}
'''
with tempfile.TemporaryDirectory(prefix='radiohead-alpha-') as temporary:
    out = Path(temporary)
    path = out / 'fixture.cpp'
    path.write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', '-fsanitize=address,undefined',
                    '-I' + str(ROOT / '.pio/ui_assets'), str(path), '-o', str(out / 'check')], check=True)
    subprocess.run([str(out / 'check')], check=True)
print('Production alpha icons: 40 assets, 100 repeats, allocation-forbidden readable/fallback drawing and clipping passed')
