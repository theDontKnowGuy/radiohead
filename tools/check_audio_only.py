#!/usr/bin/env python3
"""Compile/run the real portable audio engine and controls with undefined-behavior checks."""
import argparse
import subprocess
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--write-run-sheet',action='store_true',help='Regenerate the blank CSV (overwrites recorded results)')
args=parser.parse_args()
assets=ROOT/'.pio/audio-only-assets'
if not (assets/'music.pcm').exists():
    subprocess.run(['python3',str(ROOT/'tools/prepare_audio_only.py')],check=True)
with tempfile.TemporaryDirectory(prefix='radiohead-audio-tests-') as temp:
    binary=Path(temp)/'check'
    subprocess.run(['c++','-std=c++17','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined',
        '-fno-sanitize-recover=all','-I'+str(ROOT/'experiments/audio-only/main'),'-I'+str(assets),
        str(ROOT/'tools/native_audio_only/check.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(assets/'music.pcm')],check=True)
    if args.write_run_sheet:
        sheet=subprocess.check_output([str(binary),str(assets/'music.pcm'),'--sheet'],text=True)
        (ROOT/'docs/hardware/audio-only-results.csv').write_text(sheet)
