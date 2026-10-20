#!/usr/bin/env python3
"""Offline: validate a full radio backup and prepare an exact sector-prefix restore.

Never opens a serial port. Backup contains credentials; never upload or commit it.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

FLASH_SIZE=0x1000000
EXPECTED=[('nvs',1,2,0x9000,0x5000),('otadata',1,0,0xe000,0x2000),
          ('app0',0,16,0x10000,0x640000),('app1',0,17,0x650000,0x640000),
          ('spiffs',1,130,0xc90000,0x360000),('coredump',1,3,0xff0000,0x10000)]

def digest(data): return hashlib.sha256(data).hexdigest()

def validate_backup(data):
    if len(data)!=FLASH_SIZE: raise ValueError('Backup must be a complete 16 MiB read_flash snapshot')
    if data[0]!=0xe9 or struct.unpack_from('<H',data,12)[0]!=9:
        raise ValueError('Expected an unencrypted ESP32-S3 boot image at 0x0; stop for encrypted/secure-boot devices')
    table=data[0x8000:0x9000]
    entries=[]
    for offset in range(0,4096,32):
        entry=table[offset:offset+32]
        if entry[:2]==b'\xeb\xeb':
            if entry[2:16]!=b'\xff'*14 or entry[16:]!=hashlib.md5(table[:offset]).digest():
                raise ValueError('Partition table MD5 mismatch')
            if entries!=EXPECTED: raise ValueError('Live partition layout differs from the reviewed radio layout')
            return entries
        if entry[:2]!=b'\xaa\x50': raise ValueError('Missing partition table entry or MD5')
        magic,typ,sub,addr,size,label,flags=struct.unpack('<HBBII16sI',entry)
        if flags: raise ValueError('Flagged/encrypted partitions are not supported by this procedure')
        entries.append((label.split(b'\0')[0].decode('ascii'),typ,sub,addr,size))
    raise ValueError('No partition MD5')

def prepare(backup,image,manifest,out):
    data=backup.read_bytes()
    partitions=validate_backup(data)
    firmware=image.read_bytes()
    build=json.loads(manifest.read_text())
    if digest(firmware)!=build['files']['zephyr.bin']: raise ValueError('Firmware hash differs from checked build')
    if firmware[0]!=0xe9 or struct.unpack_from('<H',firmware,12)[0]!=9: raise ValueError('Wrong target image')
    end=(len(firmware)+4095)&~4095
    if end!=int(build['affected_sector_end'],0) or end>=0x650000: raise ValueError('Unexpected flash span')
    out.mkdir(parents=True,exist_ok=True)
    restore=out/'restore-prefix.bin'
    if restore.exists(): raise ValueError('Restore prefix already exists; use a fresh output directory')
    restore.write_bytes(data[:end])
    # Secrets remain only in backup / restore binary files, never in JSON or output.
    plan={'backup_sha256':digest(data),'backup_size':len(data),'restore_prefix_sha256':digest(data[:end]),
          'restore_bytes':end,'firmware_sha256':digest(firmware),'firmware_channel':build['channel'],
          'flash_offset':'0x0','affected_sector_end':hex(end),
          'overwritten_regions':['bootloader','partition-table']+[p[0] for p in partitions if p[3]<end],
          'nvs_restored_byte_for_byte':data[0x9000:0xe000]==restore.read_bytes()[0x9000:0xe000],
          'device_readback_verified':False}
    (out/'restore-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    print('Offline plan ready. Backup and prefix contain private settings. Keep them local.')
    print('Affected sector window: 0x0..'+hex(end)+' (end exclusive).')
    print('Before installing: verify_flash the FULL backup against the SAME device; check security_info.')
    return plan

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--backup',type=Path,required=True)
    p.add_argument('--image',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    try: prepare(args.backup,args.image,args.image.parent/'manifest.json',args.out)
    except (ValueError,KeyError,OSError) as e: p.exit(1,str(e)+'\n')
