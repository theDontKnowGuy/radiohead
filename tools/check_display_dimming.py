"""Host-check production backlight timing against encoder updates and rollover."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'src/device_control.cpp').read_text()
start = source.index('void updatePowerState(')
body = source.index('{', start)
end, depth = body + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1

fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
constexpr int PIN_K0 = 6, LOW = 0;
constexpr uint16_t AUTO_DIM_NEVER_SECONDS = 0;
uint16_t autoDimSeconds = 30;
volatile unsigned long lastInteraction = 0;
unsigned long lastVolChange = 0;
bool forceRedraw = false, isDimmed = false;
uint32_t clockMs = 0;
int brightness = 255, brightnessWrites = 0;
bool encoderUpdatesOnClockRead = false;
unsigned long millis() {
    if (encoderUpdatesOnClockRead) lastInteraction = clockMs + 1;
    return clockMs;
}
void setBrightness(int duty) { brightness = duty; ++brightnessWrites; }
int digitalRead(int) { return 1; }
void factoryReset() { assert(false); }
void goToSleep() { assert(false); }
'''
checks = r'''
int main() {
    // The encoder can record a newer timestamp than the loop's cached now.
    clockMs = 1001; lastInteraction = 1001;
    updatePowerState(1000);
    assert(!isDimmed && brightness == 255 && brightnessWrites == 0);
    // A further task update after the snapshot must not corrupt this check.
    encoderUpdatesOnClockRead = true;
    updatePowerState(1000);
    encoderUpdatesOnClockRead = false;
    assert(!isDimmed && brightnessWrites == 0);
    // Preserve the existing strict timeout boundary and once-only PWM writes.
    lastInteraction = 1000; clockMs = 31000;
    updatePowerState(clockMs);
    assert(!isDimmed);
    clockMs = 31001; updatePowerState(clockMs);
    assert(isDimmed && brightness == 20 && brightnessWrites == 1);
    clockMs = 31002; updatePowerState(clockMs);
    assert(brightnessWrites == 1);
    lastInteraction = 31003; clockMs = 31003;
    updatePowerState(31002);
    assert(!isDimmed && brightness == 255 && brightnessWrites == 2);
    // millis() is 32-bit on ESP32, including when the host unsigned long is 64-bit.
    lastInteraction = UINT32_MAX - 999; clockMs = 29000;
    updatePowerState(clockMs);
    assert(!isDimmed);
    clockMs = 29001; updatePowerState(clockMs);
    assert(isDimmed && brightness == 20);
    // A newer interaction crossing rollover is still activity, not idle time.
    lastInteraction = 1; clockMs = 2;
    updatePowerState(UINT32_MAX);
    assert(!isDimmed && brightness == 255);
    // Never remains disabled even after a long idle period.
    autoDimSeconds = AUTO_DIM_NEVER_SECONDS;
    lastInteraction = 0; clockMs = UINT32_MAX;
    updatePowerState(clockMs);
    assert(!isDimmed && brightness == 255);
    puts("Backlight encoder timing, timeout, wake, rollover and Never checks passed.");
}
'''
out = root / '.pio/display_dimming_native'
out.mkdir(parents=True, exist_ok=True)
translation_unit = out / 'display_dimming.cpp'
translation_unit.write_text(fixture + source[start:end] + checks)
exe = out / 'display_dimming'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                str(translation_unit), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
