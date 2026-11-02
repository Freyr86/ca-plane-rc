"""Extract flight-resolution outer skins from Will Beavor's printable LongBow Mk2 STL archive."""
from pathlib import Path
import sys
from zipfile import ZipFile
import numpy as np
from PIL import Image, ImageDraw
from painted_glider import PaintedGlider

mesh = PaintedGlider()
out = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/LongBowMk2"
out.mkdir(exist_ok=True)
archive = ZipFile(sys.argv[1])
def triangles(name):
    data = archive.read("STLs/"+name+".stl")
    return np.frombuffer(data, dtype=[("normal","<f4",3),("v","<f4",(3,3)),("attribute","<u2")],offset=84)["v"].astype(float)

# Intersect each print with a spanwise station, then sample the upper/lower
# envelope of the segments. Internal ribs and carbon-tube sockets drop out.
u = (1-np.cos(np.linspace(0,np.pi,49)))/2
def section(tris, station):
    subset = tris[(tris[:,:,0].min(1)<station)&(tris[:,:,0].max(1)>station)]
    ends=[]
    for a,b in ((0,1),(1,2),(2,0)):
        p,q=subset[:,a],subset[:,b]
        mask=(p[:,0]<station)!=(q[:,0]<station)
        idx=np.flatnonzero(mask)
        hit=p[mask]+((station-p[mask,0])/(q[mask,0]-p[mask,0]))[:,None]*(q[mask]-p[mask])
        ends.extend(zip(idx,hit[:,1:]))
    ends.sort(key=lambda x:x[0])
    seg=np.array([v for _,v in ends]).reshape(-1,2,2)
    a,b=seg[:,0],seg[:,1]
    lo,hi=seg[:,:,0].min(),seg[:,:,0].max()
    x=hi-(hi-lo)*np.clip(u,.0001,.9999)
    upper=[]; lower=[]
    for chord in x:
        valid=(a[:,0]<chord)!=(b[:,0]<chord)
        h=a[valid,1]+(chord-a[valid,0])/(b[valid,0]-a[valid,0])*(b[valid,1]-a[valid,1])
        upper.append(h.max() if h.size else np.nan); lower.append(h.min() if h.size else np.nan)
    def fill(values):
        valid=np.isfinite(values)
        return np.interp(u,u[valid],np.asarray(values)[valid])
    return x,fill(upper),fill(lower)

# Coordinates in millimetres: span, forward from nose, up.
front=triangles("Centre Front")[:,:,[1,2,0]]
rear=triangles("Centre Rear")[:,:,[1,2,0]]
for part in (front,rear):
    part[:,:,0]-=(part[:,:,0].min()+part[:,:,0].max())/2
front[:,:,1]-=front[:,:,1].max()
joint=front[:,:,1].min()
rear[:,:,1]=joint-(rear[:,:,1]-rear[:,:,1].min())
front[:,:,2]+=1355
center=[]
for s in np.linspace(.03,124.96,18):
    x,t,b=section(front,s); xx,tt,bb=section(rear,s)
    # Interpolate across the glued seam and the removable hatch.
    cx=np.r_[x,xx]; top=np.r_[t,tt]; bot=np.r_[b,bb]
    center.append((s,cx,top,bot))

# All six right panels retain the design chord coordinates in print Y.
# Stitch the scan at its end sections and mirror the half-wing.
wing=[]
span=125.
for i in range(1,7):
    tri=triangles("RW"+str(i))[:,:,[2,1,0]]
    tri[:,:,0]-=tri[:,:,0].min()
    tri[:,:,2]*=-1
    size=tri[:,:,0].max()
    stations=np.linspace(.03,size-.03,18 if i!=4 else 42)
    for j,s in enumerate(stations):
        # Small print-bed bevels can cut off almost the entire first/end
        # contour. Extend the first complete profile to the glued seam.
        cut = np.clip(s,1.5,size-(.06 if i==6 else 1.5))
        x,t,b=section(tri,cut)
        if j==0:
            _,px,pt,pb=(wing or center)[-1]
            delta=np.interp(x[12],px[::-1],(.5*(pt+pb))[::-1])-.5*(t[12]+b[12])
        wing.append((span+s,x,t+delta,b+delta))
    span+=size

