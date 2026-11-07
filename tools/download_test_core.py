#!/usr/bin/env python3
"""Explicit private partition download from the temporary bench image only.

May contain credentials. Creates a new 0600 file, checks size/fingerprint,
never prints memory contents and never clears device evidence.
"""
import argparse
import hashlib
import os
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import Request, urlopen
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('radio')
    parser.add_argument('--id', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    url = urlsplit(args.radio)
    if url.scheme != 'http' or not url.netloc or url.username or url.password or url.query or url.fragment or url.path not in ('', '/'):
        parser.error('Use the radio HTTP base URL without credentials/path/query')
    if len(args.id) != 8 or any(c not in '0123456789abcdef' for c in args.id):
        parser.error('Use the eight lowercase hex ID from the crash summary')
    fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as output:
        request = Request(args.radio.rstrip('/') + '/api/radio/crash/test-partition?id=' + args.id,
                          headers={'X-Radiohead-Backup': '1'})
        with urlopen(request, timeout=20) as response:
            data = response.read(65537)
        if len(data) != 65536 or f'{zlib.crc32(data):08x}' != args.id:
            raise ValueError('Partition size/fingerprint differs; device evidence retained')
        output.write(data)
        output.flush()
        os.fsync(output.fileno())
    print(f'Saved private 0600 partition: {args.output}; SHA256={hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    main()
