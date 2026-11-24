"""Exercise production orientation storage, startup, HTTP and held-touch handling."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/display_orientation_native'
out.mkdir(parents=True, exist_ok=True)


def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    body = source.index('{', start)
    end, depth = body + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


web = (root / 'src/web_server.cpp').read_text()
routes = re.findall(
    r'    server\.on\("/api/device/display-orientation", HTTP_(?:GET|POST), \[\] \{.*?\n    \}\);',
    web, re.S,
)
assert len(routes) == 2
fixture = r'''
#include "Arduino.h"
#include "Preferences.h"
#include "touch_gesture.h"
#include <cassert>
#include <functional>
#include <iostream>
Preferences pref;
bool displayFlipped = false, forceRedraw = false, webRestartScheduled = false;
bool maintenanceBusy = false;
bool webMaintenanceBusy() { return maintenanceBusy; }
constexpr int HTTP_GET = 0, HTTP_POST = 1;
constexpr int TFT_BLK = 1, OUTPUT = 1, LOW = 0, TFT_BLACK = 0;
constexpr uint32_t TOUCH_DEBUG_MARKER_HOLD_MS = 750;
using gpio_num_t = int;
uint32_t tick = 1000;
uint32_t millis() { return tick; }
void rtc_gpio_hold_dis(int) {}
void rtc_gpio_deinit(int) {}
void pinMode(int, int) {}
void digitalWrite(int, int) {}
void delay(int) {}
void ledcAttach(int, int, int) {}
void setBrightness(int) {}
unsigned touchEnds = 0;
void uiControllerTouchEnd() { ++touchEnds; }
TouchPressLatch pressLatch;
struct { bool contact = false, onPanel = false; uint32_t lastContactAt = 0; } touchState;
struct {
    int rotation = -1, rotationWrites = 0, fills = 0;
    void init() {}
    void setRotation(int value) { rotation = value; ++rotationWrites; }
    void fillScreen(int) { assert(rotation == (displayFlipped ? 3 : 1)); ++fills; }
} tft;
struct {
    std::map<int, std::function<void()>> routes;
    String value, configHeader, response, cache;
    int status = 0;
    void on(const char*, int method, std::function<void()> handler) { routes[method] = handler; }
    String arg(const char* key) { assert(std::string(key) == "flipped"); return value; }
    String header(const char* key) { assert(std::string(key) == "X-Radiohead-Config"); return configHeader; }
    void sendHeader(const char*, const char* value) { cache = value; }
    void send(int code, const char*, const String& body) { status = code; response = body; }
    void request(int method) { cache.clear(); routes.at(method)(); assert(cache == "no-store"); }
} server;
void sendMaintenanceBusy() { server.send(409, "application/json; charset=utf-8", "{}"); }
'''
production = '\n'.join([
    function('src/settings.cpp', 'bool loadDisplayFlipped()'),
    function('src/settings.cpp', 'bool saveDisplayFlipped(bool flipped)'),
    function('src/device_control.cpp', 'void cancelTouchUntilRelease()'),
    function('src/display.cpp', 'void initializeDisplay()'),
    function('src/display.cpp', 'void setDisplayFlipped(bool flipped)'),
    'void registerRoutes() {\n' + '\n'.join(routes) + '\n}',
])
checks = r'''
int main() {
    using namespace fakeNvs;
    values["radio/vol"] = uint8_t(8);
    values["touch/data"] = std::string("calibration");
    initializeDisplay();
    assert(!displayFlipped && tft.rotation == 1 && tft.fills == 1);
    registerRoutes();
    server.request(HTTP_GET);
    assert(server.status == 200 && server.response == "{\"flipped\":false}");
    server.value = "1";
    server.request(HTTP_POST);
    assert(server.status == 403 && writes == 0);
    server.configHeader = "1";
    for (const char* invalid : {"", "2", "-1", "01", "true", "1x", " 1", "1 ", "0.0"}) {
        server.value = invalid;
        server.request(HTTP_POST);
        assert(server.status == 400 && writes == 0 && tft.rotation == 1);
    }
    server.value = "1";
    maintenanceBusy = true;
    server.request(HTTP_POST);
    assert(server.status == 409 && writes == 0);
    maintenanceBusy = false;
    webRestartScheduled = true;
    server.request(HTTP_POST);
    assert(server.status == 409 && writes == 0);
    webRestartScheduled = false;
    for (bool* failure : {&failOpen, &failWrite}) {
        *failure = true;
        server.request(HTTP_POST);
        assert(server.status == 500 && writes == 0 && !displayFlipped && tft.rotation == 1);
        *failure = false;
    }
    int16_t x = 0, y = 0;
    assert(pressLatch.sample(true, 20, 20, tick, x, y) == TouchEvent::Begin);
    touchState.contact = touchState.onPanel = true;
    touchState.lastContactAt = tick;
    server.request(HTTP_POST);
    assert(server.status == 200 && server.response == "{\"flipped\":true}");
    assert(displayFlipped && tft.rotation == 3 && forceRedraw && touchEnds == 1);
    assert(!touchState.contact && !touchState.onPanel);
    assert(uint32_t(tick - touchState.lastContactAt) >= TOUCH_DEBUG_MARKER_HOLD_MS);
    assert(pressLatch.sample(true, 300, 220, ++tick, x, y) == TouchEvent::None);
    assert(pressLatch.sample(false, 0, 0, tick + 79, x, y) == TouchEvent::None);
    assert(pressLatch.sample(false, 0, 0, tick + 80, x, y) == TouchEvent::Release);
    assert(pressLatch.sample(true, 300, 220, tick + 81, x, y) == TouchEvent::Begin);
    forceRedraw = false;
    const auto rotations = tft.rotationWrites;
    server.request(HTTP_POST); // Retrying an explicit state cannot toggle it back.
    assert(server.status == 200 && displayFlipped && !forceRedraw);
    assert(tft.rotationWrites == rotations && touchEnds == 1);
    initializeDisplay(); // Simulated reboot, before calibration or startup artwork.
    assert(displayFlipped && tft.rotation == 3);
    const auto firstDevice = values;
    values.clear();
    initializeDisplay(); // Same binary, another device with its own NVS.
    assert(!displayFlipped && tft.rotation == 1);
    values = firstDevice;
    initializeDisplay();
    assert(displayFlipped && tft.rotation == 3);
    server.value = "0";
    server.request(HTTP_POST);
    assert(server.status == 200 && !displayFlipped && tft.rotation == 1);
    initializeDisplay();
    assert(!displayFlipped && tft.rotation == 1);
    assert(std::get<uint8_t>(values.at("radio/vol")) == 8);
    assert(std::get<std::string>(values.at("touch/data")) == "calibration");
    for (uint8_t invalid : {uint8_t(2), uint8_t(255)}) {
        values["radio/displayFlip"] = invalid;
        initializeDisplay();
        assert(!displayFlipped && tft.rotation == 1);
    }
    values["radio/displayFlip"] = std::string("wrong NVS type");
    initializeDisplay();
    assert(!displayFlipped && tft.rotation == 1);
    values.erase("radio/displayFlip"); // Existing factory reset clears radio NVS.
    initializeDisplay();
    assert(!displayFlipped && tft.rotation == 1);
    std::cout << "Orientation storage, boot, HTTP and held-touch checks passed.\n";
}
'''
(out / 'check.cpp').write_text(fixture + production + checks)
subprocess.run([
    'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
    f'-I{root / "tools/native_boot_mode"}', f'-I{root / "tools/native_ui"}',
    f'-I{root / "include"}', str(out / 'check.cpp'), '-o', str(out / 'check'),
], check=True)
subprocess.run([str(out / 'check')], check=True)
