"""Generate the reviewable relay-power prototype drawings and wiring list."""
from pathlib import Path
import csv
from reportlab.graphics.shapes import Drawing, Rect, Line, String, Circle, Polygon
from reportlab.graphics import renderPDF, renderSVG
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

# Embed metrically compatible fonts when the bundled document runtime is present.
font_dir = Path.home() / '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pdfjs-dist/standard_fonts'
for name, filename in [('Helvetica', 'LiberationSans-Regular.ttf'), ('Helvetica-Bold', 'LiberationSans-Bold.ttf')]:
    if (font_dir / filename).exists():
        pdfmetrics.registerFont(TTFont(name, str(font_dir / filename)))

ROOT = Path(__file__).resolve().parent
PDF = ROOT.parents[2] / 'output/pdf/radiohead-relay-power-plan.pdf'
PDF.parent.mkdir(parents=True, exist_ok=True)
W, H = 1190, 842
INK, BLUE, RED, GREEN = '#172b40', '#236da1', '#bc382d', '#287158'
pages = []

def text(d, x, y, s, size=15, color=INK, bold=False):
    d.add(String(x, H-y, s, fontName='Helvetica-Bold' if bold else 'Helvetica', fontSize=size, fillColor=HexColor(color)))
def line(d, x1, y1, x2, y2, color=INK, width=1.7):
    d.add(Line(x1,H-y1,x2,H-y2,strokeColor=HexColor(color),strokeWidth=width))
def lines(d,x,y,ss,size=15,gap=23,color=INK):
    for i,s in enumerate(ss): text(d,x,y+i*gap,s,size,color)
def box(d,x,y,w,h,title,ss=(),color=BLUE):
    d.add(Rect(x,H-y-h,w,h,rx=8,ry=8,fillColor=HexColor('#f3f7fa'),strokeColor=HexColor(color),strokeWidth=1.5))
    text(d,x+15,y+27,title,17,color,True)
    lines(d,x+15,y+53,ss,14,21)
def net(d,x,y,s,color=BLUE):
    text(d,x,y,s,13,color,True)
def page(title, subtitle):
    d=Drawing(W,H)
    d.add(Rect(0,0,W,H,fillColor=HexColor('#ffffff'),strokeColor=None))
    text(d,35,43,title,27,INK,True)
    text(d,35,71,subtitle,14)
    line(d,35,89,W-35,89,BLUE,2)
    text(d,35,810,'REV A - DESK-REVIEWED PROTOTYPE / NOT HARDWARE-VALIDATED / 2026-10-05',12,RED,True)
    text(d,1050,810,f'Sheet {len(pages)+1}',12)
    pages.append(d)
    return d
def resistor(d,x,y,label,value):
    line(d,x-20,y,x,y)
    d.add(Rect(x,H-y-7,58,14,fillColor=HexColor('#ffffff'),strokeColor=HexColor(INK)))
    line(d,x+58,y,x+78,y)
    text(d,x,y-15,f'{label} {value}',12)
def transistor(d,x,y,label,pnp=False):
    d.add(Circle(x,H-y,26,fillColor=None,strokeColor=HexColor(INK),strokeWidth=1.5))
    line(d,x-12,y-15,x-12,y+15)
    line(d,x-45,y,x-12,y)
    line(d,x-12,y-9,x+14,y-24)
    line(d,x-12,y+9,x+14,y+24)
    line(d,x+14,y-24,x+14,y-43)
    line(d,x+14,y+24,x+14,y+43)
    ax,ay=(x+3,y+18) if not pnp else (x-3,y-14)
    pts=[ax,H-ay,ax-8,H-ay+1,ax-2,H-ay+8] if not pnp else [ax,H-ay,ax+10,H-ay+3,ax+5,H-ay+9]
    d.add(Polygon(pts,fillColor=HexColor(INK),strokeColor=HexColor(INK)))
    text(d,x+40,y-8,label,14,INK,True)
    text(d,x+40,y+12,'2N3906 PNP' if pnp else '2N3904 NPN',12)
    text(d,x-37,y-8,'B',11)
    text(d,x+19,y-30,'E' if pnp else 'C',11)
    text(d,x+19,y+39,'C' if pnp else 'E',11)
