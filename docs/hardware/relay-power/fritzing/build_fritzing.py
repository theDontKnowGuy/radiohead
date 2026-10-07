"""Native Fritzing wiring sketch, bundled custom terminal representations.

Module terminal positions are illustrative, not unverified physical footprints.
The source power netlist is unchanged; this is not a PCB manufacturing file.
"""
import csv
import math
import zipfile
from collections import defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET
from html import escape

HERE = Path(__file__).resolve().parent
rows = list(csv.DictReader((HERE.parent / 'connections.csv').open()))
parts = {}
for r in rows:
    parts.setdefault(r['Reference'], {'value': r['Part/value'], 'pins': []})['pins'].append((r['Terminal'], r['Net']))

# Upper row: pack/charger/relay/radio; lower rows: startup and relay interface.
positions = {
 'CELL1':(70,130),'CELL2':(70,330),'PROTECT':(480,130),'F1':(880,130),
 'IP5310':(1280,130),'RELAY':(1680,130),'ESP32':(2080,130),
 'MAX_LEFT':(2080,430),'MAX_RIGHT':(2080,630),'C4':(1680,540),
 'SW1':(70,800),'R1':(70,1000),'C3':(70,1200),
 'U1':(600,780),'C1':(600,1400),'C2':(600,1600),
 'Q1':(1100,780),'R2':(1100,1000),'R3':(1100,1200),'R4':(1100,1400),'R15':(1100,1600),
 'D1':(70,1900),'D2':(70,2100),'R5':(480,1900),'Q2':(880,1900),'R6':(880,2120),
 'R7':(1280,1900),'Q3':(1680,1900),'R8':(1680,2120),'R9':(2080,1900),
 'Q4':(1680,800),'R10':(1680,1020),'R11':(1680,1220),
 'Q5':(2080,1000),'R12':(2080,1220),'R13':(2080,1420),'R14':(2080,1620),
}
assert set(positions)==set(parts)
COLORS={'GND':'#222222','VBAT_P':'#b31b1b','5V_PRE':'#e46017','5V_RADIO':'#d62c54',
        'HOLD':'#7d3ca2','START':'#008974','KEY':'#946e18','PRESSED':'#0064b1',
        'CELL_PLUS':'#be3232','CELL_MINUS':'#222222','PACK_PLUS':'#be3232','3V3_RADIO':'#d58008'}
def color(net): return COLORS.get(net,'#488896')
def child(parent, tag, text=None, **attrs):
    e=ET.SubElement(parent, tag, {k:str(v) for k,v in attrs.items()})
    if text is not None:e.text=text
    return e
def xml(e): return ET.tostring(e,encoding='utf-8',xml_declaration=True)
assets={}
root=ET.Element('module',fritzingVersion='1.0.8',icon='.png')
views=child(root,'views')
for v in ['breadboardView','schematicView','pcbView']:
    child(views,'view',name=v,backgroundColor='#ffffff',gridSize='0.1in',showGrid='0',alignToGrid='0',viewFromBelow='0')
instances=child(root,'instances')
models={}; pin_positions={}; conn_elements={}; next_id=1000

