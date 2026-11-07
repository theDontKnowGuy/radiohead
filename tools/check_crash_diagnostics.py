"""Exercise the production selected-summary/clear code under ASan/UBSan."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / '.pio/crash_native'
out.mkdir(parents=True, exist_ok=True)
stubs = {
    'Arduino.h': '''#pragma once
#include <algorithm>
#include <string>
#include <cstdint>
#include <cstdio>
struct String : std::string {
 using std::string::string;
 bool reserve(size_t n) { std::string::reserve(n); return true; }
};
''',
    'esp_core_dump.h': '''#pragma once
#include <cstdint>
#include <cstddef>
#define APP_ELF_SHA256_SZ 65
using esp_err_t = int;
constexpr int ESP_OK=0;
struct esp_core_dump_summary_t {
 char exc_task[16]; uint32_t exc_pc;
 struct {uint32_t bt[16],depth; bool corrupted;} exc_bt_info;
 uint8_t app_elf_sha256[65];
 struct {uint32_t exc_cause,exc_vaddr;} ex_info;
};
int esp_core_dump_image_check();
int esp_core_dump_image_get(size_t*,size_t*);
int esp_core_dump_image_erase();
int esp_core_dump_get_summary(esp_core_dump_summary_t*);
int esp_core_dump_get_panic_reason(char*,size_t);
''',
    'esp_partition.h': '''#pragma once
#include <cstddef>
#include <cstdint>
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_COREDUMP=3;
struct esp_partition_t {size_t address,size;};
const esp_partition_t* esp_partition_find_first(int,int,const char*);
int esp_partition_read(const esp_partition_t*,size_t,void*,size_t);
''',
    'esp_rom_crc.h': '''#pragma once
#include <cstddef>
#include <cstdint>
inline uint32_t esp_rom_crc32_le(uint32_t v,const uint8_t* p,size_t n) {
 for(size_t i=0;i<n;++i) v=(v^p[i])*16777619U;
 return v;
}
''',
    'esp_system.h': '#pragma once\n',
}
for name, source in stubs.items():
    (out / name).write_text(source)
fixture = r'''
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "crash_diagnostics.cpp"
esp_partition_t part{0xff0000,65536};
std::vector<uint8_t> flash(65536,255);
bool missing=false,readFail=false,eraseFail=false;
int imageError=0,summaryError=0,erases=0;
esp_core_dump_summary_t fake{};
const esp_partition_t* esp_partition_find_first(int,int,const char*) {return missing?nullptr:&part;}
int esp_partition_read(const esp_partition_t*,size_t at,void* p,size_t n) {
 assert(at+n<=flash.size()); if(readFail)return -8;
 memcpy(p,flash.data()+at,n); return 0;
}
int esp_core_dump_image_check() {return imageError;}
int esp_core_dump_image_get(size_t* at,size_t* size) {*at=part.address; *size=3200;return 0;}
int esp_core_dump_get_summary(esp_core_dump_summary_t* p) {*p=fake;return summaryError;}
int esp_core_dump_get_panic_reason(char* p,size_t n) {snprintf(p,n,"assert failed: private-secret-password");return 0;}
int esp_core_dump_image_erase() {
 ++erases;if(eraseFail)return -1;
 std::fill(flash.begin(),flash.end(),255);return 0;
}
int main() {
 crashDiagnosticsBegin(); assert(crashDiagnosticsEmpty());
 std::cout<<crashDiagnosticsJson()<<'\n';
 assert(!crashDiagnosticsClear("00000000") && erases==0);
 uint32_t bytes=3200;memcpy(flash.data(),&bytes,4);
 strcpy(fake.exc_task,"loopTask");
 memset(fake.app_elf_sha256,'a',64);fake.app_elf_sha256[64]=0;
 fake.exc_pc=0x42001234;fake.ex_info.exc_cause=28;fake.ex_info.exc_vaddr=0;
 fake.exc_bt_info.depth=99;fake.exc_bt_info.corrupted=true;
 for(int i=0;i<16;++i)fake.exc_bt_info.bt[i]=0x42000000+i*4;
 crashDiagnosticsBegin(); assert(!crashDiagnosticsEmpty());
 std::cout<<crashDiagnosticsJson()<<'\n';
 const std::string savedId=id;
 for(int i=0;i<5;++i)crashDiagnosticsJson();
 assert(erases==0); // GETs never acknowledge/delete a dump.
 crashDiagnosticsBegin();assert(savedId==id); // Boot does not erase.
 assert(!crashDiagnosticsClear("stale-id") && erases==0);
 eraseFail=true;assert(!crashDiagnosticsClear(id) && !crashDiagnosticsEmpty());
 eraseFail=false;
 imageError=-7;crashDiagnosticsBegin();assert(!crashDiagnosticsEmpty());
 std::cout<<crashDiagnosticsJson()<<'\n';
 assert(crashDiagnosticsClear(id) && crashDiagnosticsEmpty());
 std::cout<<crashDiagnosticsJson()<<'\n';
 readFail=true;crashDiagnosticsBegin();assert(!crashDiagnosticsEmpty());
 std::cout<<crashDiagnosticsJson()<<'\n';
 readFail=false;missing=true;crashDiagnosticsBegin();
 std::cout<<crashDiagnosticsJson()<<'\n';
}
'''
(out / 'check.cpp').write_text(fixture)
subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', '-g',
                '-I', str(out), '-I', str(ROOT / 'include'), '-I', str(ROOT / 'src'),
                str(out / 'check.cpp'), '-o', str(out / 'check')], check=True)
result = subprocess.check_output([str(out / 'check')], text=True)
rows = [json.loads(line) for line in result.splitlines()]
assert [r['state'] for r in rows] == ['empty', 'valid', 'invalid', 'empty', 'read_error', 'partition_error']
assert rows[1]['reason'] == 'assertion' and len(rows[1]['backtrace']) == 16
assert len(rows[1]['build']) == 64 and rows[1]['task'] == 'loopTask'
assert 'private-secret' not in result and 'registers' not in result
assert rows[1]['captureArmed'] is False and rows[3]['captureArmed'] is True
print('PASS: selected summary, bounded backtrace, no panic-text export, missing/invalid/read-error states, retention, stale clear and erase failure')
