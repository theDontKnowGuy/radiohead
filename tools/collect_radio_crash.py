#!/usr/bin/env python3
"""Collect selected crash evidence over HTTP, resolve addresses with matching ELF.

No USB access, raw stacks, settings changes, or automatic clearing. Explicit
--clear requires a durable saved valid report and sends its exact crash ID.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, urlopen
import os
import re

from collect_radio_diagnostics import collect
from firmware_artifacts import digest

ROOT = Path(__file__).resolve().parents[1]


def request_json(base, path, data=None):
    request = Request(base.rstrip('/') + path,
                      data=urlencode(data).encode() if data is not None else None,
                      headers={'X-Radiohead-Config': '1'} if data is not None else {})
    with urlopen(request, timeout=10) as response:
        body = response.read(16001)
    if len(body) > 16000:
        raise ValueError('Unexpectedly large response')
    return json.loads(body)


def decode(report, symbols, addr2line=None):
    build = report.get('build', '')
    if len(build) != 64 or any(c not in '0123456789abcdef' for c in build):
        raise ValueError('Missing/invalid full crash build hash')
    elf = symbols / build / 'firmware.elf'
    if digest(elf) != build:
        raise ValueError('ELF does not match the crashed firmware')
    if addr2line is None:
        core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
        matches = sorted(core.glob('packages/toolchain-xtensa-esp-elf*/bin/xtensa-esp32s3-elf-addr2line'))
        if not matches:
            matches = sorted(core.glob('packages/toolchain-xtensa-esp-elf*/bin/xtensa-esp-elf-addr2line'))
        if not matches:
            raise ValueError('addr2line not found; supply --addr2line')
        addr2line = str(matches[0])
    addresses = [report['pc']] + report.get('backtrace', [])
    if len(addresses) > 17 or any(type(a) is not int or not 0 <= a <= 0xffffffff for a in addresses):
        raise ValueError('Invalid backtrace addresses')
    result = subprocess.run([addr2line, '-a', '-f', '-C', '-i', '-e', str(elf),
                             *(hex(a) for a in addresses)], check=True, capture_output=True, text=True, timeout=20)
    if not re.search(r':[1-9][0-9]*(?:\s|$)', result.stdout):
        raise ValueError('Matching ELF found, but no source frame could be resolved; retain dump')
    return result.stdout


def annotate_bank(bank, symbols):
    breadcrumbs = bank.get('breadcrumbs')
    if not breadcrumbs:
        return
    prefix = bank.get('build', '')
    if len(prefix) != 16 or any(c not in '0123456789abcdef' for c in prefix):
        return
    matches = [path for path in symbols.glob(prefix + '*') if path.is_dir()]
    routes = []
    if len(matches) == 1:
        metadata = json.loads((matches[0] / 'symbols.json').read_text())
        mapping = matches[0] / 'web_diagnostic_routes.h'
        if digest(mapping) != metadata['maps']['web_diagnostic_routes.h']:
            raise ValueError('Archived route map checksum mismatch')
        routes = re.findall(r'^\s*"([^"]+)",', mapping.read_text(), re.MULTILINE)
    for name, operation in breadcrumbs.items():
        if not isinstance(operation, dict):
            continue
        operation['durationMs'] = None if operation['active'] else (
            operation['finishedMs'] - operation['startedMs']) & 0xffffffff
        if name == 'web':
            route, method = divmod(operation['code'], 16)
            operation['route'] = routes[route - 1] if 1 <= route <= len(routes) else None
            operation['method'] = {1: 'GET', 2: 'HEAD', 3: 'POST', 4: 'PUT', 5: 'PATCH', 6: 'DELETE', 7: 'OPTIONS'}.get(method)
        else:
            operation['operation'] = {1: 'playlist', 2: 'connect', 3: 'await_playback', 4: 'retry_wait'}.get(operation['code'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('radio')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--symbols', type=Path, default=ROOT / '.pio/symbols')
    parser.add_argument('--addr2line')
    parser.add_argument('--clear', action='store_true', help='Explicitly clear this dump after saving and decoding it')
    args = parser.parse_args()
    url = urlsplit(args.radio)
    if url.scheme != 'http' or not url.netloc or url.username or url.password or url.query or url.fragment or url.path not in ('', '/'):
        parser.error('Use the radio HTTP base URL without credentials/path/query')
    # Refuse overwrites before performing any work.
    with args.output.open('x') as output:
        crash = request_json(args.radio, '/api/radio/crash')
        report = {'collectedUtc': datetime.now(timezone.utc).isoformat(), 'crash': crash}
        for bank in ('live', 'previous', 'crash'):
            try:
                report.setdefault('banks', {})[bank] = collect(args.radio, bank)
                annotate_bank(report['banks'][bank], args.symbols)
            except Exception as error:
                report.setdefault('errors', {})[bank] = str(error)
        if crash.get('state') == 'valid':
            try:
                report['decoded'] = decode(crash, args.symbols, args.addr2line)
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                report['decodeError'] = str(error)
        output.write(json.dumps(report, indent=2) + '\n')
        output.flush()
        os.fsync(output.fileno())
    print(f'Saved {args.output}; crash state={crash.get("state")}')
    if report.get('decoded'):
        print(report['decoded'])
    if args.clear:
        if crash.get('state') != 'valid' or not report.get('decoded') or report.get('errors'):
            raise SystemExit('Evidence not fully collected/decoded; dump retained')
        print(request_json(args.radio, '/api/radio/crash/clear', {'confirm': 'CLEAR', 'id': crash['id']}))


if __name__ == '__main__':
    main()3
