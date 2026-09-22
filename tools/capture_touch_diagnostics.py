"""Capture only touch diagnostics, excluding other device logs and credentials."""
import argparse
import json
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--label', required=True,
                        choices=['idle', 'nail', 'light-pad', 'firm-pad'])
    parser.add_argument('--seconds', type=int, default=15)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 5 <= args.seconds <= 120:
        parser.error('--seconds must be between 5 and 120')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    adc_lines = 0
    poll_lines = 0
    with serial.Serial(args.port, 115200, timeout=0.2) as device:
        device.reset_input_buffer()
        started = time.monotonic()
        print(f'Capturing {args.label} for {args.seconds} seconds.', flush=True)
        with args.output.open('a', encoding='utf-8') as output:
            while time.monotonic() - started < args.seconds:
                line = device.readline(512).decode('ascii', errors='replace').strip()
                if not line.startswith(('TouchADC ', 'Touch polls=')):
                    continue
                adc_lines += line.startswith('TouchADC ')
                poll_lines += line.startswith('Touch polls=')
                output.write(json.dumps({
                    'label': args.label,
                    'elapsed_seconds': round(time.monotonic() - started, 3),
                    'report': line,
                }) + '\n')
                output.flush()
                print(line, flush=True)
    if not adc_lines or not poll_lines:
        raise SystemExit('Incomplete diagnostics: check TOUCH_DEBUG_ENABLED=1 '
                         'firmware, USB connection, and that calibration is finished.')
    print(f'Saved {adc_lines} ADC and {poll_lines} polling reports to {args.output}')


if __name__ == '__main__':
    main()
