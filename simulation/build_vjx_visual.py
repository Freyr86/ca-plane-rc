"""Build the 2022 VJX display mesh. Pass a checkout of jxjo/VJX as the argument."""
import math
import re
import sys
from pathlib import Path

from painted_glider import PaintedGlider

import numpy as np
from PIL import Image, ImageDraw

source = Path(sys.argv[1])
dest = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/VJX"
flz = next((source / "wing/design/data").glob("*.flz")).read_text(encoding="latin1")
# The comparison file contains the VJX on the LEFT and an ellipse on the right.
widths, chords, offsets = [0.], [.230], [0.]
blocks = re.split(r"\[SEGMENT\d+\]", flz)
section_data = [blocks[0]]
for block in reversed([b for b in blocks[1:] if "SEGMENTBREITE=-" in b]):
    def field(key):
        return float(re.search("^" + key + "=(.*)$", block, re.M)[1])
    width = -field("SEGMENTBREITE")
    chord = field("PROFILTIEFE")
    # FLZ gives the sweep angle on the chord reference line.
    ref = field("BEZUGSPUNKT_PFEILWINKEL") / 100
    offsets.append(offsets[-1] - width * math.tan(math.radians(field("PFEILWINKEL"))) + ref * (chords[-1] - chord))
    widths.append(widths[-1] + width)
    chords.append(chord)
    section_data.append(block)
x = (1-np.cos(np.linspace(0,np.pi,49))) / 2
perimeter = np.r_[x[::-1],x[1:-1]]
profiles = []
for block in section_data:
    pts = np.array([[float(v) for v in pair] for pair in re.findall(r"^PK\d+=([\d.\-]+) ([\d.\-]+)", block, re.M)])
    if len(pts) < 5:
        profiles.append(None)
        continue
    cut = np.argmin(pts[:,0])
    upper, lower = pts[:cut+1][::-1], pts[cut:]
    profiles.append(np.r_[np.interp(x[::-1],*upper.T), np.interp(x[1:-1],*lower.T)])
known = [i for i,p in enumerate(profiles) if p is not None]
heights = np.array([np.interp(widths, np.array(widths)[known], [profiles[i][j] for i in known]) for j in range(len(perimeter))]).T

mesh = PaintedGlider()
def loft(rings, uv_rings):
    # Register the 90 mm drawing datum at the published 98 mm CG.
    mesh.loft(np.asarray(rings) + [.008, 0., 0.], uv_rings)

def constant_uv(u,n): return np.tile([u,.92],(n,1))
for side in [-1,1]:
    rings,uv=[],[]
    for y,ch,le,h in zip(widths,chords,offsets,heights):
        lateral=side*(y+.015)
        rings.append(np.column_stack((.09-le-perimeter*ch, .015+y*math.tan(math.radians(3))+h*ch, np.full_like(h,lateral))))
        uv.append(np.column_stack((np.full_like(h,y/1.45),.04+.69*perimeter)))
    loft(rings,uv)
    rings,uv=[],[]
    for frac,ch in [(0,.120),(.5,.109),(.8,.082),(.94,.051),(1,.01)]:
        span=.37*frac
        thick=.072*ch*np.sin(np.pi*np.sqrt(perimeter))*.5
        thick[len(x):]*=-1
        angle=math.radians(41.5)
        rings.append(np.column_stack((-.82-.7*(.120-ch)-perimeter*ch,
                .015+span*math.sin(angle)+thick*math.cos(angle),
                side*(span*math.cos(angle)-thick*math.sin(angle)))))
        uv.append(constant_uv(.2,len(thick)))
    loft(rings,uv)
# Fine elliptical cross sections approximate the pod and the carbon boom.
rings,uv=[],[]
theta=np.linspace(0,2*np.pi,48,endpoint=False)
for pos,ry,rz in [(.41,.001,.001),(.40,.009,.008),(.37,.014,.013),(.31,.018,.016),(.20,.020,.018),(.10,.022,.019),(-.06,.021,.018),(-.18,.017,.015),(-.40,.012,.012),(-.69,.009,.009),(-.82,.008,.011),(-.91,.004,.006),(-.945,.001,.001)]:
    rings.append(np.column_stack((np.full_like(theta,pos),-.005+ry*np.sin(theta),rz*np.cos(theta))))
    uv.append(constant_uv(.2 if pos >= .10 else .8,len(theta)))
loft(rings,uv)

paint=Image.new("RGB",(1024,512),"#eeeFEB")
draw=ImageDraw.Draw(paint)
draw.polygon([(829,0),(857,0),(915,390),(876,390)],fill="#181a1d")
draw.polygon([(918,0),(1024,0),(1024,390),(948,390)],fill="#efb716")
draw.rectangle((512,420,1023,511),fill="#202126")
mesh.save(dest/"VJX2022.glb", paint)