def note(d,x,y,ss): lines(d,x,y,ss,14,22)

d=page('01 / Power wiring', 'Existing relay + IP5310 charger/boost board + a small start/hold interface')
box(d,35,135,230,125,'Matched 18650 pair',['1S2P: positives together','Negatives together','Cell model/rating: verify'])
box(d,335,135,245,125,'1S protection + fuse',['Rated >= 6 A continuous','Raw cells -> B+ / B-','5 A fuse: provisional'])
box(d,665,135,250,145,'IP5310 module',['Pack P+ -> module B+','Pack P- -> module B-','USB-C: charging input','USB-A: fixed 5 V output'])
line(d,265,197,335,197,RED,3); line(d,580,197,665,197,RED,3)
box(d,35,345,320,150,'Songle relay module',['Header + : 5V_PRE','Header - : GND','Header S : RELAY_S (sheet 3)','Identify COM / NO by continuity'])
box(d,455,345,300,150,'Relay power contacts',['COM : 5V_PRE','NO : 5V_RADIO','NC : leave disconnected','Use screw terminals / rated wire'])
line(d,790,280,790,310,RED,3); line(d,195,310,790,310,RED,3); line(d,195,310,195,345,RED,3)
line(d,605,310,605,345,RED,3); net(d,815,313,'5V_PRE = upstream +5 V',RED)
box(d,845,345,310,175,'Switched radio supply',['ESP32-S3: 5V / VIN','Both MAX98357: VIN','TFT: correct existing supply','TFT 3.3 V only if specified','470 uF / 16 V to GND: C4'])
line(d,755,405,845,405,RED,3); net(d,767,390,'5V_RADIO',RED)
box(d,35,570,540,135,'Common GND',['Protected pack P- / IP5310 output ground / relay -','ESP32 GND / amplifier GNDs / TFT GND','Never bridge raw cell B- around the pack protector.'])
box(d,625,570,530,135,'Controller supply',['VBAT_P = protected pack P+, before IP5310','U1 runs from VBAT_P, NOT switched 3.3 V.','5V_PRE powers the relay module, NOT 5V_RADIO.'])
note(d,35,751,['No TP4056 in parallel with this charger. No programming USB cable during battery operation.',
                 'Keep speaker terminals on amplifier +/- outputs; neither speaker terminal goes to GND.'])

d=page('02 / Encoder and start logic', 'U1 = SN74HC132N, PDIP-14; named nets connect between sheets')
box(d,35,130,315,170,'Encoder push switch',['SW1: BUTTON_N to GND','R1: VBAT_P -> BUTTON_N, 100k','C3: BUTTON_N -> GND, 10 nF','Move switch off direct GPIO7 wiring.','Encoder rotation A/B remain separate.'])
box(d,425,130,315,170,'U1A / invert button',['Pin 1 = BUTTON_N','Pin 2 = BUTTON_N','Pin 3 = PRESSED','PRESSED = high only while pressed'])
line(d,350,204,425,204,BLUE,2)
box(d,815,130,340,170,'Q1 / detect firmware hold',['GPIO18 -> R3 10k -> Q1 base','R4: Q1 base -> GND, 100k','Q1 emitter = GND','Q1 collector = NOT_HOLD','R2: NOT_HOLD -> VBAT_P, 100k'])
box(d,35,365,315,150,'U1B / allow start only when off',['Pin 4 = PRESSED','Pin 5 = NOT_HOLD','Pin 6 = START_N','START_N = NOT(PRESSED AND','                          NOT_HOLD)'])
box(d,425,365,315,150,'U1C / make START high',['Pin 9 = START_N','Pin 10 = START_N','Pin 8 = START','START = PRESSED AND NOT_HOLD'])
line(d,350,438,425,438,BLUE,2)
box(d,815,365,340,150,'Outputs to sheet 3',['START -> Q4 wakes IP5310','START -> D1 enables relay','PRESSED -> Q5 reads button','GPIO18 / HOLD -> D2 holds relay'])
line(d,740,438,815,438,BLUE,2)
box(d,35,585,530,135,'U1 supply and unused gate',['Pin 14 = VBAT_P; pin 7 = GND','C1 100 nF + C2 1 uF between pins 14 and 7','Unused pins 12 and 13 = GND; pin 11 = no connection.'])
box(d,615,585,540,135,'Firmware connection',['GPIO18 = proposed POWER_HOLD (confirm board pad)','R15: GPIO18 -> GND, 100k','GPIO7 = isolated encoder input from Q5; sheet 3'])
note(d,35,758,['GPIO18 is unused in the inspected firmware pin map; board exposure still needs confirmation.',
                 'No battery-voltage logic signal is wired directly into an ESP32 input.'])

