"""Host-check production encoder timing and sleep/wake release consumption."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'src/device_control.cpp').read_text()


def function(signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


constants = '\n'.join(re.findall(
    r'constexpr unsigned long kButton(?:Debounce|Hold)Ms = \d+;', source))
fixture = r'''
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
constexpr int PIN_SW = 38, PIN_K0 = 6, LOW = 0, HIGH = 1;
enum class ButtonEvent : uint8_t { None, Push, Hold };
bool encoderDown = false;
bool releaseScenario = false;
unsigned long clockMs = 0;
unsigned long millis() { return clockMs; }
void delay(unsigned long ms) { clockMs += ms; }
int digitalRead(int pin) {
    if (!releaseScenario) return pin == PIN_SW && encoderDown ? LOW : HIGH;
    // Both buttons must be released; include a bounced wake press.
    const bool down = pin == PIN_SW ? clockMs < 40 || (clockMs >= 55 && clockMs < 58)
                                   : clockMs < 70 || (clockMs >= 80 && clockMs < 83);
    return down ? LOW : HIGH;
}
'''
checks = r'''
int main() {
    const auto none = ButtonEvent::None;
    auto poll = [](unsigned long now, bool down) {
        encoderDown = down;
        return pollEncoderButton(now);
    };
    assert(poll(0, false) == none);
    assert(poll(100, true) == none);
    assert(poll(110, false) == none); // contact bounce is not a click
    assert(poll(140, false) == none);
    assert(poll(200, true) == none);
    assert(poll(225, true) == none);
    assert(poll(400, false) == none);
    assert(poll(425, false) == ButtonEvent::Push);
    assert(poll(450, false) == none);
    assert(poll(500, true) == none);
    assert(poll(525, true) == none);
    assert(poll(1225, true) == none); // old 700-ms hold no longer sleeps
    assert(poll(3524, true) == none);
    assert(poll(3525, true) == ButtonEvent::Hold);
    assert(poll(6000, true) == none); // once per hold
    assert(poll(6100, false) == none);
    assert(poll(6125, false) == none); // hold release never mutes
    assert(poll(6200, true) == none);
    assert(poll(6225, true) == none);
    assert(poll(6300, false) == none);
    assert(poll(6325, false) == ButtonEvent::Push);
    const unsigned long start = ULONG_MAX - 1000;
    assert(poll(start, true) == none);
    assert(poll(start + 25, true) == none);
    assert(poll(start + 3024, true) == none);
    assert(poll(start + 3025, true) == ButtonEvent::Hold); // millis rollover
    releaseScenario = true;
    clockMs = 0;
    waitForPowerButtonsRelease();
    assert(clockMs >= 107 && clockMs <= 108);
    clockMs = 200;
    waitForPowerButtonsRelease();
    assert(clockMs == 225); // already released still needs stable debounce
    puts("Encoder 3-second hold, bounce, repeat/release, rollover and wake-release checks passed.");
}
'''
out = root / '.pio/encoder_power_native'
out.mkdir(parents=True, exist_ok=True)
translation_unit = out / 'encoder_power.cpp'
translation_unit.write_text(fixture + constants + '\n' +
                            function('void waitForPowerButtonsRelease()') + '\n' +
                            function('ButtonEvent pollEncoderButton(') + checks)
exe = out / 'encoder_power'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                str(translation_unit), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
