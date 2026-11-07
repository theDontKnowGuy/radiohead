"""Check immutable symbol archives and source decoding against a real build."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from archive_firmware_symbols import archive_symbols
from collect_radio_crash import annotate_bank, decode
from firmware_artifacts import validate_export

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / '.pio/build/esp32s3'
manifest = validate_export(source)
build = manifest['sha256']['firmware.elf']
config = ROOT / '.pio/symbols' / build / 'sdkconfig'
with tempfile.TemporaryDirectory(prefix='radio-crash-tools-') as temp:
    destination = Path(temp) / 'symbols'
    target = archive_symbols(source, config, destination, 'radio')
    stamp = (target / 'firmware.elf').stat().st_mtime_ns
    assert archive_symbols(source, config, destination, 'radio') == target
    assert (target / 'firmware.elf').stat().st_mtime_ns == stamp
    try:
        archive_symbols(source, config, destination, 'wrong-variant')
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted conflicting archive metadata')
    saved_config = (target / 'sdkconfig').read_bytes()
    (target / 'sdkconfig').write_bytes(saved_config + b'corrupt')
    try:
        archive_symbols(source, config, destination, 'radio')
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted corrupted archived sdkconfig')
    (target / 'sdkconfig').write_bytes(saved_config)
    core = Path.home() / '.platformio/packages'
    nm = next(core.glob('toolchain-xtensa-esp-elf*/bin/xtensa-esp32s3-elf-nm'))
    symbols = subprocess.check_output([str(nm), '-C', str(target / 'firmware.elf')], text=True)
    line = next(line for line in symbols.splitlines() if line.endswith(' T crashDiagnosticsBegin()'))
    address = int(line.split()[0], 16)
    decoded = decode({'build': build, 'pc': address, 'backtrace': [address]}, destination)
    assert 'crashDiagnosticsBegin()' in decoded and 'crash_diagnostics.cpp:' in decoded
    bank = {'build': build[:16], 'breadcrumbs': {'version': 1,
        'web': {'code': 97, 'startedMs': 0xfffffff0, 'finishedMs': 0x10, 'active': False},
        'media': {'code': 2, 'startedMs': 10, 'finishedMs': 0, 'active': True}}}
    annotate_bank(bank, destination)
    assert bank['breadcrumbs']['web']['durationMs'] == 32
    assert bank['breadcrumbs']['web']['route'] == '/api/radio/crash'
    assert bank['breadcrumbs']['media']['operation'] == 'connect'
    assert bank['breadcrumbs']['media']['durationMs'] is None
    with (target / 'firmware.elf').open('ab') as output:
        output.write(b'corrupt')
    try:
        decode({'build': build, 'pc': address}, destination)
    except ValueError:
        pass
    else:
        raise AssertionError('Decoded with mismatched ELF')
print('PASS: immutable archive, conflicting metadata rejection, matching source decode, route map, rollover and wrong-ELF rejection')
