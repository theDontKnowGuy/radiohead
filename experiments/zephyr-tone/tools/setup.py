#!/usr/bin/env python3
"""Install the pinned host environment locally; never access the device."""
import hashlib
import platform
import subprocess
import sys
from pathlib import Path

APP = Path(__file__).resolve().parents[1]
REPO = APP.parents[1]
ENV = REPO/'.pio/zephyr-tone-env'
ZEPHYR_COMMIT = '64e98eb5278d790f3b3e5279ca8b39b45097bbc0' # v4.2.1
SDK_VERSION = '0.17.4'
ASSETS = {
    'sdk.tar.xz': ('zephyr-sdk-0.17.4_macos-aarch64_minimal.tar.xz', 'cc3d26aee46b4264e49f5ef85f72aca8622c30d62b6233ef495837d4303d7c2b'),
    'toolchain.tar.xz': ('toolchain_macos-aarch64_xtensa-espressif_esp32s3_zephyr-elf.tar.xz', '708293da553e8caa105238e29e09a64839899d6b7e9262af1a866d3e9db02e49'),
}

def run(*args, **kwargs):
    subprocess.run(list(map(str,args)),check=True,**kwargs)

def main():
    if platform.system()!='Darwin' or platform.machine()!='arm64':
        sys.exit('This verified installer targets Apple Silicon macOS. See README for other hosts.')
    if sys.version_info[:2]!=(3,11):
        sys.exit('Use Python 3.11 for the pinned host lock.')
    ENV.mkdir(parents=True,exist_ok=True)
    py=ENV/'venv/bin/python'
    if not py.exists(): run(sys.executable,'-m','venv',ENV/'venv')
    run(py,'-m','pip','install','-r',APP/'tools/requirements-host.lock')
    zephyr=ENV/'workspace/zephyr'
    if not zephyr.exists(): run('git','clone','--depth','1','--branch','v4.2.1','https://github.com/zephyrproject-rtos/zephyr.git',zephyr)
    assert subprocess.check_output(['git','-C',str(zephyr),'rev-parse','HEAD'],text=True).strip()==ZEPHYR_COMMIT
    west=ENV/'venv/bin/west'
    if not (zephyr.parent/'.west').exists(): run(west,'init','-l',zephyr)
    run(west,'update','hal_espressif','hal_xtensa','--narrow','-o=--depth=1',cwd=zephyr.parent)
    sdk=ENV/f'zephyr-sdk-{SDK_VERSION}'
    for local,(name,sha) in ASSETS.items():
        path=ENV/local
        if not path.exists(): run('curl','-fL','--retry','3',f'https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{SDK_VERSION}/{name}','-o',path)
        assert hashlib.sha256(path.read_bytes()).hexdigest()==sha, f'Checksum mismatch: {path}'
        destination=ENV if local=='sdk.tar.xz' else sdk
        marker=sdk/'sdk_version' if local=='sdk.tar.xz' else sdk/'xtensa-espressif_esp32s3_zephyr-elf/bin/xtensa-espressif_esp32s3_zephyr-elf-gcc'
        if not marker.exists(): run('tar','-xJf',path,'-C',destination)
    print('Pinned environment ready. No radio, Wi-Fi or Bluetooth blobs needed; no device accessed.')

if __name__=='__main__': main()
