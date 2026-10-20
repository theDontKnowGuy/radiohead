#!/usr/bin/env python3
"""Exercise restore preparation without a device or any user's settings."""
import hashlib
import json
import struct
import tempfile
from pathlib import Path
from prepare_restore import EXPECTED, FLASH_SIZE, prepare, validate_backup
from setup import APP

# Synthetic flash: deterministic nonzero bytes also check sector-end preservation.
data=bytearray(bytes(range(256))*(FLASH_SIZE//256))
data[0]=0xe9
struct.pack_into('<H',data,12,9)
table=b''.join(struct.pack('<HBBII16sI',0x50aa,t,st,o,s,n.encode().ljust(16,b'\0'),0) for n,t,st,o,s in EXPECTED)
table+=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(table).digest()
data[0x8000:0x9000]=table.ljust(4096,b'\xff')
assert validate_backup(data)==EXPECTED
for failure in ('short','bad-md5','wrong-layout','wrong-chip','encrypted'):
    bad=bytearray(data)
    if failure=='short': bad=bad[:-1]
    if failure=='bad-md5': bad[0x8004]^=1
    if failure=='wrong-chip': bad[12]=5
    if failure=='encrypted': struct.pack_into('<I',bad,0x8000+28,1)
    if failure=='wrong-layout':
        struct.pack_into('<I',bad,0x8000+4,0xa000)
        bad[0x8000+len(EXPECTED)*32+16:0x8000+len(EXPECTED)*32+32]=hashlib.md5(bad[0x8000:0x8000+len(EXPECTED)*32]).digest()
    try: validate_backup(bad)
    except ValueError: pass
    else: raise AssertionError('Accepted '+failure)
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp)
    backup=root/'synthetic.bin';backup.write_bytes(data)
    for channel in ('right','left'):
        image=APP/'artifacts'/channel/'zephyr.bin'
        plan=prepare(backup,image,image.parent/'manifest.json',root/channel)
        limit=plan['restore_bytes']
        restored=(root/channel/'restore-prefix.bin').read_bytes()+bytes(data[limit:])
        assert restored==data
        modified=bytearray(data);modified[:image.stat().st_size]=image.read_bytes()
        # Simulate sector erase tail: restore must include it too.
        modified[image.stat().st_size:limit]=b'\xff'*(limit-image.stat().st_size)
        modified[:limit]=(root/channel/'restore-prefix.bin').read_bytes()
        assert modified==data, 'restore did not recover exact original flash'
        wrong_manifest=root/'wrong.json'
        wrong_manifest.write_text(json.dumps({'files':{'zephyr.bin':'bad'}}))
        try: prepare(backup,image,wrong_manifest,root/'bad')
        except ValueError: pass
        else: raise AssertionError('Accepted mismatched image')
print('Restore checks passed: MD5/layout/chip/size/flags/hash rejection; exact full-state restoration including sector tail, both images. No device accessed.')