# The elevon is tilted on its print bed. Recover its principal plane before
# fitting its spanwise chord and thickness distribution along the hinge rebate.
ev=triangles("Right Elevon")
coords=np.unique(ev.reshape(-1,3),axis=0)
_,axes=np.linalg.eigh(np.cov(coords.T))
ev=(ev-coords.mean(0))@axes[:,[2,1,0]]
if axes[1,2]<0: ev[:,:,0]*=-1
size=np.ptp(ev[:,:,0]); ev[:,:,0]-=ev[:,:,0].min()
profiles=[section(ev,h) for h in np.linspace(.02,size-.02,40)]
start=next(k for k in range(1,len(wing)) if wing[k][1][-1]-wing[k-1][1][-1]>8)
a=wing[start][0]; end=wing[-1][0]
# A few millimetres of wing skin remain beyond the end of the hinge.
def loft(sections,side):
    rings=[];uv=[]
    for s,x,top,bot in sections:
        xp=np.r_[x,x[::-1]]; zp=np.r_[top,bot[::-1]]
        rings.append(np.column_stack((xp+214,zp+.115*(-xp)-13,np.full_like(xp,side*s)))*.001)
        uv.append(np.column_stack((np.full_like(xp,.5+side*s/2170),np.r_[.02+.42*(-x)/593,.53+.42*(-x[::-1])/593])))
    mesh.loft(rings,uv)

# Scale the reconstructed silhouette to the overall dimensions on manual p. 2.
sscale=1077.5/span
elev=[]
for s,x,top,bot in wing[start:]:
    f=(s-a)/(end-a)
    sec=profiles[min(len(profiles)-1,round(f*(len(profiles)-1)))]
    ex,et,eb=sec
    edge=x[-1]-.65
    # The hinge line's local sweep projects the measured chord onto longitudinal X.
    ex=edge-(ex.max()-ex)*1.04
    depth=(et-eb)/2
    height=.5*(top[-1]+bot[-1])+.11*(edge-ex)
    elev.append((s,ex,height+depth,height-depth))
length=-min(row[1].min() for rows in (center,wing,elev) for row in rows)
for rows in (center,wing,elev):
    for side in (-1,1):
        loft([(s*sscale,x*593/length,t,b) for s,x,t,b in rows],side)

paint=Image.new("RGB",(1024,512),"#e9e8e3")
d=ImageDraw.Draw(paint)
# Cover photo: one red tip and diagonal blue flash. Page 27: contrasting underside.
for left,color in [(756,"#226baf"),(788,"#efefea"),(816,"#c32b39")]:
    d.polygon([(left,0),(1024,0),(1024,250),(left-75,250)],fill=color)
for left,right in ((88,181),(278,371),(650,743),(838,931)):
    d.rectangle((left,263,right,511),fill="#272a30")
# Outline the flush central hatch in the printed white skin.
d.rounded_rectangle((482,30,542,56),radius=6,outline="#92918b",width=1)
# Parked folding pusher at the central trailing edge.
d.rectangle((0,250,10,260),fill="#393b40")
theta=np.linspace(0,2*np.pi,24,endpoint=False)
def prop_part(stations):
    rows=[];uv=[]
    for x,side,z,width,height in stations:
        rows.append(np.column_stack((np.full_like(theta,x),z+height*np.sin(theta),side+width*np.cos(theta)))*.001)
        uv.append(np.tile([.005,.498],(len(theta),1)))
    mesh.loft(rows,uv)
prop_part([(-86,0,4,13,12),(-107,0,4,12,11),(-119,0,4,7,7)])
for side in (-1,1):
    prop_part([(-115,side*8,4,3,3),(-128,side*12,4,6,2),(-153,side*18,4,7.5,1.6),
               (-182,side*22,4,5,1.2),(-204,side*24,4,1,1)])
mesh.save(out/"LongBowMk2.glb",paint)
print("Triangles",len(mesh.faces),"STL span",2*span,"length",length,"elevon start",a)
