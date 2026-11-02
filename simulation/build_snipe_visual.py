"""Build the original carbon/orange Snipe 2 DLG visual in metres, CG at the origin."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from painted_glider import PaintedGlider

destination = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/Snipe2"
destination.mkdir(exist_ok=True)
mesh = PaintedGlider()
# A low-contrast diagonal weave keeps the carbon readable close to the pilot.
y,x = np.indices((512,1024))
weave = 32 + np.where(((x+y)//6)%2,5,0) + np.where(((x-y)//5)%2,4,0)
paint = Image.fromarray(np.uint8(np.stack((weave-3,weave,weave+3),axis=-1)))
draw = ImageDraw.Draw(paint)
orange,amber,white = "#fa5426","#ffad21","#f1eee1"
for top,bottom in ((5,178),(185,358)):
    draw.polygon([(450,bottom),(810,top),(1023,top),(1023,bottom)],fill=orange)
    draw.polygon([(325,bottom),(742,top),(766,top),(400,bottom)],fill=amber)
    draw.polygon([(795,bottom),(943,top),(986,top),(878,bottom)],fill=white)
    if top>180:
        for at in (205,336):
            draw.polygon([(at,top),(at+52,top),(at+80,bottom),(at+28,bottom)],fill=white)
    draw.line((65,bottom-42,976,bottom-35),fill="#5a504c",width=2)
# Dedicated longitudinal pod stripe. The tail boom uses the bare carbon swatch.
draw.polygon([(0,425),(235,408),(450,415),(270,419),(27,437)],fill=orange)
draw.polygon([(24,439),(196,426),(243,425),(94,438)],fill=amber)
draw.rectangle((912,470,959,510),fill="#e17424")
theta=np.linspace(0,2*np.pi,48,endpoint=False)
def solid(u,v,n):
    return np.tile([u/1024,v/512],(n,1))

rings,uv=[],[]
# Slender removable nose and a carbon tail boom, joined by one smooth skin.
for at,width,height,axis in [
    (.260,.001,.001,-.006),(.256,.006,.005,-.006),(.244,.010,.010,-.006),
    (.217,.014,.015,-.005),(.168,.018,.019,-.005),(.109,.019,.021,-.006),
    (.043,.018,.021,-.007),(-.031,.016,.017,-.009),(-.103,.012,.013,-.009),
    (-.192,.009,.010,-.008),(-.39,.007,.007,-.006),(-.62,.005,.005,-.004),
    (-.78,.004,.004,-.003),(-.821,.001,.002,-.003)]:
    rings.append(np.column_stack((np.full_like(theta,at),axis+height*np.sin(theta),width*np.cos(theta))))
    uv.append(np.column_stack((np.full_like(theta,.005+.98*(.26-at)/1.081),(421-24*np.sin(theta))/512)))
mesh.loft(rings,uv)

u=(1-np.cos(np.linspace(0,np.pi,45)))/2
perim=np.r_[u[::-1],u[1:-1]]
sign=np.r_[np.ones(len(u)),-np.ones(len(u)-2)]
def lifting_surface(plan,side,fin=False,wing=False):
    rings,uv=[],[]
    for reach,le,chord,datum in plan:
        x=perim
        h=sign*5*(.068 if wing else .055)*(.2969*np.sqrt(x)-.126*x-.3516*x*x+.2843*x**3-.1036*x**4)
        if wing: h+=.071*x*(1-x)
        axis=np.full_like(x,reach*side)
        rings.append(np.column_stack((le-chord*x,axis if fin else datum+chord*h,datum+chord*h if fin else axis)))
        uv.append(np.column_stack((np.full_like(x,.004+.99*abs(reach)/max(abs(s[0]) for s in plan)),
                                   np.where(sign>0,7+168*x,188+165*x)/512)))
    mesh.loft(rings,uv)

# Elliptically tapered planform. Match the documented projected area including
# the centre section and keep the root leading edge at the published CG datum.
plan=np.array([(0,.068,.183,.012),(.16,.068,.181,.024),(.34,.062,.160,.039),
               (.51,.048,.132,.052),(.63,.025,.091,.062),(.706,-.006,.052,.067),
               (.736,-.021,.025,.070),(.745,-.031,.006,.071)])
area=np.sum(np.diff(plan[:,0])*(plan[:-1,2]+plan[1:,2]))
plan[:,2]*=.1965/area
for side in (-1,1):
    lifting_surface(plan,side,wing=True)
    lifting_surface([(0,-.648,.097,-.010),(.088,-.657,.087,-.010),
                     (.150,-.678,.054,-.010),(.170,-.701,.014,-.010)],side)
lifting_surface([(-.103,-.777,.025,0),(-.077,-.731,.082,0),(-.025,-.697,.122,0),
                 (.008,-.705,.115,0),(.052,-.736,.080,0),(.079,-.777,.027,0)],1,fin=True)

# Through-wing launch blade on the left tip for a right-handed throw.
rings,uv=[],[]
for z,dx,half,thick in [(.035,0,.006,.001),(.039,.001,.012,.002),(.055,.002,.010,.002),
                        (.072,.002,.006,.002),(.090,.006,.009,.0025),(.103,.009,.012,.002),
                        (.106,.010,.006,.001)]:
    rings.append(np.column_stack((-.023+dx+half*np.cos(theta),np.full_like(theta,z),
                                  -.709+thick*np.sin(theta))))
    uv.append(solid(934,490,len(theta)))
mesh.loft(rings,uv)
mesh.save(destination/"Snipe2.glb",paint)
