"""Reproducible generic RC geometry. Meshes + section curves, Rhino 7 archive."""
from pathlib import Path
import json
import math
import numpy as np
import trimesh
import rhino3dm as rh

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'models/generic_250'
WHITE=[235,241,242,255]
ORANGE=[242,104,36,255]
DARK=[30,42,53,255]
scene=trimesh.Scene()
parts=[]
curves=[]

def closed_loft(rings):
    n=len(rings[0]); vertices=np.array(rings).reshape((-1,3)).tolist(); faces=[]
    for i in range(len(rings)-1):
        for j in range(n):
            a=i*n+j; b=i*n+(j+1)%n; c=(i+1)*n+(j+1)%n; d=(i+1)*n+j
            faces.extend([[a,b,c],[a,c,d]])
    for index in [0,len(rings)-1]:
        center=len(vertices); vertices.append(np.mean(rings[index],axis=0).tolist())
        for j in range(n):
            faces.append([center,index*n+j,index*n+(j+1)%n])
    mesh=trimesh.Trimesh(vertices,faces,process=True)
    mesh.fix_normals()
    return mesh

def add(name,mesh,color,pivot=None,axis=None):
    if pivot is None: pivot=[0,0,0]
    mesh.visual=trimesh.visual.texture.TextureVisuals(material=trimesh.visual.material.PBRMaterial(baseColorFactor=color,metallicFactor=0.0,roughnessFactor=0.55))
    local=mesh.copy(); local.apply_translation(-np.array(pivot))
    transform=np.eye(4); transform[:3,3]=pivot
    scene.add_geometry(local,node_name=name,geom_name=name,transform=transform)
    parts.append(dict(name=name,mesh=mesh,color=color,pivot=pivot,axis=axis))

# Godot coordinates: +X right wing, +Y up, +Z aft. Unit: metre.
def profile(u,thickness=.12,camber=.02):
    yt=5*thickness*(.2969*math.sqrt(max(0,u))-.126*u-.3516*u*u+.2843*u**3-.1036*u**4)
    yc=camber/(.4*.4)*(2*.4*u-u*u) if u<.4 else camber/(.6*.6)*((1-.8)+.8*u-u*u)
    return yc,yt

def section(s,side=1,lo=0,hi=1,tail=False,vertical=False):
    if vertical:
        chord=.22-.09*s/.27; le=.65+.07*s/.27; base=.035
    elif tail:
        chord=.15-.05*s/.30; le=.70+.025*s/.30; base=.035
    else:
        chord=.25-.10*s/1.25; le=-.0625+.055*s/1.25; base=s*math.tan(math.radians(3))
    # Split off trailing controls; a narrow clearance makes hinges visible.
    us=np.linspace(lo,hi,35)
    ring=[]
    for top,values in [(1,us),(-1,us[::-1])]:
        for u in values:
            yc,yt=profile(float(u),.09 if tail or vertical else .12,0 if tail or vertical else .02)
            h=(yc+top*yt)*chord
            if vertical: ring.append([h,base+s,le+u*chord])
            else: ring.append([side*s,base+h,le+u*chord])
    # Remove duplicate leading/trailing vertices at exact endpoints.
    out=[]
    for p in ring:
        if not out or np.linalg.norm(np.array(p)-out[-1])>1e-9: out.append(p)
    if np.linalg.norm(np.array(out[0])-out[-1])<1e-9: out.pop()
    return out

for side,label in [(-1,'left'),(1,'right')]:
    for name,start,end,lo,hi,color in [
        ('wing_inner',0,.43,0,1,WHITE),
        ('wing_outer',.432,1.25,0,.715,WHITE),
        ('aileron',.432,1.248,.73,1,ORANGE)]:
        rings=[section(s,side,lo,hi) for s in np.linspace(start,end,9)]
        pivot=section(start,side,.73,.99)[0] if name=='aileron' else None
        endp=section(end,side,.73,.99)[0]
        axis=(np.array(endp)-pivot).tolist() if pivot is not None else None
        add(name+'_'+label,closed_loft(rings),color,pivot,axis)
    for name,lo,hi,color in [('stabilizer',0,.685,WHITE),('elevator',.705,1,ORANGE)]:
        rings=[section(s,side,lo,hi,tail=True) for s in np.linspace(.025,.30,6)]
        pivot=section(.025,side,.705,.99,tail=True)[0] if name=='elevator' else None
        axis=(np.array(section(.30,side,.705,.99,tail=True)[0])-pivot).tolist() if pivot is not None else None
        add(name+'_'+label,closed_loft(rings),color,pivot,axis)
    for s in [0,.43,1.25]: curves.append(('wing_section_'+label+'_'+str(s),section(s,side)))

for name,lo,hi,color in [('fin',0,.68,WHITE),('rudder',.70,1,ORANGE)]:
    rings=[section(s,lo=lo,hi=hi,vertical=True) for s in np.linspace(0,.27,7)]
    pivot=section(0,lo=.70,hi=.99,vertical=True)[0] if name=='rudder' else None
    axis=(np.array(section(.27,lo=.70,hi=.99,vertical=True)[0])-pivot).tolist() if pivot is not None else None
    add(name,closed_loft(rings),color,pivot,axis)