def make_svg(ref, part, view):
    pins=part['pins']; n=len(pins); width=330; height=70+n*29
    discrete=ref[0] in 'RCDQF' and ref not in ['RELAY','CELL1','CELL2']
    fill='#f5f7f9' if view=='schematic' else ('#35343d' if ref=='U1' else '#775b2a' if discrete else '#205766')
    ink='#172a35' if view=='schematic' else '#ffffff'
    a=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{width/90}in" height="{height/90}in" viewBox="0 0 {width} {height}"><g id="{view}">',
       f'<rect x="22" y="3" width="304" height="{height-6}" rx="8" fill="{fill}" stroke="#29353b" stroke-width="2"/>',
       f'<text x="38" y="28" font-family="Droid Sans" font-size="17" font-weight="bold" fill="{ink}">{escape(ref)}</text>',
       f'<text x="38" y="50" font-family="Droid Sans" font-size="12" fill="{ink}">{escape(part["value"][:42])}</text>']
    # Small identification symbols supplement the explicit terminal table.
    if ref.startswith('R') and ref!='RELAY':
        a.append(f'<path d="M270 26h8v-6h28v12h-28v-6m28 0h9" fill="none" stroke="{ink}" stroke-width="2"/>')
    elif ref.startswith('C') and not ref.startswith('CELL'):
        a.append(f'<path d="M270 26h17m0-10v20m8-20v20m0-10h20" fill="none" stroke="{ink}" stroke-width="2"/>')
    elif ref.startswith('D'):
        a.append(f'<path d="M270 26h10l18-9v18l-18-9m21-11v22m0-11h14" fill="none" stroke="{ink}" stroke-width="2"/>')
    elif ref.startswith('Q'):
        a.append(f'<circle cx="292" cy="26" r="19" fill="none" stroke="{ink}" stroke-width="1.5"/><path d="M268 26h15m0-11v22m0-17l20-12m-20 23l20 13" fill="none" stroke="{ink}" stroke-width="2"/>')
    for j,(term,net) in enumerate(pins):
        y=76+j*29
        a.extend([f'<line x1="2" y1="{y}" x2="32" y2="{y}" stroke="{color(net)}" stroke-width="3"/>',
                  f'<circle id="connector{j}pin" cx="2" cy="{y}" r="3" fill="#d9b655" stroke="#555555"/>',
                  f'<rect id="connector{j}terminal" x="1.9" y="{y-.1}" width=".2" height=".2" fill="none"/>',
                  f'<text x="40" y="{y+4}" font-family="Droid Sans" font-size="12" fill="{ink}">{escape(term)}</text>',
                  f'<text x="185" y="{y+4}" font-family="Droid Sans" font-size="12" fill="{ink}">{escape(net)}</text>'])
    a.append('</g></svg>')
    return ''.join(a),height

for ref,part in parts.items():
    mid='radiohead_relay_revA_'+ref
    fname=mid+'.fzp'
    fzp=ET.Element('module',fritzingVersion='1.0.8',moduleId=mid)
    child(fzp,'version','1');child(fzp,'title',ref+' / '+part['value']);child(fzp,'label',ref)
    child(fzp,'date','2026-10-05');child(fzp,'author','Radiohead circuit plan')
    child(fzp,'description','Terminal wiring representation. Pad placement and scale are illustrative. No PCB footprint or simulation model. See relay-power/README.md for prototype checks.')
    props=child(fzp,'properties');child(props,'property','Radiohead relay power wiring',name='family');child(props,'property',part['value'],name='value')
    fv=child(fzp,'views')
    for view in ['breadboard','schematic','icon']:
        source='breadboard' if view=='icon' else view
        svg,height=make_svg(ref,part,source)
        if view=='icon':svg=svg.replace('id="breadboard"','id="icon"')
        image=f'{view}/{mid}.svg'
        layers=child(child(fv,view+'View'),'layers',image=image);child(layers,'layer',layerId=view)
        assets[f'svg.{view}.{mid}.svg']=svg.encode()
    fc=child(fzp,'connectors')
    for j,(term,net) in enumerate(part['pins']):
        c=child(fc,'connector',id=f'connector{j}',name=term,type='male');child(c,'description',term+' / '+net)
        cv=child(c,'views')
        for view in ['breadboard','schematic']:
            child(child(cv,view+'View'),'p',svgId=f'connector{j}pin',terminalId=f'connector{j}terminal',layer=view)
    assets['part.'+fname]=xml(fzp)
    index=next_id;next_id+=1;models[ref]=index
    i=child(instances,'instance',moduleIdRef=mid,modelIndex=index,path=fname);child(i,'title',ref)
    iv=child(i,'views')
    for view in ['breadboard','schematic']:
        x,y=positions[ref]
        if view=='schematic':x+=180
        vv=child(iv,view+'View',layer=view);child(vv,'geometry',x=x,y=y,z='2')
        vc=child(vv,'connectors')
        for j,(term,net) in enumerate(part['pins']):
            connector=child(vc,'connector',connectorId=f'connector{j}',layer=view)
            child(connector,'geometry',x=2,y=76+j*29)
            conn_elements[(view,index,j)]=child(connector,'connects')
            pin_positions[(view,index,j)]=(x+2,y+76+j*29)

