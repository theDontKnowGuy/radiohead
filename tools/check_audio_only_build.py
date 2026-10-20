#!/usr/bin/env python3
"""Verify exported diagnostics, isolated link graph and settings-preserving flash plans."""
import json
from pathlib import Path
import subprocess
import sys
from firmware_artifacts import validate_export
ROOT=Path(__file__).resolve().parents[1]
normal=ROOT/'.pio/build/esp32s3'
validate_export(normal)
variants=[('',False,'encoder',None,None),('-evidence',True,'encoder',None,None),
          ('-handsfree',False,'handsfree',None,None),('-case-20-trim-m12',False,'fixed',20,-12),
          ('-music-formats',False,'music-formats',None,None),
          ('-music-formats-radio18',False,'music-formats',None,None),
          ('-music-channels-radio18',False,'music-channels',None,None),
          ('-music-channels-radio18-evidence-uart',True,'music-channels',None,None)]
for suffix,evidence,control,fixed,trim in variants:
    output=ROOT/('.pio/build/audio-only'+suffix)
    build=ROOT/('.pio/idf-audio-only'+suffix)
    manifest=validate_export(output)
    expected=dict(suite='audio-only',evidence=evidence,case_count=55,control=control,fixed_case=fixed,fixed_trim_db=trim)
    if '-radio18' in suffix: expected['radio_index']=18
    if suffix.endswith('-evidence-uart'): expected['evidence_transport']='uart0-gpio43-44'
    assert manifest['diagnostic']==expected
    assert (output/'partitions.bin').read_bytes()==(normal/'partitions.bin').read_bytes()
    project=json.loads((build/'project_description.json').read_text())
    assert project['git_revision']=='5.5.5' and project['project_path']==str(ROOT/'experiments/audio-only')
    components=set(project['build_components'])
    assert not components.intersection({'arduino','esp_wifi','esp_phy','bt','nvs_flash','spiffs','fatfs','radiohead_spotify','radiohead_audio','radiohead_lovyangfx'})
    commands=json.loads((build/'compile_commands.json').read_text())
    app_sources=[x['file'] for x in commands if x['file'].startswith(str(ROOT)) and not x['file'].startswith(str(ROOT/'.pio'))]
    assert app_sources==[str(ROOT/'experiments/audio-only/main/main.cpp')],app_sources
    app_command=next(x['command'] for x in commands if x['file']==str(ROOT/'experiments/audio-only/main/main.cpp'))
    assert f'-DAUDIO_ONLY_HANDSFREE={int(control=="handsfree")}' in app_command
    assert f'-DAUDIO_ONLY_FIXED_CASE={fixed or 0}' in app_command
    if control in ('music-formats','music-channels'):
        macro='MUSIC_FORMATS' if control=='music-formats' else 'MUSIC_CHANNELS'
        assert f'-DAUDIO_ONLY_{macro}=1' in app_command
        if '-radio18' in suffix: assert '-DAUDIO_ONLY_RADIO_INDEX=18' in app_command
        source=json.loads((ROOT/'.pio/build/audio-only-handsfree/fixture.json').read_text())
        assert json.loads((output/'fixture.json').read_text())['pcm_sha256']==source['pcm_sha256']
        compiler=Path(project['c_compiler'])
        nm=compiler.with_name(compiler.name.removesuffix('gcc')+'nm')
        symbols=subprocess.check_output([str(nm),'-u',str(build/'esp-idf/main/CMakeFiles/__idf_main.dir/main.cpp.obj')],text=True)
        assert 'gpio_get_level' not in symbols and 'gpio_config' not in symbols
    if suffix.endswith('-evidence-uart'):
        assert '-DAUDIO_ONLY_UART_EVIDENCE=1' in app_command and '-DAUDIO_ONLY_EVIDENCE=1' in app_command
        assert 'esp_driver_uart' in components
        assert 'uart_tx_chars' in symbols and 'uart_wait_tx_done' in symbols
        assert 'usb_serial_jtag_write_bytes' not in symbols and 'uart_write_bytes' not in symbols
    config=(build/'sdkconfig').read_text()
    for flag in ('CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y','CONFIG_SPIRAM_MODE_OCT=y','CONFIG_ESP_CONSOLE_NONE=y'):
        assert flag in config,flag
    assert '# CONFIG_AUTOSTART_ARDUINO is not set' in config or 'CONFIG_AUTOSTART_ARDUINO=' not in config
    dry=subprocess.check_output([sys.executable,str(ROOT/'tools/flash_built_firmware.py'),
        '--artifacts-dir',str(output),'--port','unused','--dry-run'],text=True)
    assert 'write-flash' in dry and 'erase' not in dry and str(output/'firmware.bin') in dry
    assert all(f' {offset} ' not in dry for offset in ('0x9000','0x650000','0xc90000','0xff0000'))
    print('PASS:',output.relative_to(ROOT),'hashes, shared partition bytes, isolated graph, pinned IDF and flash plan')
for arguments in (['--case','0'],['--case','56'],['--case','4','--trim-db','1'],
                  ['--hands-free','--case','4'],['--trim-db','-12'],
                  ['--music-formats','--hands-free'],['--music-formats','--case','4'],
                  ['--music-formats','--trim-db','0'],['--radio-index','18'],
                  ['--music-formats','--radio-index','-1'],['--music-formats','--radio-index','22'],['--music-channels','--case','4'],
                  ['--music-channels','--music-formats'],['--music-channels','--hands-free'],
                  ['--music-channels','--radio-index','22'],['--music-channels','--trim-db','0'],
                  ['--evidence','--evidence-uart']):
    rejected=subprocess.run([sys.executable,str(ROOT/'tools/build_audio_only.py'),*arguments],capture_output=True,text=True)
    assert rejected.returncode==2,arguments
print('PASS: invalid case/trim values and conflicting control modes rejected before building')
restore=subprocess.check_output([sys.executable,str(ROOT/'tools/flash_built_firmware.py'),
    '--artifacts-dir',str(normal),'--port','unused','--dry-run'],text=True)
assert str(normal/'firmware.bin') in restore and 'erase' not in restore
print('PASS: original radio restore export and explicit restore plan')
