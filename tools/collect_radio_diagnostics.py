#!/usr/bin/env python3
"""Download bounded flight-recorder pages without opening USB serial."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
from urllib.parse import urlencode, urlsplit
from urllib.request import urlopen

EVENTS = {
    1: 'slow_stream', 2: 'stream_lost', 4: 'eof', 8: 'stream_ready',
    16: 'recovery_scheduled', 32: 'connect_timeout', 64: 'player_stopped',
    128: 'state_changed', 256: 'wifi_changed', 512: 'pcm_stall',
    1024: 'pcm_resumed', 2048: 'loop_gap', 4096: 'allocation_failed',
    8192: 'i2s_write_timeout',
}
PHASES = ['boot', 'audio', 'web', 'network', 'weather', 'controls', 'media', 'display', 'idle', 'diagnostics']


def collect(base, bank):
    records, metadata, offset = {}, None, 0
    for _ in range(7):
        url = base.rstrip('/') + '/api/radio/diagnostics?' + urlencode({'bank': bank, 'offset': offset})
        with urlopen(url, timeout=10) as response:
            body = response.read(10001)
        if len(body) > 10000:
            raise ValueError('Unexpectedly large diagnostics response')
        page = json.loads(body)
        if page.get('schema') != 1 or page.get('bank') != bank or page.get('offset') != offset:
            raise ValueError('Unexpected diagnostics schema or page')
        if metadata is None:
            metadata = {key: value for key, value in page.items() if key not in ('records', 'offset', 'nextOffset')}
        elif any(page[key] != metadata[key] for key in ('boot', 'build', 'available')):
            raise ValueError('Radio restarted or archive changed during collection; run again')
        for record in page['records']:
            record['eventNames'] = [name for flag, name in EVENTS.items() if record['events'] & flag]
            phase = record['slowPhase']
            record['slowPhaseName'] = PHASES[phase] if phase < len(PHASES) else 'unknown'
            records[record['seq']] = record
        next_offset = page['nextOffset']
        if not page['available'] or next_offset == -1:
            break
        if next_offset != offset + 8 or next_offset >= 56:
            raise ValueError('Invalid next page')
        offset = next_offset
    return {**metadata, 'records': sorted(records.values(), key=lambda r: r['seq'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('radio', help='Radio base URL, e.g. http://radio.local')
    parser.add_argument('--output', type=Path, default=Path('radio-diagnostics.json'))
    args = parser.parse_args()
    address = urlsplit(args.radio)
    if address.scheme != 'http' or not address.netloc or address.username or address.password or address.query or address.fragment or address.path not in ('', '/'):
        parser.error('Use the radio HTTP base URL without a path, credentials or query')
    banks = {bank: collect(args.radio, bank) for bank in ('live', 'previous', 'crash')}
    report = {'collectedUtc': datetime.now(timezone.utc).isoformat(), 'banks': banks}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Saved {args.output}: ' + ', '.join(f'{bank}={len(data["records"])} records' for bank, data in banks.items()))


if __name__ == '__main__':
    main()
