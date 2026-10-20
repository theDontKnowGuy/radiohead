#!/usr/bin/env python3
"""Build and export this application only; no flash command exists here."""
import argparse
import os
import shutil
import subprocess
from pathlib import Path
from setup import APP, ENV, ZEPHYR_COMMIT

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--left',action='store_true')
args=p.parse_args()
channel='left' if args.left else 'right'
zephyr=ENV/'workspace/zephyr'
assert subprocess.check_output(['git','-C',str(zephyr),'rev-parse','HEAD'],text=True).strip()==ZEPHYR_COMMIT
for path in (zephyr, ENV/'workspace/modules/hal/espressif', ENV/'workspace/modules/hal/xtensa'):
    assert not subprocess.check_output(['git','-C',str(path),'status','--porcelain'],text=True), f'Modified dependency: {path}'
env=os.environ.copy()
env.update(PATH=str(ENV/'venv/bin')+os.pathsep+env['PATH'], ZEPHYR_BASE=str(zephyr),
           ZEPHYR_TOOLCHAIN_VARIANT='zephyr', ZEPHYR_SDK_INSTALL_DIR=str(ENV/'zephyr-sdk-0.17.4'))
for key in ('ZEPHYR_MODULES','EXTRA_ZEPHYR_MODULES','ESP_IDF_PATH','ESPRESSIF_TOOLCHAIN_PATH'):
    env.pop(key,None)
subprocess.run([str(ENV/'venv/bin/python'),str(APP/'tools/generate_pcm.py'),'--check'],check=True)
build=APP/f'build-{channel}'
cmd=[str(ENV/'venv/bin/west'),'build','-p','always','-b','esp32s3_devkitc/esp32s3/procpu','-d',str(build),str(APP),'--',f'-DCONFIG_TONE_LEFT={"y" if args.left else "n"}']
# Write a complete build log while retaining useful progress in the terminal.
out=APP/'artifacts'/channel
out.mkdir(parents=True,exist_ok=True)
with (out/'build.log').open('w') as log:
    proc=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    for line in proc.stdout:
        log.write(line)
        if any(word in line for word in ('error:', 'warning:', 'Memory region', 'FLASH:', '_seg:', 'Zephyr version:', 'Error', 'FAILED')): print(line,end='')
    if proc.wait(): raise SystemExit('Build failed; see '+str(out/'build.log'))
for source,target in [('zephyr.bin','zephyr.bin'),('zephyr.elf','zephyr.elf'),('zephyr.map','zephyr.map'),('.config','zephyr.config'),('zephyr.dts','zephyr.dts')]:
    shutil.copy2(build/'zephyr'/source,out/target)
subprocess.run([str(ENV/'venv/bin/python'),str(APP/'tools/check_build.py'),'--channel',channel],check=True)
print('Built '+str(out/'zephyr.bin')+'. Not flashed.')
