"""Exercise production recorder/archive with a simulated clock, heap and filesystem.

ASan/UBSan cover record corruption, rolling windows, stalls, transient Wi-Fi
loss, allocation failure, page bounds, reboot archives and failed replacement.
This is not a hardware reset/RTC-retention test.
"""
from pathlib import Path
import json
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/radio_log_native'
out.mkdir(parents=True, exist_ok=True)
stubs = {
'Arduino.h': r'''#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
class String : public std::string {
public:
 using std::string::string;
 bool reserve(size_t n) { std::string::reserve(n); return true; }
};
extern uint32_t clockMs, clockUs;
inline uint32_t millis() { return clockMs; }
inline uint32_t micros() { return clockUs; }
''',
'media.h': r'''#pragma once
#include <cstdint>
enum class PlaybackState : uint8_t { Stopped, Connecting, Playing, Failed };
extern bool local;
extern int station;
extern PlaybackState state;
inline bool mediaLocalAudioAvailable() { return local; }
inline int mediaRequestedStation() { return station; }
inline bool mediaTestActive() { return false; }
inline PlaybackState mediaPlaybackState() { return state; }
''',
'app_state.h': r'''#pragma once
#include <cstdint>
extern bool podcastMode, radioMuted;
extern int mainVal;
struct AudioFake {
 uint32_t fill = 100000;
 uint32_t inBufferFilled() { return fill; }
 uint32_t getInBufferSize() { return 200000; }
 bool isRunning() { return true; }
};
extern AudioFake audio;
''',
'WiFi.h': r'''#pragma once
#include <cstdint>
constexpr int WL_CONNECTED=3, ARDUINO_EVENT_WIFI_STA_DISCONNECTED=1;
using WiFiEvent_t=int;
struct WiFiEventInfo_t { struct { uint32_t reason; } wifi_sta_disconnected; };
struct WifiFake {
 int statusValue=3;
 void (*callback)(WiFiEvent_t, WiFiEventInfo_t)=nullptr;
 int status() { return statusValue; }
 int RSSI() { return -55; }
 void onEvent(void (*fn)(WiFiEvent_t, WiFiEventInfo_t), int) { callback=fn; }
};
extern WifiFake WiFi;
''',
'esp_attr.h': '#pragma once\n#define RTC_NOINIT_ATTR\n',
'esp_system.h': r'''#pragma once
enum { ESP_RST_POWERON=1, ESP_RST_PANIC=4, ESP_RST_INT_WDT=5, ESP_RST_TASK_WDT=6, ESP_RST_WDT=7, ESP_RST_BROWNOUT=9 };
extern int resetReason;
inline int esp_reset_reason() { return resetReason; }
''',
'esp_app_desc.h': r'''#pragma once
#include <cstdint>
struct esp_app_desc_t { uint8_t app_elf_sha256[32]; };
inline const esp_app_desc_t* esp_app_get_description() { static esp_app_desc_t d{}; return &d; }
''',
'esp_heap_caps.h': r'''#pragma once
#include <cstddef>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_SPIRAM=4;
struct multi_heap_info_t { size_t total_free_bytes, minimum_free_bytes, largest_free_block; };
inline void heap_caps_get_info(multi_heap_info_t* p, uint32_t) { *p={50000,40000,30000}; }
inline size_t heap_caps_get_free_size(uint32_t) { return 1000000; }
inline int heap_caps_register_failed_alloc_callback(void (*)(size_t,uint32_t,const char*)) { return 0; }
''',
'LittleFS.h': r'''#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
constexpr int FILE_WRITE=1, FILE_READ=2;
inline std::map<std::string,std::vector<uint8_t>> files;
inline bool failWrite=false, failRename=false;
struct File {
 std::vector<uint8_t>* data=nullptr;
 size_t at=0;
 explicit operator bool() const { return data; }
 size_t size() const { return data ? data->size() : 0; }
 size_t write(const uint8_t* bytes,size_t n) {
  if (!data || failWrite) return 0;
  data->insert(data->end(),bytes,bytes+n); return n;
 }
 size_t read(uint8_t* bytes,size_t n) {
  if (!data) return 0;
  n=std::min(n,data->size()-at); memcpy(bytes,data->data()+at,n); at+=n; return n;
 }
 bool seek(size_t n) { if (!data || n>data->size()) return false; at=n;return true; }
 void close() { data=nullptr; }
};
struct Filesystem {
 File open(const char* path,int mode) {
  if (mode==FILE_WRITE) files[path].clear();
  auto i=files.find(path); return i==files.end() ? File{} : File{&i->second,0};
 }
 bool rename(const char* from,const char* to) {
  if(failRename) return false;
  files[to]=files.at(from); files.erase(from); return true;
 }
 void remove(const char* path) { files.erase(path); }
};
inline Filesystem LittleFS;
''',
}
for name, source in stubs.items():
    (out / name).write_text(source)
