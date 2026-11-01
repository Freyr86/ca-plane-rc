"""Build a bent-bough fir and a low-poly stand-in for wooded slopes, in metres."""
import json
import math
import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).parent


def branch_atlas():
    source = Image.open(ROOT / "FirAtlas.png").convert("RGBA")
    twig = source.crop((306, 351, 665, 710))
    branch = Image.new("RGBA", (768, 768))
    ImageDraw.Draw(branch).line([(388, 755), (377, 410), (380, 43)], fill=(63, 67, 35, 255), width=7)
    rng = random.Random(517)
    for i in reversed(range(27)):
        t = i/27
        y = round(703 - 648*t)
        for sign in (-1, 1):
            length = int((158+210*math.sin(math.pi*(.10+.75*t)))*(1-.68*t)*rng.uniform(.82,1.09))
            spray = twig.resize((int(length*.82),length), Image.Resampling.LANCZOS)
            pad = Image.new("RGBA", (length*2,length*2))
            pad.paste(spray,(length-spray.width//2,0))
            spray = pad.rotate(sign*rng.uniform(34,65),Image.Resampling.BICUBIC)
            branch.alpha_composite(spray,(380-length,y-length))
    atlas = Image.new("RGBA",(1024,1024))
    atlas.paste(branch.resize((1024,888),Image.Resampling.LANCZOS),(0,0))
    atlas.paste(source.crop((0,917,1024,1024)).resize((1024,136)),(0,888))
    atlas.save(ROOT/"FirBough.png")


def build(name, distant):
    rng = random.Random(822)
    pos, norms, uv, triangles = [], [], [], []
    def vertex(x,y,z,nx,ny,nz,u,v):
        pos.append((x,z,-y))
        norms.append((nx,nz,-ny))
        uv.append((u,v))
        return len(pos)-1
    def band(start,sides):
        for i in range(sides):
            a=start+i
            triangles.extend((a,a+1,a+sides+1, a+sides+1,a+1,a+sides+2))
    if not distant:
        for z,r in [(0,.25),(7,.15),(15,.035)]:
            for i in range(9):
                a=i*math.tau/8
                vertex(r*math.cos(a),r*math.sin(a),z,math.cos(a),math.sin(a),0,i/8,.88+.11*z/15)
        band(0,8)
        band(9,8)
        for tier in range(20):
            t=tier/20
            z=2+12.9*t
            for j in range(8):
                a=(j+rng.uniform(-.23,.23))*math.tau/8+tier*2.41
                dx,dy=math.cos(a),math.sin(a)
                length=(.5+3.5*(1-t)**.76)*rng.uniform(.78,1.20)
                depth=length*rng.uniform(.2,.41)
                base=len(pos)
                for k in (0,.48,1.):
                    cz=z-depth*math.sin(k*2.25)+k*k*.24
                    width=length*(.42-.13*k)
                    for side in (-1,0,1):
                        vertex(k*length*dx-side*width*dy,k*length*dy+side*width*dx,
                               cz+(.19*width if side==0 else 0),dx*.35,dy*.35,.93675,
                               .015+.97*(side+1)/2,(.014+.84*(1-k)))
                for k in (0,1,3,4):
                    a0=base+k
                    triangles.extend((a0,a0+1,a0+3,a0+3,a0+1,a0+4))
    elif name == "AlderCrown":
        for z,r in ((0,.22),(7,.12)):
            for i in range(7):
                a=i*math.tau/6
                vertex(r*math.cos(a),r*math.sin(a),z,math.cos(a),math.sin(a),0,0,.1)
        band(0,6)
        for limb in range(7):
            a=limb*2.40
            reach = 1.7+rng.random() if limb else 0.
            cx,cy=reach*math.cos(a),reach*math.sin(a)
            cz=10.4 if limb==0 else rng.uniform(7.2,9.4)
            width=rng.uniform(1.95,2.8)
            start=len(pos)
            for row in range(5):
                pitch=-1.46+row*.73
                for i in range(8):
                    ang=a+(i%7)*math.tau/7
                    dx=math.cos(ang)*math.cos(pitch)
                    dy=math.sin(ang)*math.cos(pitch)
                    dz=math.sin(pitch)
                    radius=width*(1+.13*math.sin(3*ang+row*.6))
                    vertex(cx+radius*dx,cy+radius*dy,cz+3.6*dz,dx,dy,dz,i/7,.35+.65*row/4)
            for k in range(4): band(start+k*8,7)
    else:
        # Overlapping unequal skirts preserve the silhouette at sub-pixel twig sizes.
        for tier in range(5):
            z=1.7+tier*2.35
            radius=3.35*(1-tier*.16)
            phase=rng.random()*math.tau
            start=len(pos)
            for level,(r,up) in enumerate(((.40,-.6),(1.,0),(.48,2.),(.05,4.4))):
                for i in range(9):
                    a=phase+(i%8)*math.tau/8
                    dx,dy=math.cos(a),math.sin(a)
                    span=r*radius*(.93+.15*math.sin(3*a+tier))
                    vertex(dx*span,dy*span,z+up+.28*math.sin(2*a),.74*dx,.74*dy,.6726,
                           (i%8)/8,level/3)
            for k in (0,9,18): band(start+k,8)
    data=bytearray()
    views,acc=[],[]
    for rows,fmt,kind,code in [(pos,"3f","VEC3",5126),(norms,"3f","VEC3",5126),
                               (uv,"2f","VEC2",5126),([(i,) for i in triangles],"H","SCALAR",5123)]:
        offset=len(data)
        for row in rows: data.extend(struct.pack("<"+fmt,*row))
        views.append(dict(buffer=0,byteOffset=offset,byteLength=len(data)-offset))
        acc.append(dict(bufferView=len(views)-1,componentType=code,count=len(rows),type=kind))
    acc[0].update(min=list(map(min,zip(*pos))),max=list(map(max,zip(*pos))))
    (ROOT/(name+".bin")).write_bytes(data)
    primitive=dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2),indices=3)
    model=dict(asset={"version":"2.0"},scene=0,scenes=[dict(nodes=[0])],nodes=[dict(name=name,mesh=0)],
               meshes=[dict(name=name,primitives=[primitive])],buffers=[dict(uri=name+".bin",byteLength=len(data))],
               bufferViews=views,accessors=acc)
    (ROOT/(name+".gltf")).write_text(json.dumps(model,separators=(",",":")),encoding="ascii")


if __name__=="__main__":
    branch_atlas()
    build("SilverFir",False)
    build("ForestCrown",True)
    build("AlderCrown",True)
