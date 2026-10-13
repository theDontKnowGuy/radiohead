"""Host-check the production encoder task, diagnostic snapshot and GET route."""
from pathlib import Path
import json
import re
import subprocess

root = Path(__file__).resolve().parents[1]
control = (root / 'src/device_control.cpp').read_text()
header = (root / 'include/device_control.h').read_text()
pins = (root / 'include/app_state.h').read_text()
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
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
constexpr int HIGH=1, INPUT_PULLUP=5, pdPASS=1, HTTP_GET=0;
int encoderMux=0, lockDepth=0, createResult=0;
volatile int encoderPos=0;
volatile unsigned long lastInteraction=0;
std::vector<unsigned> sequence;
size_t sample=0;
std::map<int,int> modes;
struct EndSampling {};
void portENTER_CRITICAL(int*) { assert(lockDepth++ == 0); }
void portEXIT_CRITICAL(int*) { assert(--lockDepth == 0); }
unsigned long millis() { return sample + 1; }
void pinMode(int pin, int mode) { modes[pin]=mode; }
void vTaskDelay(int) { if (++sample == sequence.size()) throw EndSampling{}; }
int xTaskCreatePinnedToCore(void(*)(void*), const char*, int, void*, int, void*, int) {
    return createResult;
}
struct Server {
    std::map<std::string,std::function<void()>> routes;
    std::string body, cache;
    int status=0;
    void on(const char* path, int, std::function<void()> handler) { routes[path]=handler; }
    void sendHeader(const char*, const char* value) { cache=value; }
    void send(int code, const char*, const char* value) { status=code; body=value; }
} server;
'''
fixture += '\n'.join(re.findall(
    r'constexpr int PIN_(?:A|B|SW|K0) = \d+;', pins)) + '\n'
fixture += function(header, 'struct EncoderDiagnostics') + ';\n'
fixture += r'''
EncoderDiagnostics encoderState;
int digitalRead(int pin) {
    if (pin == PIN_SW) return HIGH;
    const unsigned ab=sequence[std::min(sample,sequence.size()-1)];
    return pin == PIN_A ? ((ab >> 1) & 1) : (ab & 1);
}
'''
fixture += '\n'.join(function(control, signature) for signature in (
    'void incrementTouchCounter(', 'void taskControl(', 'void startDeviceControl(',
    'int consumeEncoderDetents(', 'EncoderDiagnostics encoderDiagnostics('))
route_start = web.index('    server.on("/api/device/encoder-debug"')
route_end = web.index('    server.on(', route_start + 10)
fixture += '\nvoid registerRoute() {\n' + web[route_start:route_end] + '}\n'
fixture += r'''
void run(std::vector<unsigned> input) {
    sequence=input; sample=0;
    try { taskControl(nullptr); } catch (const EndSampling&) {}
    assert(lockDepth == 0);
}
int main() {
    startDeviceControl(); assert(!encoderState.taskCreated);
    createResult=pdPASS; startDeviceControl(); assert(encoderState.taskCreated);
    run({3,2,0,1,3});
    auto snapshot=encoderDiagnostics();
    assert(snapshot.samples==5 && snapshot.aEdges==2 && snapshot.bEdges==2);
    assert(snapshot.negativeTicks==4 && snapshot.positiveTicks==0);
    assert(snapshot.invalidTransitions==0 && snapshot.rawA && snapshot.rawB);
    assert(modes[PIN_A]==INPUT_PULLUP && modes[PIN_B]==INPUT_PULLUP);
    assert(modes[PIN_SW]==INPUT_PULLUP && modes[PIN_K0]==INPUT_PULLUP);
    assert(consumeEncoderDetents()==-1);
    assert(encoderDiagnostics().negativeTicks==4); // observing never consumes counts
    run({3,1,0,2,3}); assert(consumeEncoderDetents()==1);
    assert(encoderState.positiveTicks==4);
    run({3,2,3,2,3}); assert(consumeEncoderDetents()==0); // one channel only
    run({3,0,3}); assert(encoderState.invalidTransitions==2);
    assert(consumeEncoderDetents()==0); // simultaneous edges are rejected
    encoderState.samples=encoderState.aEdges=encoderState.bEdges=UINT32_MAX;
    encoderState.invalidTransitions=UINT32_MAX;
    run({0,3});
    assert(encoderState.samples==UINT32_MAX && encoderState.aEdges==UINT32_MAX);
    assert(encoderState.bEdges==UINT32_MAX && encoderState.invalidTransitions==UINT32_MAX);
    registerRoute(); server.routes.at("/api/device/encoder-debug")();
    assert(server.status==200 && server.cache=="no-store");
    puts(server.body.c_str());
}
'''
out = root / '.pio/encoder_diagnostics_native'
out.mkdir(parents=True, exist_ok=True)
translation_unit = out / 'encoder_diagnostics.cpp'
translation_unit.write_text(fixture)
exe = out / 'encoder_diagnostics'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                str(translation_unit), '-o', str(exe)], check=True)
response = json.loads(subprocess.check_output([str(exe)], text=True))
for key, pin in [('pinA', 'A'), ('pinB', 'B'), ('pinSwitch', 'SW')]:
    assert response[key] == int(re.search(rf'PIN_{pin} = (\d+)', pins)[1])
assert response['taskCreated'] and response['samples'] == 2**32 - 1
assert response['invalidTransitions'] == 2**32 - 1
print('Encoder rotation, task creation, snapshot, saturation and debug JSON checks passed.')
