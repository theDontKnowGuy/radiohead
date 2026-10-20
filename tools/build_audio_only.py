#!/usr/bin/env python3
"""Build/export only the isolated audio diagnostic with the pinned IDF 5.5.5."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from firmware_artifacts import (APP_SLOT_SIZE, EXPORT_FILES, FLASH_FILES, FLASH_SETTINGS,
                                digest, validate_export, validate_layout)
from prepare_audio_only import prepare

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    evidence_mode=parser.add_mutually_exclusive_group()
    evidence_mode.add_argument('--evidence-uart', action='store_true', help='Stopped-only UART0 status for the USB-UART connector (TX43/RX44, 115200 baud)')
    evidence_mode.add_argument('--evidence', action='store_true', help='Opt-in nonblocking USB status, only while output is stopped')
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--music-channels', action='store_true', help='Music left-only / right-only / both, then stopped; default radio volume 18/21')
    mode.add_argument('--music-formats', action='store_true', help='Music A/B/A: 16-bit / 32-bit / 16-bit slots, then stopped; no encoder input')
    mode.add_argument('--hands-free', action='store_true', help='Run only the quiet first-listening sequence, then stop; no encoder input')
    mode.add_argument('--case', type=int, choices=range(1,56), metavar='1..55', help='Hold one fixed case without encoder input')
    parser.add_argument('--trim-db', type=int, choices=range(-60,1), metavar='-60..0', help='Extra attenuation for --case (default -12 dB)')
    parser.add_argument('--radio-index', type=int, choices=range(0,22), metavar='0..21', help='Corrected radio volume index for music comparison profiles; replaces extra trim')
    args = parser.parse_args()
    if args.trim_db is not None and args.case is None:
        parser.error('--trim-db requires --case')
    if args.music_channels and args.radio_index is None: args.radio_index=18
    if args.radio_index is not None and not (args.music_formats or args.music_channels):
        parser.error('--radio-index requires --music-formats or --music-channels')
    trim=args.trim_db if args.trim_db is not None else -12
    suffix='-music-channels' if args.music_channels else '-music-formats' if args.music_formats else '-handsfree' if args.hands_free else f'-case-{args.case}-trim-m{-trim}' if args.case else ''
    if args.radio_index is not None: suffix+=f'-radio{args.radio_index}'
    if args.evidence_uart: suffix+='-evidence-uart'
    elif args.evidence: suffix+='-evidence'
    build = ROOT / ('.pio/idf-audio-only'+suffix)
    output = ROOT / ('.pio/build/audio-only'+suffix)
    assets = ROOT / '.pio/audio-only-assets'
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio')).expanduser()
    idf = core/'packages/framework-espidf'
    arduino = core/'packages/framework-arduinoespressif32'
    for path, version in ((idf, '3.50505'), (arduino, '3.3.11')):
        if json.loads((path/'package.json').read_text())['version'] != version:
            raise RuntimeError(f'Expected pinned {version} in {path}; run pio pkg install -e esp32s3')
    prepare(assets)
    python_env = core/'penv/.espidf-5.5.5'
    python = python_env/'bin/python'
    env = os.environ.copy()
    env.update(IDF_PATH=str(idf), IDF_TOOLS_PATH=str(core), IDF_PYTHON_ENV_PATH=str(python_env),
        IDF_PYTHON_CHECK_CONSTRAINTS='no', RADIOHEAD_AUDIO_ASSETS=str(assets),
        RADIOHEAD_AUDIO_EVIDENCE=str(int(args.evidence or args.evidence_uart)),
        RADIOHEAD_AUDIO_UART_EVIDENCE=str(int(args.evidence_uart)),
        RADIOHEAD_AUDIO_HANDSFREE=str(int(args.hands_free)),
        RADIOHEAD_AUDIO_MUSIC_FORMATS=str(int(args.music_formats)),
        RADIOHEAD_AUDIO_MUSIC_CHANNELS=str(int(args.music_channels)),
        RADIOHEAD_AUDIO_RADIO_INDEX=str(args.radio_index if args.radio_index is not None else -1),
        RADIOHEAD_AUDIO_FIXED_CASE=str(args.case or 0), RADIOHEAD_AUDIO_TRIM=str(trim))
    paths = []
    for tool, suffix_path in (('tool-cmake', 'bin'), ('tool-ninja', ''), ('toolchain-xtensa-esp-elf', 'bin')):
        base = core/'tools'/tool
        if not base.exists(): base = core/'packages'/tool
        paths.append(str(base/suffix_path))
    env['PATH'] = os.pathsep.join(paths+[env.get('PATH','')])
    def run(command):
        subprocess.run([str(x) for x in command], cwd=ROOT/'experiments/audio-only', env=env, check=True)
    build.mkdir(parents=True, exist_ok=True)
    # Arduino is not in this graph. Retain common board settings but omit its
    # component-only autostart symbol rather than feeding an unknown Kconfig key.
    board_defaults=build/'sdkconfig.board.defaults'
    board_defaults.write_text('\n'.join(line for line in (ROOT/'sdkconfig.defaults').read_text().splitlines()
        if not line.startswith('CONFIG_AUTOSTART_ARDUINO='))+'\n')
    defaults = f'{board_defaults};{ROOT}/experiments/audio-only/sdkconfig.defaults'
    run([python, idf/'tools/idf.py', '-B', build, '-D', f'SDKCONFIG={build}/sdkconfig',
         '-D', f'SDKCONFIG_DEFAULTS={defaults}', 'build', 'size'])
    flash = json.loads((build/'flasher_args.json').read_text())
    validate_layout(build, flash['flash_files'], flash['flash_settings'])
    output.mkdir(parents=True, exist_ok=True)
    (output/'artifacts.json').unlink(missing_ok=True)
    for offset, name in FLASH_FILES.items():
        target = output/EXPORT_FILES[offset]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build/name, target)
    shutil.copyfile(build/'radiohead.elf', output/'firmware.elf')
    shutil.copyfile(build/FLASH_FILES['0x8000'], output/'partitions.bin')
    command = [python, '-m', 'esptool', '--chip', 'esp32s3', 'merge-bin', '--output',
               output/'firmware.factory.bin', '--flash-mode', 'dio', '--flash-freq', '80m', '--flash-size', '16MB']
    for offset, name in EXPORT_FILES.items(): command.extend((offset, output/name))
    run(command)
    names = set(EXPORT_FILES.values()) | {'firmware.factory.bin','firmware.elf','partitions.bin'}
    manifest = dict(diagnostic={'suite':'audio-only', 'evidence':args.evidence or args.evidence_uart, 'case_count':55,
                   'control':'music-channels' if args.music_channels else 'music-formats' if args.music_formats else 'handsfree' if args.hands_free else 'fixed' if args.case else 'encoder',
                   'fixed_case':args.case, 'fixed_trim_db':trim if args.case else None},
                   flash_files=EXPORT_FILES, flash_settings=FLASH_SETTINGS,
                   sha256={name:digest(output/name) for name in sorted(names)})
    if args.radio_index is not None: manifest['diagnostic']['radio_index']=args.radio_index
    if args.evidence_uart: manifest['diagnostic']['evidence_transport']='uart0-gpio43-44'
    (output/'artifacts.json').write_text(json.dumps(manifest, indent=2)+'\n')
    shutil.copyfile(assets/'fixture.json', output/'fixture.json')
    validate_export(output)
    size = (output/'firmware.bin').stat().st_size
    print(f'Audio-only image: {output}/firmware.bin ({size:,} bytes; {APP_SLOT_SIZE-size:,} bytes free)')

if __name__ == '__main__': main()
