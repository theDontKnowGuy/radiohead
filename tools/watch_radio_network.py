#!/usr/bin/env python3
"""Bounded read-only network comparison capture; never opens USB or changes Wi-Fi."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from urllib.parse import urlsplit
from urllib.request import urlopen

from collect_radio_diagnostics import collect


def capture(base):
    report = {'collectedUtc': datetime.now(timezone.utc).isoformat()}
    try:
        report['live'] = collect(base, 'live')
    except Exception as error:
        # Loss of HTTP reachability is evidence, not proof of an AP failure.
        report['error'] = str(error)
        return report
    for key, path in (
        ('roaming', '/api/radio/diagnostics/roaming'),
        ('player', '/api/player'),
        ('resources', '/api/spotify/resources'),
    ):
        try:
            with urlopen(base.rstrip('/') + path, timeout=5) as response:
                data = response.read(32001)
            if len(data) > 32000:
                raise ValueError('Unexpectedly large response')
            report[key] = json.loads(data)
        except Exception as error:
            report[key] = {'error': str(error)}
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('radio')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--duration', type=int, default=2400, help='Seconds; default 40 minutes')
    parser.add_argument('--interval', type=int, default=60, help='Seconds between captures')
    args = parser.parse_args()
    address = urlsplit(args.radio)
    if address.scheme != 'http' or not address.netloc or address.username or address.password or address.query or address.fragment or address.path not in ('', '/'):
        parser.error('Use a radio HTTP base URL without a path, credentials or query')
    if not 1 <= args.duration <= 7200 or not 30 <= args.interval <= 300:
        parser.error('Duration must be 1–7200 seconds; interval must be 30–300 seconds')
    # Exclusive creation prevents replacing preserved incident evidence.
    with args.output.open('x') as output:
        end = time.monotonic() + args.duration
        try:
            while True:
                started = time.monotonic()
                report = capture(args.radio)
                output.write(json.dumps(report, separators=(',', ':')) + '\n')
                output.flush()
                live = report.get('live', {})
                print(report['collectedUtc'], report.get('error',
                      f"boot={live.get('boot')} network={live.get('network', {}).get('flags') if live.get('network') else 'unavailable'}"), flush=True)
                remaining = end - time.monotonic()
                if remaining <= 0:
                    break
                time.sleep(min(remaining, max(0, args.interval - (time.monotonic() - started))))
                if time.monotonic() >= end:
                    break
        except KeyboardInterrupt:
            print('Stopped; completed captures are saved.', flush=True)


if __name__ == '__main__':
    main()
