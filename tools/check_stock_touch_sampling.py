"""Compare the production instrumented reader with the pinned stock XPT2046 reader."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/touch_stock_native'
out.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    end, depth = body + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


stock = (root / '.pio/libdeps/esp32s3/LovyanGFX/src/lgfx/v1/touch/Touch_XPT2046.cpp').read_text()
production = (root / 'src/app_state.cpp').read_text()
fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include "stock_touch_sampling.h"
uint8_t input[60], commands[60];
unsigned reads=0, records=0, resets=0;
bool baseInitOk=true;int busQueryStatus=0;size_t busCapacity=64;
using spi_host_device_t=int;constexpr int ESP_OK=0;
int spi_bus_get_max_transaction_len(int,size_t* length) { *length=busCapacity;return busQueryStatus; }
namespace lgfx {
struct touch_point_t { int16_t x=999,y=888;uint16_t size=42; };
void gpio_lo(int) {} void gpio_hi(int) {} int gpio_in(int) { return 0; }
namespace spi {
void release(int host) { assert(host==2);++resets; }
void beginTransaction(int,unsigned,int) {} void endTransaction(int) {}
void readBytes(int,uint8_t* data,size_t count) {
    assert(count==57);++reads;
    // Match the library helper's rounded word read from the backing buffer.
    memcpy(commands,data,60);memcpy(data,input,count);
}
}
struct Touch_XPT2046 {
    struct config_t { int spi_host=2,pin_cs=14,pin_int=-1;unsigned freq=1000000; };
    config_t _cfg;
    bool _inited=true;
    bool init() { _inited=baseInitOk;return baseInitOk; }
    uint_fast8_t getTouchRaw(touch_point_t* tp,uint_fast8_t count);
};
}
using namespace lgfx;
void transfer(uint8_t*,uint8_t*,size_t,const Touch_XPT2046::config_t*) { assert(false); }
struct DiagnosticTouchXPT2046: lgfx::Touch_XPT2046 {
    bool init();bool recover();
    uint_fast8_t getTouchRaw(lgfx::touch_point_t*,uint_fast8_t);
};
stock_touch_sampling::Frame last;
void recordTouchFrame(const stock_touch_sampling::Frame& frame) { ++records;last=frame; }
'''
# Stock uses a 57-byte buffer although the ESP32 helper reads 60 bytes from it.
# Padding the reference fixture fixes only this backing-storage size, so ASan
# can compare its unmodified protocol, pressure math and selection rules.
fixture += function(stock, 'uint_fast8_t Touch_XPT2046::getTouchRaw(').replace('uint8_t data[57]', 'uint8_t data[60] = {}')
fixture += function(production, 'bool DiagnosticTouchXPT2046::init(')
fixture += function(production, 'bool DiagnosticTouchXPT2046::recover(')
fixture += function(production, 'uint_fast8_t DiagnosticTouchXPT2046::getTouchRaw(')
fixture += r'''
void compare() {
    lgfx::Touch_XPT2046 stock;DiagnosticTouchXPT2046 instrumented;
    lgfx::touch_point_t a,b;
    const auto accepted=stock.getTouchRaw(&a,1);
    uint8_t stockCommands[60];memcpy(stockCommands,commands,60);
    const auto observed=instrumented.getTouchRaw(&b,1);
    assert(accepted==observed && a.x==b.x && a.y==b.y && a.size==b.size);
    assert(memcmp(stockCommands,commands,57)==0);
    assert(records*2==reads && last.accepted==bool(accepted));
}
void setValue(unsigned i,unsigned offset,uint16_t value) {
    input[i*8+offset]=value>>8;input[i*8+offset+1]=value&255;
}
int main() {
    uint8_t expected[60];stock_touch_sampling::prepare(expected);
    for(unsigned i=57;i<60;++i)assert(expected[i]==0);
    memset(input,0,sizeof(input));compare();
    memset(input,255,sizeof(input));compare();
    // Rail boundaries, exactly 2/3 valid readings, isolated/repeated outliers,
    // positive pressure near 0/256 and negative pressure. Encode ADC words.
    for(unsigned valid=0;valid<=7;++valid)for(int z2:{0,1,2500,3200,4095}) {
        memset(input,0,sizeof(input));
        for(unsigned i=0;i<7;++i) {
            setValue(i,1,(i<valid?uint16_t(1000+i):uint16_t(128))*8);
            setValue(i,3,0);setValue(i,5,(i<valid?uint16_t(3000-i):uint16_t(3969))*8);
            setValue(i,7,z2*8);
        }
        compare();
    }
    std::mt19937 random(2046);
    for(unsigned trial=0;trial<20000;++trial) {
        for(unsigned i=0;i<57;++i)input[i]=random()&255;
        if(trial%2==0)for(unsigned i=0;i<7;++i)for(unsigned offset:{1u,3u,5u,7u})
            setValue(i,offset,(random()%4096)*8);
        compare();
    }
    DiagnosticTouchXPT2046 instrumented;lgfx::touch_point_t point;
    auto before=reads;
    assert(!instrumented.getTouchRaw(nullptr,1));
    assert(!instrumented.getTouchRaw(&point,0));
    instrumented._inited=false;assert(!instrumented.getTouchRaw(&point,1));
    assert(reads==before);
    assert(instrumented.init() && instrumented._inited);
    assert(instrumented.recover() && resets==1);
    busQueryStatus=1;assert(!instrumented.recover() && !instrumented._inited && resets==2);
    assert(!instrumented.getTouchRaw(&point,1) && reads==before);
    busQueryStatus=0;busCapacity=1;assert(!instrumented.init());
    busCapacity=64;baseInitOk=false;assert(!instrumented.init());
    baseInitOk=true;assert(instrumented.init());
    printf("Instrumented/stock touch: identical commands, acceptance, coordinates and pressure over %u frames; no extra SPI reads; recovery init/failure guards passed.\n", records);
}
'''
source = out / 'stock.cpp'
source.write_text(fixture)
exe = out / 'stock'
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', f'-I{root / "include"}',
                str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