d=page('03 / Relay driver and isolated interfaces', 'All resistors are 1/4 W. Diode band = cathode. All NPN emitters connect to GND.')
box(d,35,115,535,315,'Relay driver / active-HIGH module only')
note(d,55,174,['START -> D1 anode; GPIO18 -> D2 anode','D1 and D2 cathodes join at OR_NODE','D1/D2 = 1N4148; bands point toward OR_NODE'])
resistor(d,125,265,'R5','1k'); net(d,55,295,'OR_NODE')
transistor(d,265,265,'Q2')
line(d,203,265,220,265)
net(d,302,239,'C -> R7 1k -> Q3 base')
net(d,302,321,'E -> GND',GREEN)
note(d,55,369,['R6: Q2 base -> GND, 100k','Q3 PNP: emitter = 5V_PRE; collector = RELAY_S','R8: Q3 base -> 5V_PRE, 47k; R9: RELAY_S -> GND, 10k'])
box(d,620,115,535,315,'Q3 / supply 5 V to relay signal')
transistor(d,875,265,'Q3',True)
net(d,770,168,'5V_PRE',RED); line(d,889,222,889,183,RED,2)
resistor(d,695,265,'R7','1k'); line(d,773,265,830,265)
net(d,645,297,'Q2 collector')
line(d,889,308,889,345,RED,2); net(d,909,339,'RELAY_S -> module S',RED)
note(d,640,385,['Driver intended for S input current <= 30 mA.','First verify that HIGH turns your module ON.'])
box(d,35,470,535,245,'Q4 / wake IP5310 without GPIO backfeed')
resistor(d,130,570,'R10','22k'); net(d,55,550,'START')
transistor(d,265,570,'Q4'); line(d,208,570,220,570)
net(d,305,522,'C -> IP5310 KEY pad')
net(d,305,625,'E -> GND',GREEN)
note(d,55,670,['R11: Q4 base -> GND, 100k','KEY ground pad joins common GND. Keep onboard button.'])
box(d,620,470,535,245,'Q5 / encoder input stays at 3.3 V')
resistor(d,715,570,'R12','47k'); net(d,640,550,'PRESSED')
transistor(d,875,570,'Q5'); line(d,793,570,830,570)
net(d,915,522,'C -> GPIO7 / BUTTON_READ')
net(d,915,625,'E -> GND',GREEN)
note(d,640,670,['R13: Q5 base -> GND, 100k','R14: GPIO7 -> switched ESP32 3.3 V, 10k'])
note(d,35,760,['Q1/Q2/Q4/Q5 = 2N3904. Q3 = 2N3906. Use E/B/C from the actual purchased transistor datasheet.',
                 'The existing relay module already contains the coil driver and flyback diode.'])

d=page('04 / Pin map, bill of materials and state table', 'The accompanying connections.csv lists both endpoints of every added component.')
box(d,35,120,330,335,'U1 top view / notch upward')
for i,(a,b) in enumerate([('1  BUTTON_N','14 VBAT_P'),('2  BUTTON_N','13 GND'),('3  PRESSED','12 GND'),('4  PRESSED','11 unused'),('5  NOT_HOLD','10 START_N'),('6  START_N','9  START_N'),('7  GND','8  START')]):
    text(d,55,190+i*33,a,15);text(d,205,190+i*33,b,15)
