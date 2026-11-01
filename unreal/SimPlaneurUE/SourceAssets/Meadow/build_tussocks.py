"""Create metre-scale, Y-up glTF meshes with folded leaves and seed heads."""
import json
import math
import random
import struct
from pathlib import Path


def build(name, seed, crowns, flowering, short=False):
    rng = random.Random(seed)
    positions, normals, uvs, leaf_uvs, indices = [], [], [], [], []

    def vertex(x, y, z, normal, flex, pigment, across=.5):
        positions.append((x, z, -y))
        nx, ny, nz = normal
        normals.append((nx, nz, -ny))
        uvs.append((pigment, flex))
        leaf_uvs.append((across, flex))
        return len(positions) - 1

    def leaf(x, y, direction, height, reach, width, color):
        dx, dy = math.cos(direction), math.sin(direction)
        twist = rng.uniform(-1.1, 1.1)
        first = len(positions)
        for level, fraction in enumerate((0., .43, .78)):
            distance = reach * fraction**1.8
            cx, cy = x + dx*distance, y + dy*distance
            cz = height * (fraction + .19*math.sin(math.pi*fraction))
            a = direction + twist*fraction
            sx, sy = -math.sin(a), math.cos(a)
            half = width * (.47, .5, .24)[level]
            for side in (-1, 0, 1):
                fold = .4*half if side == 0 else 0.
                nx, ny = dx*.42+sx*side*.27, dy*.42+sy*side*.27
                vertex(cx+sx*half*side+dx*fold, cy+sy*half*side+dy*fold, cz,
                       (nx, ny, math.sqrt(1.-nx*nx-ny*ny)), fraction, color, (side+1)*.5)
            if level:
                left = first + (level-1)*3
                for i in (left, left+1):
                    indices.extend((i,i+1,i+3, i+3,i+1,i+4))
        tip = vertex(x+dx*reach, y+dy*reach, height, (dx*.48,dy*.48,.8773), 1., color)
        indices.extend((tip-3,tip-2,tip, tip-2,tip-1,tip))

    for _ in range(crowns):
        a = rng.uniform(0., math.tau)
        radius = .24 * math.sqrt(rng.random())
        x, y = math.cos(a)*radius, math.sin(a)*radius
        vigor = rng.uniform(.65, 1.1)
        for _ in range(rng.randint(5, 9)):
            leaf(x+rng.uniform(-.008,.008), y+rng.uniform(-.008,.008), rng.uniform(0.,math.tau),
                 vigor*rng.uniform(.14,.39)*(.52 if short else 1.), rng.uniform(.045,.22),
                 rng.uniform(.005,.013)*(.72 if short else 1.), rng.uniform(.05,.68))
        if flowering and rng.random() < .7:
            angle = rng.uniform(0., math.tau)
            height = rng.uniform(.42,.64)
            bend = rng.uniform(.025,.07)
            leaf(x,y,angle,height,bend,.007,.83)
            dx, dy = math.cos(angle), math.sin(angle)
            # Paired, pointed spikelets grow along the upper fifth of each stem.
            for j in range(6):
                t = .8 + j*.034
                bx, by, bz = x+dx*bend*t**1.8, y+dy*bend*t**1.8, height*(t+.19*math.sin(math.pi*t))
                sign = 1 if j % 2 else -1
                offset = .008*sign
                base = vertex(bx,by,bz,(0,0,1),t,.94)
                for k in range(3):
                    theta = math.tau*k/3
                    vertex(bx+dy*offset*.45+.004*math.cos(theta),by-dx*offset*.45+.004*math.sin(theta),
                           bz+.011,(.45*math.cos(theta),.45*math.sin(theta),.893),t,.90)
                tip = vertex(bx+dy*offset,by-dx*offset,bz+.031,(0,0,1),t,.99)
                for k in range(3):
                    m, n = base+1+k, base+1+(k+1)%3
                    indices.extend((base,m,n, m,tip,n))

    data = bytearray()
    views, accessors = [], []
    def attribute(rows, fmt, kind, code):
        start = len(data)
        for row in rows:
            data.extend(struct.pack("<"+fmt, *row))
        views.append(dict(buffer=0, byteOffset=start, byteLength=len(data)-start))
        accessors.append(dict(bufferView=len(views)-1, componentType=code, count=len(rows), type=kind))
    attribute(positions,"3f","VEC3",5126)
    accessors[0].update(min=list(map(min,zip(*positions))),max=list(map(max,zip(*positions))))
    attribute(normals,"3f","VEC3",5126)
    attribute(uvs,"2f","VEC2",5126)
    attribute(leaf_uvs,"2f","VEC2",5126)
    attribute([(i,) for i in indices],"H","SCALAR",5123)
    path = Path(__file__).parent
    (path / (name+".bin")).write_bytes(data)
    model = dict(asset={"version":"2.0"}, scene=0, scenes=[{"nodes":[0]}], nodes=[dict(mesh=0,name=name)],
                 meshes=[dict(name=name, primitives=[dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2,TEXCOORD_1=3),indices=4)])],
                 buffers=[dict(uri=name+".bin",byteLength=len(data))],bufferViews=views,accessors=accessors)
    (path / (name+".gltf")).write_text(json.dumps(model,separators=(",",":")),encoding="ascii")


if __name__ == "__main__":
    build("BentGrass", 4522, 11, False)
    build("FloweringFescue", 722, 7, True)
    build("CreepingBent", 810, 12, False, short=True)
