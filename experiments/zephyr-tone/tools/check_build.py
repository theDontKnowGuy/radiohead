#!/usr/bin/env python3
"""Check compiled PCM, native driver provenance, pins, image header and layout."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from generate_pcm import tables
from setup import APP, ENV, REPO, ZEPHYR_COMMIT

FLASH_BYTES=0x1000000
PARTITIONS=[('nvs',1,2,0x9000,0x5000),('otadata',1,0,0xe000,0x2000),
            ('app0',0,16,0x10000,0x640000),('app1',0,17,0x650000,0x640000),
            ('spiffs',1,130,0xc90000,0x360000),('coredump',1,3,0xff0000,0x10000)]

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def check(channel):
    out=APP/'artifacts'/channel
    config=(out/'zephyr.config').read_text()
    def enabled(name): return f'CONFIG_{name}=y\n' in config
    for key in ('I2S','I2S_ESP32','DMA','DMA_ESP32','ESP_SIMPLE_BOOT'): assert enabled(key), key
    for key in ('WIFI','BT','NETWORKING','SERIAL','CONSOLE','PRINTK','LOG','FLASH','NVS','FILE_SYSTEM','SETTINGS','ESP_SPIRAM','BOOTLOADER_MCUBOOT','PM','WATCHDOG'): assert not enabled(key),key
    assert enabled('TONE_LEFT') == (channel=='left')
    assert 'CONFIG_FLASH_SIZE=16777216\n' in config
    assert 'CONFIG_FLASH_LOAD_OFFSET=0x0\n' in config
    map_text=(out/'zephyr.map').read_text()
    for name in ('i2s_esp32.c.obj','dma_esp32_gdma.c.obj','sequence.c.obj'): assert name in map_text,name
    for name in ('i2s_channel_write','i2s_new_channel','i2s_std.c.obj','audio-only','libESP32-audioI2S','media.cpp.obj'): assert name not in map_text,name
    with (out/'zephyr.elf').open('rb') as file:
        elf=ELFFile(file)
        symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
        def symbol_data(name):
            s=symbols[name]; sec=elf.get_section(s['st_shndx']); pos=s['st_value']-sec['sh_addr']
            return sec.data()[pos:pos+s['st_size']]
        for name,values in tables().items():
            assert symbol_data(name)==struct.pack('<'+'h'*len(values),*values),f'Compiled PCM mismatch: {name}'
        assert symbol_data('tone_pcm'), 'missing table descriptor'
        for name in ('tone_fill','i2s_esp32_tx_callback','dma_esp32_isr_handle_tx'): assert name in symbols,name
        ram_sections={s.name:s['sh_size'] for s in elf.iter_sections() if s['sh_flags']&2 and 0x3fc80000<=s['sh_addr']<0x40000000}
        # The ten 3528-byte buffers must be internal SRAM; no PSRAM dependency.
        slab=[s for s in symbols.values() if 'k_mem_slab_buf_tx_slab' in s.name]
        assert len(slab)==1 and slab[0]['st_size']==35280 and 0x3fc80000<=slab[0]['st_value']<0x3fd00000
    # Verify resolved device tree with Zephyr's own parser (locally generated pickle).
    sys.path.insert(0,str(ENV/'workspace/zephyr/scripts/dts/python-devicetree/src'))
    import pickle
    with (APP/f'build-{channel}/zephyr/edt.pickle').open('rb') as file: dt=pickle.load(file)
    node=dt.label2node['i2s0']
    assert node.status=='okay' and dt.label2node['i2s1'].status=='disabled'
    dma=node.props['dmas'].val
    assert len(dma)==1 and dma[0].name=='tx' and dma[0].data['channel']==3
    pin_node=node.props['pinctrl-0'].val[0]
    pins=list(pin_node.children.values())[0].props['pinmux'].val
    # Compare encoded pinmux values with the preprocessor-generated DTS (exact GPIOs).
    assert len(pins)==3
    assert [((v>>0)&0x3f) for v in pins]==[39,41,40], pins
    for name in ('uart0','usb_serial','spi2','spi3','i2c0','i2c1','touch','twai','wdt0','wifi','esp32_bt_hci','psram0'):
        assert dt.label2node[name].status=='disabled',name
    image=(out/'zephyr.bin').read_bytes()
    assert image[0]==0xe9 and struct.unpack_from('<H',image,12)[0]==9
    assert image[2]==2 and image[3]==0x4f, 'Expected DIO / 16MB / 80MHz'
    # Use upstream esptool to verify the visible RAM-image checksum as well.
    from esptool.bin_image import ESP32S3FirmwareImage
    with (out/'zephyr.bin').open('rb') as file: img=ESP32S3FirmwareImage(file)
    assert img.checksum==img.calculate_checksum(), 'image checksum invalid'
    limit=(len(image)+4095)&~4095
    assert limit < 0x650000
    overlaps=[{'name':n,'offset':hex(o),'size':hex(s)} for n,t,st,o,s in PARTITIONS if o<limit]
    # The public CSV is also checked, preventing drift in the offline layout assumption.
    csv=(REPO/'partitions.csv').read_text()
    for n,t,st,o,s in PARTITIONS:
        line=next(line for line in csv.splitlines() if line.split(',')[0].strip()==n)
        fields=[v.strip() for v in line.split(',')]
        assert int(fields[3],0)==o and int(fields[4],0)==s
    versions={}
    for name,path in [('zephyr',ENV/'workspace/zephyr'),('hal_espressif',ENV/'workspace/modules/hal/espressif'),('hal_xtensa',ENV/'workspace/modules/hal/xtensa')]:
        versions[name]=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
    assert versions=={'zephyr':ZEPHYR_COMMIT,'hal_espressif':'f3453bdeced28642424692aae32cce4eec3f2d7f','hal_xtensa':'b38620c7cc61e349e192ed86a54940a5cd0636b7'}
    result={'channel':channel,'versions':versions,'sdk':'0.17.4','compiler':'GNU 12.2.0','image_bytes':len(image),
            'flash_offset':'0x0','affected_sector_end':hex(limit),'production_overlap':overlaps,
            'layout_compatible':False,'backup_and_exact_restore_required':True,
            'ram_sections_bytes':ram_sections,'checks':['18 compiled PCM arrays exactly match generator','native I2S and GDMA linked; IDF channel driver absent','internal SRAM DMA slab','TX DMA channel 3','only GPIO39/41/40 in active I2S pinctrl; MCLK absent','other board peripheral nodes disabled','no network, storage, console or logging enabled','ESP32-S3 16MB/DIO/80MHz image and checksum','partition CSV offsets match'],
            'hardware_verified':False,'files':{n:sha(out/n) for n in ('zephyr.bin','zephyr.elf','zephyr.map','zephyr.config','zephyr.dts')},
            'source_sha256':{str(p.relative_to(APP)):sha(p) for p in sorted(APP.rglob('*')) if p.is_file() and ('src' in p.relative_to(APP).parts or p.name in ('prj.conf','app.overlay','CMakeLists.txt','Kconfig'))}}
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--channel',choices=('right','left'),default='right')
    check(p.parse_args().channel)