box(d,405,120,355,335,'Additional parts',['1 x SN74HC132N + DIP-14 socket','4 x 2N3904 (Q1,Q2,Q4,Q5)','1 x 2N3906 (Q3)','2 x 1N4148 (D1,D2)','7 x 100k resistors','3 x 10k resistors','2 x 1k resistors','2 x 47k resistors','1 x 22k resistor','100 nF, 1 uF, 10 nF capacitors','470 uF / 16 V bulk capacitor','Perfboard / connectors / rated wire'])
box(d,800,120,355,335,'Existing / pack hardware',['Your Songle 5 V relay module','IP5310 USB-C + USB-A board','Two matched 18650 cells','1S pack protection >= 6 A','5 A pack fuse: provisional','5 V USB supply with load/charge margin','Do not carry radio power through','a solderless breadboard.','Use bench supply before real cells.'])
box(d,35,515,1120,185,'Logic state table')
rows=[('BUTTON PRESSED','HOLD','START / KEY pull-down','RELAY'),('No','Low / unpowered','Off','Off'),('Yes','Low','On','On while pressed'),('No','High','Off','On'),('Yes','High','Off','On; GPIO7 reads press')]
for i,row in enumerate(rows):
    for x,s in zip([55,360,530,910],row):text(d,x,570+i*26,s,14,INK,i==0)
note(d,35,755,['GND always means protected pack P-. VBAT_P means protected pack P+.','Confirm protection and fuse ratings against the actual cells, enclosure wiring and measured load.'])

d=page('05 / Commissioning and firmware handoff', 'Prototype acceptance steps - firmware has NOT been changed or flashed.')
box(d,35,125,545,300,'Before connecting the radio',['1. Relay only: use a current-limited 5 V bench supply.','2. Header + to 5 V, - to GND. S to GND: relay must OFF.','3. S to +5 V: relay must ON; measure S current <= 30 mA.','4. Identify COM/NO/NC by continuity; mark the terminals.','5. IP5310: identify KEY and ground button pads by continuity.','6. Confirm B- and output ground topology on the exact board.','7. Verify 5 V output and 4.2 V battery configuration.','8. Assemble controller; test with a dummy load first.','If the relay is active-LOW, this drawing must be revised.'])
box(d,625,125,530,300,'Firmware changes required before use',['Early setup: GPIO18 OUTPUT LOW.','While start button is held, allow >= 400 ms for KEY wake.','Then set GPIO18 HIGH before releasing start button.','Consume the power-on press; do not enter AP/calibration.','Normal GPIO7 input: pressed LOW; released HIGH.','Shutdown: wait for release, save state, stop audio.','Set GPIO18 LOW as the final action. No deep-sleep handoff.','During flash/OTA writing, defer power-off.','A reset/Hi-Z HOLD drops the relay in this simple design.'])
box(d,35,475,545,205,'User operation',['OFF -> hold encoder about 1 second, until takeover.','ON -> short clicks mute; the IP5310 KEY stays inactive.','OFF request -> release encoder before power is cut.','USB connection may wake IP5310, but must not close relay.','Press held during shutdown would otherwise keep START on.','Boot recovery needs a separate intentional gesture.'])
box(d,625,475,530,205,'Required bench evidence',['Test battery-only and USB-connected operation.','Measure relay start/hold at low battery, full battery and USB.','Verify KEY low interval >= 300 ms on battery startup.','Verify no backfeed at GPIO7/GPIO18 while ESP32 is off.','Test fast repeated clicks, held button and aborted startup.','Measure whole-pack OFF current after boost enters standby.','Test loud playback and charge/load transfer for resets.'])
note(d,35,735,['Sources: TI SN74HC132 datasheet; onsemi 2N3904/2N3906; Injoinic IP5310 V1.37; Songle SRD datasheet.',
                 'This is a schematic plan, not a Fritzing .fzz project, PCB layout, SPICE proof or hardware certification.',
                 'Supply/cell markings and module input behavior remain acceptance dependencies; see README.md.'])

connections=[]
def add(ref,value,pins):
    for pin,node in pins.items():connections.append((ref,value,pin,node))