fixture = r'''
#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "app_state.h"
#include "media.h"
#include "WiFi.h"
uint32_t clockMs=0, clockUs=0;
bool podcastMode=false, radioMuted=false, local=true;
int mainVal=10, station=0, resetReason=1;
PlaybackState state=PlaybackState::Playing;
AudioFake audio;
WifiFake WiFi;
#include "radio_diagnostics.cpp"
#include "settings_radio_diagnostics.cpp"
void advance(uint32_t ms) { clockMs+=ms; clockUs+=ms*1000; }
bool hasEvent(uint32_t event) {
 for(const auto& r:retained.records) if(RadioLog::valid(r) && (r.events & event)) return true;
 return false;
}
int main() {
 using namespace RadioLog;
 static_assert(sizeof(Store)<7500);
 size_t offset=99;
 for (const char* bad : {"", "-1", "+1", " 1", "56", "999999999999999999", "1x"}) assert(!parseOffset(bad,offset));
 assert(parseOffset("55",offset) && offset==55);
 radioDiagnosticsBegin(true);
 assert(!recovered && validHeader(retained.header));
 assert(files.empty()); // No flash writes during initial startup or playback.
 for(int i=0;i<20;++i) { advance(250); radioDiagnosticsPcmBlock(); radioDiagnosticsTick(); }
 assert(!hasEvent(RadioPcmStall));
 audio.fill=0;
 for(int i=0;i<8;++i) { advance(250); radioDiagnosticsTick(); }
 assert(hasEvent(RadioPcmStall));
 for(int i=0;i<8;++i) { advance(250); radioDiagnosticsPcmBlock(); radioDiagnosticsTick(); }
 assert(hasEvent(RadioPcmResumed));
 WiFi.callback(1,{{201}}); // Disconnect/reconnect between polling ticks.
 radioDiagnosticsAllocationFailure(12345);
 radioDiagnosticsSignal(RadioSlow | RadioLost | RadioOutputTimeout);
 radioDiagnosticsPhase(RadioPhase::Audio);
 advance(150);
 radioDiagnosticsPhase(RadioPhase::Web);
 advance(250);
 radioDiagnosticsPhase(RadioPhase::Audio);
 for(int i=0;i<8;++i) { advance(250); radioDiagnosticsPcmBlock(); radioDiagnosticsTick(); }
 assert(hasEvent(RadioWifi) && hasEvent(RadioAllocation) && hasEvent(RadioLoopGap));
 assert(hasEvent(RadioSlow) && hasEvent(RadioLost) && hasEvent(RadioOutputTimeout));
 assert(files.empty());
 std::cout << radioDiagnosticsJson("live",32) << '\n';
 assert(radioDiagnosticsJson("live",56).empty());
 assert(radioDiagnosticsJson("bad",0).empty());
 // Validate surviving records independently after a torn write/bit flip.
 Record first=retained.records[32], damaged=first;
 assert(valid(first));
 for(size_t byte=0;byte<sizeof(damaged);++byte) {
  damaged=first; reinterpret_cast<uint8_t*>(&damaged)[byte]^=1; assert(!valid(damaged));
 }
 retained.records[33].checksum=0;
 assert(!valid(retained.records[33]) && valid(retained.records[32]));
 for(size_t n=0;n<Samples*3;++n) save(first,false);
 assert(valid(retained.records[32]) && retained.records[32].sequence==first.sequence);
 resetReason=ESP_RST_PANIC;
 radioDiagnosticsBegin(true);
 assert(recovered && archiveOk && retained.header.boot==2);
 assert(files.count("/radio-crash.bin") && files.count("/radio-previous.bin"));
 std::cout << radioDiagnosticsJson("crash",32) << '\n';
 const auto crash=files.at("/radio-crash.bin");
 failWrite=true;
 assert(!archive(retained,3,true));
 assert(files.at("/radio-crash.bin")==crash);
 failWrite=false; failRename=true;
 assert(!archive(retained,3,true));
 assert(files.at("/radio-crash.bin")==crash);
 failRename=false;
 resetReason=3;
 radioDiagnosticsBegin(true);
 assert(files.at("/radio-crash.bin")==crash); // Normal restart cannot erase last crash.
 resetReason=ESP_RST_POWERON;
 radioDiagnosticsBegin(true);
 assert(!recovered && retained.header.boot==1 && files.at("/radio-crash.bin")==crash);
 std::cout << radioDiagnosticsJson("previous",0) << '\n';
 // Unavailable/corrupt archive is explicit, never serialized as real evidence.
 files["/radio-crash.bin"][0]^=1; // Bad header must not leak invalid metadata.
 std::cout << radioDiagnosticsJson("crash",0) << '\n';
 files["/radio-crash.bin"].resize(3);
 std::cout << radioDiagnosticsJson("crash",0) << '\n';
}
'''
unit = out / 'check.cpp'
unit.write_text(fixture)
exe = out / 'check'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-I', str(out),
                '-I', str(root/'include'), '-I', str(root/'src'),
                str(unit), '-o', str(exe)], check=True)
