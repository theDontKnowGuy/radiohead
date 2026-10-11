"""Exercise production touch snapshots, debug routes and overlay restoration on host."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/touch_diagnostics_native'
out.mkdir(parents=True, exist_ok=True)
control = (root / 'src/device_control.cpp').read_text()
display = (root / 'src/display.cpp').read_text()
web = (root / 'src/web_server.cpp').read_text()


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    end, depth = body + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <functional>
#include <map>
#include "device_control.h"
namespace lgfx {
using swap565_t = uint16_t;
struct touch_point_t { int32_t x = 0, y = 0; };
}
TouchDiagnostics touchState;
unsigned long tick = 0;
unsigned long millis() { return tick; }
unsigned long micros() { return tick * 1000; }
constexpr unsigned long kTouchPollMs = 8;
constexpr uint16_t TFT_RED = 0xf800, TFT_WHITE = 0xffff;
bool forceRedraw = false, uiFrameReady = true;
unsigned reads = 0, paints = 0, patches = 0, dots = 0;
struct Frame {
    std::array<uint16_t, 320 * 240> pixels;
    void* getBuffer() { return pixels.data(); }
} uiFrame;
struct Tft {
    std::array<uint16_t, 320 * 240> pixels;
    bool detected = false;
    int32_t rawX = 0, rawY = 0;
    struct Config { uint16_t x_min=300, y_min=400, x_max=3900, y_max=3900; };
    Config config() const { return {}; }
    Tft* touch() { return this; }
    int width() { return 320; }
    int height() { return 240; }
    void setTouchCalibrate(uint16_t*) {}
    bool getTouchRaw(lgfx::touch_point_t* point) {
        ++reads; point->x=rawX; point->y=rawY; return detected;
    }
    void convertRawXY(lgfx::touch_point_t* point) {
        point->x -= 100; point->y -= 100;
    }
    void pushImage(int x, int y, int w, int h, const uint16_t* source) {
        assert(x>=0 && y>=0 && x+w<=320 && y+h<=240);
        ++patches;
        for (int row=0;row<h;++row) for (int col=0;col<w;++col)
            pixels[(y+row)*320+x+col]=source[row*w+col];
    }
    void fillCircle(int x,int y,int radius,uint16_t color) {
        ++dots;
        for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx)
            if(dx*dx+dy*dy<=radius*radius) pixels[(y+dy)*320+x+dx]=color;
    }
} tft;
struct { void println(const char*) {} } Serial;
TouchCalibration stored = {3677,459,3593,3739,475,464,535,3729};
bool haveSaved = true;
bool loadTouchCalibration(TouchCalibration& result) { result=stored; return haveSaved; }
void recordTouchPoll(bool,bool,TouchEvent,unsigned long,unsigned long,unsigned long) {}
enum class UiPage { Home, PodcastPlayer };
struct UiRenderState { bool dirty=false; UiPage page=UiPage::Home; };
UiRenderState uiControllerRenderState() { return {}; }
struct PodcastPlaybackSnapshot { bool active=false, paused=false; };
PodcastPlaybackSnapshot podcastPlaybackSnapshot() { return {}; }
bool wifiSignalLevel() { return false; }
bool firmwareUpdateOverlayActive = false;
bool releaseUpdateOverlayActive() { return false; }
bool homeStationTitleRefreshDue(unsigned long) { return false; }
void renderRadioUi(const UiRenderState&,const char*,bool) { ++paints; tft.pixels=uiFrame.pixels; }
void renderHomeStationTitleTick() { tft.pixels=uiFrame.pixels; }
void uiControllerMarkRendered() {}
bool webRestartScheduled = false, maintenance = false;
bool webMaintenanceBusy() { return maintenance; }
constexpr int HTTP_GET=0, HTTP_POST=1;
struct Server {
    std::map<int,std::function<void()>> routes;
    String enabled;
    String response;
    int status=0;
    void on(const char*,int method,std::function<void()> handler) { routes[method]=handler; }
    String arg(const char*) { return enabled; }
    void send(int code,const char*,const String& body) { status=code; response=body; }
} server;
void sendMaintenanceBusy() { server.status=409; }
'''
fixture += '\n'.join(function(control, signature) for signature in (
    'void initializeTouchCalibration()', 'const TouchDiagnostics& touchDiagnostics()',
    'void setTouchDebugEnabled(', 'bool touchDebugMarkerVisible(', 'TouchEvent pollTouchEvent('))
fixture += function(display, 'void serviceDisplayRefresh(')
fixture += function(web, 'String touchCalibrationJson(')
fixture += function(web, 'String touchDebugStateFieldsJson(')
routes = re.findall(r'    server\.on\("/api/device/touch-debug", HTTP_(?:GET|POST), \[\] \{.*?\n    \}\);', web, re.S)
assert len(routes) == 2
fixture += '\nvoid registerRoutes() {\n' + '\n'.join(routes) + '\n}\n'
fixture += r'''
int main() {
    assert(!touchDiagnostics().debugEnabled);
    initializeTouchCalibration();
    assert(touchDiagnostics().activeCalibration==stored);
    assert(std::string(touchDiagnostics().calibrationSource)=="saved at boot");
    haveSaved=false; initializeTouchCalibration();
    assert(touchDiagnostics().activeCalibration[0]==300);
    assert(std::string(touchDiagnostics().calibrationSource)=="panel defaults");
    assert(touchCalibrationJson(stored)=="[3677,459,3593,3739,475,464,535,3729]");
    registerRoutes();
    for(const char* invalid : {"", "2", "true", "-1", "1junk"}) {
        server.enabled=invalid;server.routes[HTTP_POST]();
        assert(server.status==400 && !touchState.debugEnabled);
    }
    maintenance=true;server.enabled="1";server.routes[HTTP_POST]();
    assert(server.status==409 && !touchState.debugEnabled);
    maintenance=false;webRestartScheduled=true;server.routes[HTTP_POST]();
    assert(server.status==409);webRestartScheduled=false;
    server.routes[HTTP_POST]();assert(server.status==200 && touchState.debugEnabled);
    assert(!touchDebugMarkerVisible(0)); // No fabricated contact on enable.
    assert(server.response.find("\"touchDebugEnabled\":true")!=std::string::npos);
    int16_t x=0,y=0;
    tft.detected=true;tft.rawX=260;tft.rawY=220;tick=8;
    assert(pollTouchEvent(x,y,tick)==TouchEvent::Begin);
    assert(reads==1 && x==160 && y==120 && touchState.rawX==260);
    assert(touchState.contact && touchState.onPanel && touchState.screenX==160);
    for(size_t i=0;i<uiFrame.pixels.size();++i) uiFrame.pixels[i]=i%0x7000;
    serviceDisplayRefresh(tick,"12:00",true);
    assert(tft.pixels[120*320+160]==TFT_RED);
    assert(uiFrame.pixels[120*320+160]!=TFT_RED);
    const unsigned initialPaints=paints;
    serviceDisplayRefresh(tick,"12:00",true);assert(dots==2); // no repeated SPI paint
    tft.rawX=280;tick=16;pollTouchEvent(x,y,tick);
    serviceDisplayRefresh(tick,"12:00",true);
    assert(tft.pixels[120*320+160]==uiFrame.pixels[120*320+160]);
    assert(tft.pixels[120*320+180]==TFT_RED && paints==initialPaints && patches==25);
    // Ordinary page/time redraw must not hide a stationary contact dot.
    serviceDisplayRefresh(tick,"12:01",true);assert(tft.pixels[120*320+180]==TFT_RED);
    tft.rawX=50;tft.rawY=500;tick=24;pollTouchEvent(x,y,tick);
    assert(touchState.contact && !touchState.onPanel && touchState.screenX==-50);
    serviceDisplayRefresh(tick,"12:01",true);assert(tft.pixels[227*320+12]==TFT_RED);
    tft.detected=false;tft.rawX=0;tft.rawY=0;tick=32;pollTouchEvent(x,y,tick);
    assert(!touchState.contact && touchState.contactSamples==3 && touchState.polls==4);
    assert(touchState.rawX==50 && touchState.rawY==500 && touchState.screenX==-50);
    assert(touchDebugMarkerVisible(tick)); // A rejected 8-ms poll must not erase it.
    serviceDisplayRefresh(tick,"12:01",true);assert(tft.pixels[227*320+12]==TFT_RED);
    assert(touchDebugMarkerVisible(773));
    serviceDisplayRefresh(773,"12:01",true);assert(tft.pixels[227*320+12]==TFT_RED);
    assert(!touchDebugMarkerVisible(774));
    serviceDisplayRefresh(774,"12:01",true);assert(tft.pixels==uiFrame.pixels);
    server.routes[HTTP_GET]();
    assert(server.response.find("\"touchContactSamples\":3")!=std::string::npos);
    assert(server.response.find("\"touchRaw\":[50,500]")!=std::string::npos);
    tft.detected=true;tft.rawX=260;tft.rawY=220;tick=800;pollTouchEvent(x,y,tick);
    serviceDisplayRefresh(tick,"12:01",true);
    server.enabled="0";server.routes[HTTP_POST]();assert(!touchState.debugEnabled);
    serviceDisplayRefresh(tick,"12:01",true);assert(tft.pixels==uiFrame.pixels);
    // Allocation-failure path restores the old marker via the normal renderer.
    uiFrameReady=false;setTouchDebugEnabled(true);tick=808;pollTouchEvent(x,y,tick);
    serviceDisplayRefresh(tick,"12:01",true);setTouchDebugEnabled(false);
    serviceDisplayRefresh(tick,"12:01",true);assert(tft.pixels==uiFrame.pixels);
    assert(reads==6); // diagnostics and display add no ADC reads
    touchState.contact=false;touchState.debugEnabled=true;
    touchState.lastContactAt=UINT32_MAX-100;
    assert(touchDebugMarkerVisible(0));
    assert(touchDebugMarkerVisible(648));
    assert(!touchDebugMarkerVisible(649)); // 750-ms expiry across millis rollover
    puts("Touch calibration snapshots, route validation, contact mapping and dot restoration passed.");
}
'''
source = out / 'diagnostics.cpp'
source.write_text(fixture)
exe = out / 'diagnostics'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', f'-I{root / "tools/native_ui"}',
                f'-I{root / "include"}', str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
