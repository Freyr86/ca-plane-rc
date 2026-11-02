"""Create the half-scale JS-3 Rapture visual, in metres around its wing datum."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from painted_glider import PaintedGlider

dest = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/JS3"
dest.mkdir(exist_ok=True)
mesh = PaintedGlider()
white, teal, navy = "#f3f3ed", "#008c97", "#203a4c"
paint = Image.new("RGB", (1024, 512), white)
draw = ImageDraw.Draw(paint)
# Upper and lower wing paint are separate islands so the underside is readable
# from the pilot station. Control-surface hinge lines run aft of the spar.
for top, bottom in [(4,125),(136,258)]:
    draw.polygon([(760,bottom),(935,top),(1023,top),(1023,bottom)],fill=teal)
    draw.polygon([(724,bottom),(910,top),(922,top),(747,bottom)],fill=navy)
    draw.line([(45,bottom-27),(585,bottom-27),(925,bottom-23)],fill="#909b9c",width=1)
    if top>100:
        for x in (330,450):
            draw.polygon([(x,top),(x+43,top),(x+61,bottom),(x+15,bottom)],fill=navy)
draw.polygon([(5,322),(282,317),(677,296),(1009,290),(760,311),(300,325),(5,325)],fill=teal)
draw.polygon([(80,327),(377,321),(877,301),(720,312),(340,326)],fill=navy)
# Fin insignia and small satin material swatches for the cabin.
draw.polygon([(0,365),(500,365),(500,382),(0,371)],fill=teal)
draw.polygon([(0,373),(500,384),(500,387),(0,376)],fill=navy)
draw.polygon([(125,407),(270,387),(236,402),(300,401),(191,420),(209,405)],fill=teal)
for u,color in [(70,"#43515a"),(175,"#172635"),(290,"#a69a7c"),(400,"#c9d1d4"),(505,"#24272b")]:
    draw.rectangle((u-25,450,u+25,510),fill=color)

angle=np.linspace(0,2*np.pi,48,endpoint=False)
def swatch(u,n):
    return np.tile([u/1024,.94],(n,1))

# One continuous skin. A subset of panels over the single seat is acrylic.
# X, half breadth, keel, widest point and crown.
stations=np.array([
    [-2.29,.004,-.064,-.035,-.01],[-2.22,.032,-.084,-.035,.025],
    [-1.83,.046,-.091,-.037,.033],[-1.22,.055,-.104,-.037,.046],
    [-.81,.078,-.130,-.045,.062],[-.47,.117,-.177,-.059,.093],
    [-.20,.147,-.218,-.062,.123],[.04,.166,-.256,-.070,.155],
    [.29,.177,-.273,-.076,.197],[.51,.175,-.261,-.081,.209],
    [.72,.158,-.222,-.080,.176],[.93,.12,-.167,-.074,.095],
    [1.08,.079,-.127,-.068,.017],[1.15,.043,-.093,-.063,-.027],
    [1.18,.004,-.065,-.060,-.055]])
locations,profiles=stations[:,0],stations[:,1:]
slopes=np.gradient(profiles,locations,axis=0)
def section(x):
    i=np.clip(np.searchsorted(locations,x,side="right")-1,0,len(locations)-2)
    step=locations[i+1]-locations[i]
    t=(x-locations[i])/step
    return ((1+2*t)*(1-t)**2*profiles[i] + t*(1-t)**2*step*slopes[i]
            + t*t*(3-2*t)*profiles[i+1] + t*t*(t-1)*step*slopes[i+1])

aft,fore=-.045,1.04
samples=np.unique(np.r_[locations,np.linspace(-2.29,1.18,125),aft,fore])
rings,uv,glass=[],[],[]
for x in samples:
    width,lower,waist,upper=section(x)
    f=np.clip((x-aft)/(fore-aft),0,1)
    sill=.13+1.21*(np.exp(-17*f)+np.exp(-17*(1-f)))
    theta=np.r_[np.linspace(sill,np.pi-sill,33)[:-1],np.linspace(np.pi-sill,2*np.pi+sill,49)[:-1]]
    sine=np.sin(theta)
    z=waist+np.where(sine>0,upper-waist,waist-lower)*sine
    rings.append(np.column_stack((np.full_like(z,x),z,width*np.cos(theta))))
    uv.append(np.column_stack((np.full_like(z,.005+.99*(1.18-x)/3.47),(315-36*sine)/512)))
for x in (samples[:-1]+samples[1:])*.5:
    glass.append((np.arange(80)<32) & (aft<x<fore))
mesh.loft(rings,uv,np.array(glass))

def fitting(x,y,z,rx,ry,rz,color,lean=0.):
    rings,uv=[],[]
    for v in np.linspace(-1.565,1.565,15):
        h=np.sin(v)
        rings.append(np.column_stack((x+rx*np.cos(v)*np.cos(angle)+lean*h,
                                      np.full_like(angle,z+rz*h),y+ry*np.cos(v)*np.sin(angle))))
        uv.append(swatch(color,len(angle)))
    mesh.loft(rings,uv)
fitting(.47,0,-.126,.49,.157,.066,175)
fitting(.42,0,-.065,.17,.112,.041,70)
fitting(.27,0,.014,.056,.115,.111,70,-.063)
fitting(.203,0,.123,.043,.063,.032,70)
for y in (-.047,.047):
    fitting(.302,y,.038,.013,.014,.077,290,-.043)
fitting(.708,0,.013,.082,.123,.083,175)
for y,z in [(-.054,.04),(0,.052),(.054,.04),(0,.006)]:
    fitting(.639,y,z,.008,.017,.017,400)
    fitting(.634,y,z,.005,.014,.014,505)
fitting(.01,0,-.260,.09,.030,.09,505)
fitting(-2.14,0,-.068,.045,.016,.045,505)

chordwise=(1-np.cos(np.linspace(0,np.pi,45)))*.5
perimeter=np.r_[chordwise[::-1],chordwise[1:-1]]
sign=np.r_[np.ones(len(chordwise)),-np.ones(len(chordwise)-2)]
def wing_sections(plan,side,vertical=False,main=False):
    rings,uv=[],[]
    for reach,le,chord,rise in plan:
        x=perimeter
        thickness=.115 if main else .088
        y=sign*5*thickness*(.2969*np.sqrt(x)-.126*x-.3516*x*x+.2843*x**3-.1036*x**4)
        if main: y+=.078*x*(1-x)
        profile_y=rise+chord*y
        axis=np.full_like(x,side*reach)
        axial=le-chord*x
        if main and reach==0:
            for j,at in enumerate(axial):
                width,low,waist,high=section(at)
                axis[j]=side*.985*width*np.sqrt(max(0.,1.-((profile_y[j]-waist)/(high-waist))**2))
        rings.append(np.column_stack((axial,axis if vertical else profile_y,profile_y if vertical else axis)))
        if vertical:
            uv.append(np.column_stack(((5+489*x)/1024,np.full_like(x,(427-61*reach/plan[-1][0])/512))))
        else:
            uv.append(np.column_stack((np.full_like(x,.005+.99*reach/plan[-1][0]),
                                       np.where(sign>0,6+116*x,138+117*x)/512)))
    mesh.loft(rings,uv)

# High wing, progressively swept tips and continuously blended upturned winglets.
plan=np.array([(0,.116,.392,.064),(1.36,.132,.362,.083),(2.54,.118,.284,.111),
               (3.37,.056,.218,.142),(3.94,-.045,.159,.176),(4.26,-.129,.110,.212),
               (4.38,-.177,.081,.252),(4.44,-.215,.067,.318),
               (4.48,-.260,.052,.411),(4.5,-.314,.022,.495)])
projected_area=np.sum(np.diff(plan[:,0])*(plan[:-1,2]+plan[1:,2]))
plan[:,2]*=2.4875/projected_area
for side in (-1,1):
    wing_sections(plan,side,main=True)
    wing_sections([(0,-1.99,.265,.576),(.32,-2.025,.209,.580),(.57,-2.078,.136,.586),
                   (.69,-2.128,.061,.588),(.71,-2.147,.029,.588)],side)
wing_sections([(-.048,-1.56,.71,0),(.06,-1.69,.587,0),(.15,-1.81,.47,0),
               (.40,-1.98,.299,0),(.56,-2.045,.23,0),(.593,-2.081,.178,0)],1,vertical=True)
mesh.save(dest/"JS3_9m.glb",paint)