def connect(view,src,dst):
    # Every wire has reciprocal endpoint references; no raster-only connections.
    global next_id
    index=next_id;next_id+=1
    sx,sy=pin_positions[(view,*src)];tx,ty=pin_positions[(view,*dst)]
    wire_layer='breadboardWire' if view=='breadboard' else 'schematicTrace'
    i=child(instances,'instance',moduleIdRef='WireModuleID',modelIndex=index,path='wire.fzp');child(i,'title','Wire'+str(index))
    v=child(child(i,'views'),view+'View',layer=wire_layer)
    child(v,'geometry',x=sx,y=sy,x1=0,y1=0,x2=tx-sx,y2=ty-sy,z='1',wireFlags=64 if view=='breadboard' else 128)
    net=endpoint_nets[src]
    child(v,'wireExtras',mils='12',color=color(net),opacity='1',banded='0')
    cc=child(v,'connectors')
    for k,target in enumerate([src,dst]):
        c=child(cc,'connector',connectorId=f'connector{k}',layer=wire_layer);child(c,'geometry',x=0,y=0)
        cs=child(c,'connects');child(cs,'connect',connectorId=f'connector{target[1]}',modelIndex=target[0],layer=view)
        child(conn_elements[(view,*target)],'connect',connectorId=f'connector{k}',modelIndex=index,layer=wire_layer)
    return index

nets=defaultdict(list); endpoint_nets={}
for ref,part in parts.items():
    for j,(term,net) in enumerate(part['pins']):
        endpoint_nets[(models[ref],j)]=net
        if net!='NC':nets[net].append((models[ref],j))

# Breadboard: minimum-length spanning tree per net. Crossings are not junctions.
edges=[]
for net,ends in nets.items():
    connected=[ends[0]];todo=ends[1:]
    while todo:
        a,b=min(((a,b) for a in connected for b in todo),key=lambda ab:math.dist(pin_positions[('breadboard',*ab[0])],pin_positions[('breadboard',*ab[1])]))
        connect('breadboard',a,b);edges.append((net,a,b));connected.append(b);todo.remove(b)

# Schematic: built-in named net labels provide electrically common named nets.
for ref,part in parts.items():
    for j,(term,net) in enumerate(part['pins']):
        if net=='NC':continue
        ep=(models[ref],j);x,y=pin_positions[('schematic',*ep)]
        label_id=next_id;next_id+=1
        i=child(instances,'instance',moduleIdRef='v5NetLabelModuleID',modelIndex=label_id,path=':/resources/parts/core/netlabel_v5.fzp')
        child(i,'property',name='label',value=net);child(i,'property',name='style',value='left');child(i,'title',net)
        v=child(child(i,'views'),'schematicView',layer='schematic')
        # Built-in v5 mono-font label terminal: (6 + 3.6 * characters) at 72 dpi,
        # converted to Fritzing's 90-dpi scene units. Verified in native SVG export.
        label_width=7.5+4.5*len(net)
        child(v,'geometry',x=x-20-label_width,y=y-4.5,z='2')
        c=child(child(v,'connectors'),'connector',connectorId='connector0',layer='schematic')
        child(c,'geometry',x=label_width,y=4.5)
        conn_elements[('schematic',label_id,0)]=child(c,'connects')
        pin_positions[('schematic',label_id,0)]=(x-20,y)
        connect('schematic',ep,(label_id,0))

assets['radiohead-relay-power.fz']=xml(root)
output=HERE/'radiohead-relay-power.fzz'
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in assets.items():z.writestr(name,data)

# Connectivity audit: all required endpoints share one tree, no NC connection.
for net,ends in nets.items():
    reached={ends[0]}
    while True:
        prior=len(reached)
        for n,a,b in edges:
            if n==net and (a in reached or b in reached):reached.update([a,b])
        if len(reached)==prior:break
    assert reached==set(ends),(net,reached,set(ends))
assert not any(endpoint_nets[a]=='NC' or endpoint_nets[b]=='NC' for _,a,b in edges)
print(f'{output}: {len(parts)} parts, {len(nets)} nets, {len(edges)} breadboard wires, bundled custom parts')