# Elliptic fuselage stations form a slender pod and tapering tail boom.
stations=[(-.60,.002,.002,-.012),(-.57,.018,.022,-.01),(-.50,.034,.041,-.007),(-.38,.046,.052,-.003),(-.20,.042,.047,-.004),(-.02,.033,.037,-.009),(.13,.024,.028,-.012),(.30,.017,.020,-.01),(.55,.011,.013,0),(.80,.006,.008,.013),(.91,.002,.003,.02)]
rings=[]
for z,rx,ry,y in stations:
    rings.append([[rx*math.cos(t),y+ry*math.sin(t),z] for t in np.linspace(0,2*math.pi,48,endpoint=False)])
add('fuselage',closed_loft(rings),WHITE)
canopy=trimesh.creation.uv_sphere(count=[20,32])
canopy.apply_scale([.036,.023,.15]); canopy.apply_translation([0,.038,-.32])
add('canopy',canopy,DARK)

DEST.mkdir(parents=True,exist_ok=True)
(ROOT/'game/assets').mkdir(exist_ok=True)
file=rh.File3dm(); file.Settings.ModelUnitSystem=rh.UnitSystem.Meters
file.Settings.ModelAbsoluteTolerance=.0001
# Store Rhino coordinates X right, Y forward, Z up (right-handed).
def to_rh(p): return rh.Point3d(float(p[0]),float(-p[2]),float(p[1]))
for part in parts:
    layer=rh.Layer(); layer.Name=part['name']; layer.Color=tuple(part['color'])
    li=file.Layers.Add(layer)
    attr=rh.ObjectAttributes(); attr.Name=part['name']; attr.LayerIndex=li
    attr.SetUserString('status','FICTIF - NON VALIDE')
    attr.SetUserString('units','metres')
    if part['axis']:
        attr.SetUserString('hinge_godot_m',json.dumps(part['pivot']))
    mesh=rh.Mesh()
    for p in part['mesh'].vertices:
        v=to_rh(p); mesh.Vertices.Add(v.X,v.Y,v.Z)
    for face in part['mesh'].faces: mesh.Faces.AddFace(*(int(x) for x in face))
    mesh.Normals.ComputeNormals(); mesh.Compact()
    assert mesh.IsValid,part['name']
    file.Objects.AddMesh(mesh,attr)
layer=rh.Layer(); layer.Name='construction_sections'; layer.Visible=False
li=file.Layers.Add(layer)
for name,points in curves:
    attr=rh.ObjectAttributes(); attr.Name=name; attr.LayerIndex=li
    curve=rh.PolylineCurve([to_rh(p) for p in points+[points[0]]])
    file.Objects.AddCurve(curve,attr)
assert file.Write(str(DEST/'Generic250_Rhino7.3dm'),7)
glb=scene.export(file_type='glb')
(DEST/'Generic250.glb').write_bytes(glb)
(ROOT/'game/assets/Generic250.glb').write_bytes(glb)
# Wavefront fallback, separate named objects (materials in Rhino/GLB).
with (DEST/'Generic250.obj').open('w') as f:
    f.write('# Generic250. Metres. X right, Y forward, Z up.\n')
    offset=1
    for part in parts:
        f.write('o '+part['name']+'\n')
        for p in part['mesh'].vertices:
            v=to_rh(p); f.write(f'v {v.X:.7f} {v.Y:.7f} {v.Z:.7f}\n')
        for face in part['mesh'].faces: f.write('f '+' '.join(str(int(i)+offset) for i in face)+'\n')
        offset+=len(part['mesh'].vertices)
metadata=dict(name='Generic 250',status='synthetic_unvalidated',span_m=2.5,length_m=1.51,
 wing_area_m2=.5,root_chord_m=.25,tip_chord_m=.15,dihedral_deg=3,mass_kg_assumed=1.5,
 geometry_profile='NACA 2412 approximation; no aerodynamic polar computed',
 physics='Existing rc_demo synthetic coefficients, NOT derived from this geometry',
 rhino_axes='X right, Y forward, Z up',godot_axes='X right, Y up, Z aft',
 parts=[{k:v for k,v in p.items() if k!='mesh'} for p in parts])
(DEST/'aircraft.json').write_text(json.dumps(metadata,indent=2),encoding='utf8')
(ROOT/'game/assets/aircraft.json').write_text(json.dumps(metadata,indent=2),encoding='utf8')
print('Created Rhino7, GLB, OBJ:',len(parts),'parts')
print('Closed meshes:',all(p['mesh'].is_watertight for p in parts))
print('Bounds:',scene.bounds)
assert all(p['mesh'].is_watertight for p in parts)
check=rh.File3dm.Read(str(DEST/'Generic250_Rhino7.3dm'))
assert check.ArchiveVersion==70,check.ArchiveVersion
assert len(check.Objects)==len(parts)+len(curves)
print('Rhino archive verified:',check.ArchiveVersion,len(check.Objects),'objects')