res=[('R1','100k','VBAT_P','BUTTON_N'),('R2','100k','VBAT_P','NOT_HOLD'),('R3','10k','HOLD','Q1_B'),('R4','100k','Q1_B','GND'),('R5','1k','OR_NODE','Q2_B'),('R6','100k','Q2_B','GND'),('R7','1k','Q2_C','Q3_B'),('R8','47k','5V_PRE','Q3_B'),('R9','10k','RELAY_S','GND'),('R10','22k','START','Q4_B'),('R11','100k','Q4_B','GND'),('R12','47k','PRESSED','Q5_B'),('R13','100k','Q5_B','GND'),('R14','10k','3V3_RADIO','BUTTON_READ'),('R15','100k','HOLD','GND')]
for ref,val,a,b in res:add(ref,val,{'1':a,'2':b})
for ref,val,a,b in [('C1','100nF','VBAT_P','GND'),('C2','1uF','VBAT_P','GND'),('C3','10nF','BUTTON_N','GND'),('C4','470uF 16V','5V_RADIO','GND')]:add(ref,val,{'+ or 1':a,'- or 2':b})
add('D1','1N4148',{'A':'START','K band':'OR_NODE'})
add('D2','1N4148',{'A':'HOLD','K band':'OR_NODE'})
for ref,b,c,e in [('Q1','Q1_B','NOT_HOLD','GND'),('Q2','Q2_B','Q2_C','GND'),('Q3','Q3_B','RELAY_S','5V_PRE'),('Q4','Q4_B','KEY','GND'),('Q5','Q5_B','BUTTON_READ','GND')]:add(ref,'2N3906' if ref=='Q3' else '2N3904',{'B':b,'C':c,'E':e})
add('U1','SN74HC132N',dict(zip(map(str,range(1,15)),['BUTTON_N','BUTTON_N','PRESSED','PRESSED','NOT_HOLD','START_N','GND','START','START_N','START_N','NC','GND','GND','VBAT_P'])))
add('SW1','encoder momentary',{'1':'BUTTON_N','2':'GND'})
add('PROTECT','1S >=6A; exact board to verify',{'B+':'CELL_PLUS','B-':'CELL_MINUS','P+ or OUT+':'PACK_PLUS','P- or OUT-':'GND'})
add('F1','5A time-delay provisional',{'1':'PACK_PLUS','2':'VBAT_P'})
add('CELL1','matched 18650', {'+':'CELL_PLUS','-':'CELL_MINUS'})
add('CELL2','matched 18650', {'+':'CELL_PLUS','-':'CELL_MINUS'})
add('RELAY','existing Songle module',{'+':'5V_PRE','-':'GND','S':'RELAY_S','COM':'5V_PRE','NO':'5V_RADIO','NC':'NC'})
add('IP5310','USB-C + USB-A',{'B+':'VBAT_P','B-':'GND','USB-A +5V':'5V_PRE','USB-A GND':'GND','button KEY pad':'KEY','button ground pad':'GND'})
add('ESP32','existing dev board',{'GPIO18 proposed':'HOLD','GPIO7':'BUTTON_READ','5V/VIN':'5V_RADIO','3V3':'3V3_RADIO','GND':'GND'})
add('MAX_LEFT','MAX98357',{'VIN':'5V_RADIO','GND':'GND'})
add('MAX_RIGHT','MAX98357',{'VIN':'5V_RADIO','GND':'GND'})
with (ROOT/'connections.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['Reference','Part/value','Terminal','Net']);w.writerows(connections)

# Exact Boolean enumeration, not a transistor simulation or hardware proof.
for pressed in (False,True):
    for hold in (False,True):
        button_n=not pressed;not_hold=not hold
        u1a=not(button_n and button_n)
        u1b=not(u1a and not_hold)
        u1c=not(u1b and u1b)
        assert u1c==(pressed and not hold)
        assert (u1c or hold)==(pressed or hold)
        assert (not u1a)==button_n
assert len([r for r in res if r[1]=='100k'])==7

c=canvas.Canvas(str(PDF),pagesize=(W,H))
c.setTitle('Radiohead relay battery power - detailed prototype circuit plan')
for i,d in enumerate(pages,1):
    renderPDF.draw(d,c,0,0);c.showPage()
    renderSVG.drawToFile(d,str(ROOT/f'sheet-{i:02}.svg'))
c.save()
print(f'Created {PDF}; {len(pages)} editable SVG sheets; connections.csv')
