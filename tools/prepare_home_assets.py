"""Prepare native Home assets. Run with Pillow; firmware builds only embed outputs.

Original icon geometry is drawn at 4x and area filtered to grayscale/RGBA edges.
The supplied background is preserved; this recipe creates a separate toned copy.
"""
from pathlib import Path
import hashlib
import json
import math
from PIL import Image, ImageDraw, ImageEnhance, __version__

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/ui/assets/home'
S = 4
TILE_WIDTH = 74
TILE_HEIGHT = 74
ICON_SIZE = 38
OUT.mkdir(parents=True, exist_ok=True)


def save(name, image):
    image.save(OUT / f'{name}.png', optimize=True)


def drawing(w, h):
    image = Image.new('RGBA', (w*S, h*S))
    return image, ImageDraw.Draw(image)


def box(values):
    return tuple(round(v*S) for v in values)


def line(d, points, fill, width):
    pts = [(round(x*S), round(y*S)) for x,y in points]
    d.line(pts, fill=fill, width=round(width*S), joint='curve')
    r=width/2
    for x,y in (points[0], points[-1]):
        d.ellipse(box((x-r,y-r,x+r,y+r)), fill=fill)


def finish(name, image):
    save(name, image.resize((image.width//S, image.height//S), Image.Resampling.LANCZOS))


source = ROOT / 'docs/bg1.png'
bg = Image.open(source).convert('RGB').resize((320,240),Image.Resampling.LANCZOS)
bg = ImageEnhance.Color(bg).enhance(0.92)
# Keep the supplied sunset warm and recognizable. A light veil still gives
# dynamic white type a stable reading surface without turning the coast navy.
for y in range(240):
    for x in range(320):
        rgb = bg.getpixel((x, y))
        bg.putpixel((x, y), tuple(round(c * 0.84) for c in rgb))
save('background', bg)

for name,top,bottom in [('radio',(25,129,236),(7,85,190)),('shows',(33,167,101),(9,109,60)),
                        ('favorites',(151,76,211),(92,39,156)),('settings',(73,111,141),(43,71,98))]:
    image,d=drawing(TILE_WIDTH,TILE_HEIGHT)
    mask=Image.new('L',image.size)
    ImageDraw.Draw(mask).rounded_rectangle(box((0,0,TILE_WIDTH-.25,TILE_HEIGHT-.25)),radius=8*S,fill=255)
    for y in range(TILE_HEIGHT*S):
        t=y/(TILE_HEIGHT*S-1)
        # The category colors are a primary Home navigation cue. Keep them
        # dense enough to read over the brighter coast while retaining a small
        # amount of the photograph through the prepared surface.
        color=tuple(round((a+(b-a)*t)*0.93) for a,b in zip(top,bottom))+(248,)
        d.line((0,y,TILE_WIDTH*S,y),fill=color)
    image.putalpha(mask.point(lambda value: value * 248 // 255))
    d.rounded_rectangle(box((0.6,0.6,TILE_WIDTH-.9,TILE_HEIGHT-.9)),radius=7.5*S,outline=(220,235,245,110),width=S)
    finish('tile_'+name,image)

image,d=drawing(TILE_WIDTH + 4,TILE_HEIGHT + 4)
d.rounded_rectangle(box((0.8,0.8,TILE_WIDTH + 2.7,TILE_HEIGHT + 2.7)),radius=10*S,outline=(190,225,245,140),width=round(1.1*S))
finish('focus',image)

white=(255,255,255,255)
icon=(232,238,243,255)
for kind in ['radio','shows','favorites','settings']:
    image,d=drawing(36,36)
    color=icon
    if kind=='radio':
        d.rounded_rectangle(box((6,12,30,30)),radius=4*S,outline=color,width=round(2.5*S))
        line(d,[(10,10),(27,3)],color,2.5)
        for x in [13,24]: d.ellipse(box((x-2.5,19,x+2.5,24)),fill=color)
    elif kind=='shows':
        for y in [9,18,27]:
            d.ellipse(box((3,y-2,7,y+2)),fill=color)
            line(d,[(12,y),(31,y)],color,3)
    elif kind=='favorites':
        points=[]
        for i in range(241):
            t=2*math.pi*i/240
            x=16*math.sin(t)**3
            y=13*math.cos(t)-5*math.cos(2*t)-2*math.cos(3*t)-math.cos(4*t)
            points.append(((18+x*0.87)*S,(17-y*0.87)*S))
        d.polygon(points,fill=color)
    else:
        points=[]
        for tooth in range(8):
            for fraction,r in [(0,11),(.18,14),(.68,14),(.86,11)]:
                a=2*math.pi*(tooth+fraction)/8
                points.append(((18+math.cos(a)*r)*S,(18+math.sin(a)*r)*S))
        d.polygon(points,fill=color)
        d.ellipse(box((12,12,24,24)),fill=(0,0,0,0))
    save('icon_'+kind,image.resize((ICON_SIZE,ICON_SIZE),Image.Resampling.LANCZOS))

# Compact brand mark used only by the Home product header.  It shares the
# Home icon's soft off-white and high-resolution edge treatment.
image,d=drawing(20,20)
d.rounded_rectangle(box((2.0,8.0,18.0,18.0)),radius=2.7*S,outline=icon,width=round(1.8*S))
line(d,[(4.8,6.5),(15.4,2.0)],icon,1.8)
for x in [6.3,13.7]: d.ellipse(box((x-1.55,11.5,x+1.55,14.5)),fill=icon)
finish('brand_radio',image)

image,d=drawing(24,19)
for r in [11,7]: d.arc(box((12-r,15-r,12+r,15+r)),215,325,fill=white,width=round(2.2*S))
d.ellipse(box((10.4,13.4,13.6,16.6)),fill=white)
finish('wifi',image)

for kind in ['clear','partly','cloudy','rain','snow','storm','mist','unknown']:
    image,d=drawing(64,56)
    if kind in ['clear','partly']:
        cx,cy=(29,26) if kind=='clear' else (22,20)
        d.ellipse(box((cx-11,cy-11,cx+11,cy+11)),fill=(255,204,18,255))
        for i in range(8):
            a=i*math.pi/4
            line(d,[(cx+math.cos(a)*16,cy+math.sin(a)*16),(cx+math.cos(a)*21,cy+math.sin(a)*21)],(255,210,22,255),2.4)
    if kind!='clear':
        color=white if kind not in ['mist','unknown'] else (175,190,209,255)
        for rect in [(15,24,37,46),(26,15,50,44),(42,28,59,46)]: d.ellipse(box(rect),fill=color)
        d.rounded_rectangle(box((19,32,56,47)),radius=6*S,fill=color)
    if kind=='rain':
        for x in [25,37,49]: line(d,[(x,48),(x-2,53)],(91,178,255,255),2)
    if kind=='snow':
        for x in [24,37,50]: d.ellipse(box((x-1.4,50,x+1.4,53)),fill=white)
    if kind=='storm': d.polygon([tuple(v*S for v in p) for p in [(35,34),(28,46),(35,46),(31,55),(45,41),(37,41),(42,34)]],fill=(255,211,40,255))
    if kind=='mist':
        for y in [45,50,55]: line(d,[(12,y),(58,y)],(185,200,215,255),1.5)
    if kind=='unknown': line(d,[(30,35),(43,35)],(70,90,115,255),2)
    finish('weather_'+kind,image)

manifest={'pillow':__version__,'background_source':'docs/bg1.png',
          'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
          'recipe':'direct 320x240 Lanczos (no crop), saturation 0.92, uniform 16% black veil; 74x74 square category tiles with dense, slightly translucent gradients and visible borders; 38px icons and 20px Home brand radio mark from original geometry at 4x',
          'assets':{}}
for p in sorted(OUT.glob('*.png')):
    with Image.open(p) as im: size=list(im.size)
    manifest['assets'][p.stem]={'size':size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
