#!/usr/bin/env python3
"""Verify cached Wi-Fi budget and pinned Arduino's actual init-policy branch."""
from pathlib import Path
import subprocess
import tempfile
from build_firmware import ROOT, mandatory_configuration, enforce_configuration, verify_configuration

mandatory = mandatory_configuration(ROOT / 'sdkconfig.defaults')
expected = {
    'CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM': '4',
    'CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM': '32',
    'CONFIG_ESP_WIFI_STATIC_TX_BUFFER': 'y',
    'CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM': '4',
    'CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM': '4',
    'CONFIG_ESP_WIFI_RX_BA_WIN': '4',
    'CONFIG_ESP_WIFI_TX_BA_WIN': '4',
}
assert all(mandatory[key] == value for key, value in expected.items())
with tempfile.TemporaryDirectory(prefix='radiohead-wifi-budget-') as directory:
    out = Path(directory)
    config = out / 'sdkconfig'
    enforce_configuration(config, mandatory)
    assert not config.exists()
    config.write_text('''CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM=16
CONFIG_ESP_WIFI_RX_BA_WIN=16
CONFIG_ESP_WIFI_TX_BA_WIN=6
# CONFIG_ESP_WIFI_STATIC_TX_BUFFER is not set
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER=y
CONFIG_UNRELATED_CHOICE=123
''')
    enforce_configuration(config, mandatory)
    verify_configuration(config, mandatory)
    first = config.read_text()
    enforce_configuration(config, mandatory)
    assert config.read_text() == first
    assert 'CONFIG_UNRELATED_CHOICE=123' in first
    # Invisible Kconfig options may be absent; enabled wrong options may not.
    config.write_text(first.replace('# CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER is not set\n', ''))
    verify_configuration(config, mandatory)
    config.write_text(first.replace('CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4', 'CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16'))
    try:
        verify_configuration(config, mandatory)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Incorrect effective buffer budget accepted')

    source = (ROOT / '.pio/spotify-components/arduino/libraries/WiFi/src/WiFiGeneric.cpp').read_text()
    start = source.index('    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();')
    end = source.index('    esp_err_t err = esp_wifi_init(&cfg);', start)
    # Compile the pinned adapter's actual configuration branch unchanged. No
    # radio or fake heap is needed to verify the SDK default override behavior.
    fixture = r'''
#include <cassert>
struct wifi_init_config_t {
int static_rx_buf_num, dynamic_rx_buf_num, static_tx_buf_num, dynamic_tx_buf_num;
int tx_buf_type, cache_tx_buf_num, rx_ba_win, tx_ba_win;
};
#define CONFIG_ESP_HOSTED_ENABLED 0
#define WIFI_INIT_CONFIG_DEFAULT() {4,32,4,32,0,4,4,4}
struct WiFiGenericClass { static bool selected; static bool useStaticBuffers() { return selected; } };
bool WiFiGenericClass::selected = false;
wifi_init_config_t configuration() {
'''+source[start:end]+r'''
return cfg;
}
int main() {
  auto dynamic = configuration();
  assert(dynamic.static_tx_buf_num == 0 && dynamic.dynamic_tx_buf_num == 32 && dynamic.tx_buf_type == 1);
  WiFiGenericClass::selected = true;
  auto fixed = configuration();
  assert(fixed.static_rx_buf_num == 4 && fixed.static_tx_buf_num == 4 && fixed.tx_buf_type == 0);
  assert(fixed.dynamic_rx_buf_num == 32 && fixed.cache_tx_buf_num == 4);
  assert(fixed.rx_ba_win == 4 && fixed.tx_ba_win == 4);
}
'''
    (out / 'fixture.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(out / 'fixture.cpp'), '-o', str(out / 'check')], check=True)
    subprocess.run([str(out / 'check')], check=True)
print('Wi-Fi cached budget and pinned Arduino runtime-policy checks passed')