result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
reports = [json.loads(line) for line in result.stdout.splitlines()]
live, crash, previous, corrupt_header, corrupt = reports
assert len(live['records']) <= 8 and live['nextOffset'] == 40
assert any(r['pcmIdleMs'] >= 1000 for r in live['records'])
assert any(r['allocationFailures'] == 1 and r['failedBytes'] == 12345 for r in live['records'])
assert crash['endingResetReason'] == 4 and crash['boot'] == 1
assert previous['available'] and not corrupt['available'] and corrupt['records'] == []
assert not corrupt_header['available'] and corrupt_header['build'] == '' and corrupt_header['records'] == []
print('Radio diagnostics: sampler, PCM stalls/recovery, transient Wi-Fi loss, allocation failure, timing, ring wrap, corruption, paging and reboot/archive failure checks passed.')

# Exercise collector page traversal, record ordering and reboot rejection without
# network or serial access. Production exported JSON above is the response base.
import importlib.util
from urllib.parse import parse_qs, urlsplit
spec = importlib.util.spec_from_file_location('collector', root / 'tools/collect_radio_diagnostics.py')
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)
class Reply:
    def __init__(self, data): self.data = data
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def read(self, size): return self.data[:size]
calls = []
reboot = False
def fetch(url, timeout):
    at = int(parse_qs(urlsplit(url).query)['offset'][0])
    calls.append(at)
    page = dict(live, offset=at, nextOffset=at + 8 if at < 48 else -1)
    page['records'] = [dict(live['records'][0], seq=56-at)]
    if reboot and at: page['boot'] += 1
    return Reply(json.dumps(page).encode())
collector.urlopen = fetch
collected = collector.collect('http://radio.local', 'live')
assert calls == list(range(0, 56, 8))
assert [r['seq'] for r in collected['records']] == list(range(8, 57, 8))
reboot = True
try:
    collector.collect('http://radio.local', 'live')
except ValueError as error:
    assert 'restarted' in str(error)
else:
    raise AssertionError('Collector accepted mixed boot pages')
print('Collector pagination, ordering, annotations and reboot rejection passed.')
