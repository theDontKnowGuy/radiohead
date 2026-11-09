#!/usr/bin/env python3
"""Verify the production weather scheduler bounds failed task allocations."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/display.cpp').read_text()
function = 'void updateWeatherData()' + source.split('void updateWeatherData()', 1)[1].split('\nvoid invalidateWeatherData()', 1)[0]
fixture = r'''#include <cassert>
#include <cstdint>
#include <string>
struct Text : std::string { using std::string::string; bool isEmpty() const { return empty(); } };
Text weatherCitySnapshot, weatherKeySnapshot, owmCity("test"), owmKey("test");
bool weatherFetchInProgress=false, weatherRefreshRequested=false, isAP=false;
unsigned weatherFetchGeneration=0, weatherConfigurationGeneration=0, attempts=0;
constexpr int WL_CONNECTED=3, NETWORK_WORKER_PRIORITY=1, NETWORK_WORKER_CORE=1, pdPASS=1;
struct { int status() { return WL_CONNECTED; } } WiFi;
uint32_t clockMs=100;
uint32_t millis() { return clockMs; }
void fetchWeatherTask(void*) {}
bool fail=true;
int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int) { ++attempts; return fail ? 0 : pdPASS; }
''' + function + r'''
int main(int argc, char**) {
 if (argc > 1) {
  clockMs=UINT32_MAX-100; updateWeatherData(); assert(attempts==1);
  clockMs=100; updateWeatherData(); assert(attempts==1);
  clockMs=29900; fail=false; updateWeatherData(); assert(attempts==2 && weatherFetchInProgress);
  return 0;
 }
 updateWeatherData(); assert(attempts==1 && !weatherFetchInProgress);
 for (unsigned i=0;i<10000;++i) { ++clockMs; updateWeatherData(); }
 assert(attempts==1);
 clockMs=30099; updateWeatherData(); assert(attempts==1);
 clockMs=30100; updateWeatherData(); assert(attempts==2);
 clockMs=60100; fail=false; updateWeatherData(); assert(attempts==3 && weatherFetchInProgress);
 weatherFetchInProgress=false; ++clockMs; updateWeatherData(); assert(attempts==3);
 weatherRefreshRequested=true; ++clockMs; updateWeatherData(); assert(attempts==4);
}
'''
with tempfile.TemporaryDirectory(prefix='radiohead-weather-') as directory:
 path = Path(directory)
 (path / 'test.cpp').write_text(fixture)
 subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
 subprocess.run([str(path / 'test')], check=True)
 subprocess.run([str(path / 'test'), 'wrap'], check=True)
print('Weather task allocation backoff, successful retry, explicit refresh and rollover passed')
