"""Build the original MDM-1 Fox half-scale visual in local flight coordinates."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from painted_glider import PaintedGlider

dest = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/Fox"
dest.mkdir(exist_ok=True)
mesh = PaintedGlider()
paint = Image.new("RGB", (1024, 512), "#efefea")
draw = ImageDraw.Draw(paint)
red, navy, black = "#cf2934", "#25374b", "#292b30"

# The upper wing carries a tapered flash. The belly carries broad dark bands
# so the pilot can read the bank angle even from the ground.
for top, bottom in [(0, 140), (151, 285)]:
    draw.polygon([(525, bottom), (812, top), (1023, top), (1023, bottom)], fill=red)
    draw.polygon([(465, bottom), (760, top), (783, top), (493, bottom)], fill=navy)
    if top:
        for x in (230, 425):
            draw.rectangle((x, top, x + 88, bottom), fill=navy)
    draw.line((390, bottom-27, 978, bottom-27), fill="#8e9392", width=1)
    draw.line((190, top+45, 440, top+45), fill="#909697", width=2)
# Side stripes in longitudinal fuselage UV space.
draw.polygon([(0, 347), (290, 347), (1023, 316), (1023, 338), (290, 356), (0, 351)], fill=red)
draw.polygon([(90, 358), (1023, 341), (1023, 345), (270, 360)], fill=navy)
for x, color in ((40,red),(160,navy),(250,black),(360,"#a3a9ae"),(460,"#eeeee8")):
    draw.rectangle((x-20,450,x+20,500),fill=color)
draw.rectangle((0,380,508,438),fill="#f0f1ed")
draw.polygon([(0,380),(508,380),(508,402),(0,387)],fill=red)
draw.polygon([(0,388),(508,404),(508,406),(0,390)],fill=navy)
draw.polygon([(219,412),(301,406),(279,413),(313,411),(238,424),(254,416)],fill=red)
for y in range(380,434):
    v=(y-380)/54
    draw.line((540,y,1000,y),fill=tuple(int(a+(b-a)*v) for a,b in zip((38,61,79),(117,147,159))))

angle=np.linspace(0,2*np.pi,64,endpoint=False)
def solid(x, n):
    return np.tile((x/1024, .93), (n,1))

# A single outer skin follows both the nose and the canopy crown. The glass
# shares its sill vertices with the fuselage, including around both end bows.
stations = np.array([  # X, half-width, keel, equator, crown (metres)
    [-2.44,.004,.025,.040,.055], [-2.36,.039,-.008,.046,.092],
    [-2.10,.054,-.032,.048,.114], [-1.50,.071,-.054,.049,.127],
    [-1.00,.100,-.103,.039,.146], [-.71,.139,-.158,.024,.179],
    [-.52,.164,-.204,.015,.222], [-.29,.188,-.250,.006,.288],
    [-.08,.202,-.280,-.002,.328], [.16,.212,-.301,-.012,.346],
    [.41,.213,-.302,-.027,.334], [.65,.202,-.280,-.043,.290],
    [.84,.176,-.237,-.055,.219], [1.01,.141,-.188,-.062,.128],
    [1.13,.102,-.141,-.060,.058], [1.205,.066,-.102,-.053,.005],
    [1.24,.031,-.074,-.048,-.022], [1.25,.004,-.051,-.047,-.043]])
xs = stations[:, 0]
profiles = stations[:, 1:]
tangents = np.gradient(profiles, xs, axis=0)
def profile(x):
    i = min(np.searchsorted(xs, x, side="right") - 1, len(xs) - 2)
    step = xs[i + 1] - xs[i]
    t = (x - xs[i]) / step
    return ((1 + 2*t)*(1-t)**2*profiles[i] + t*(1-t)**2*step*tangents[i]
            + t*t*(3-2*t)*profiles[i+1] + t*t*(t-1)*step*tangents[i+1])

# Reshape the shell and cabin fittings together around the wing datum.
body_beam = .84
body_depth = .81

def body_loft(rings, uvs, glass_panels=None):
    mesh.loft(np.asarray(rings) * [1., body_depth, body_beam], uvs, glass_panels)

back, front, bow = -.52, 1.045, .257
sections = np.unique(np.r_[xs, np.linspace(xs[0],xs[-1],143),back,front,bow-.016,bow+.016])
rings, uv, glass = [], [], []
for x in sections:
    width, keel, waist, crown = profile(x)
    # Rotate the two geometric sill curves smoothly up into the windscreen ends.
    progress = np.clip((x-back)/(front-back),0,1)
    end_curve = (np.exp(-progress*19) + np.exp(-(1-progress)*19))
    sill = .24 + .99*end_curve
    angles = np.r_[np.linspace(sill,np.pi-sill,33)[:-1],
                   np.linspace(np.pi-sill,2*np.pi+sill,49)[:-1]]
    sine = np.sin(angles)
    z = waist + np.where(sine>0,crown-waist,waist-keel)*sine
    rings.append(np.column_stack((np.full_like(z,x),z,width*np.cos(angles))))
    uv.append(np.column_stack((np.full_like(z,.002+.996*(1.25-x)/3.69), (342-30*sine)/512)))
for a,b in zip(sections,sections[1:]):
    mid = (a+b)*.5
    glass.append((np.arange(80)<32) & (back<mid<front) & (abs(mid-bow)>.016))
body_loft(rings, uv, np.array(glass))

# Inner cockpit, satin graphite furniture and two leather seats. Small solid
# pieces use a single atlas swatch for every vertex, including their end caps.
def rounded_part(center, radii, swatch, lean=0.):
    rings, uv = [], []
    for angle_z in np.linspace(-1.565,1.565,15):
        level = np.sin(angle_z)
        radial = np.cos(angle_z)
        x = center[0] + radii[0]*radial*np.cos(angle) + lean*level
        y = center[1] + radii[1]*radial*np.sin(angle)
        z = np.full_like(x,center[2]+radii[2]*level)
        rings.append(np.column_stack((x,z,y)))
        uv.append(solid(swatch,len(angle)))
    body_loft(rings,uv)

for x, colour in [(530,"#675946"),(590,"#323941"),(650,"#a39984"),(710,"#35302a")]:
    draw.rectangle((x-20,450,x+20,500),fill=colour)
# The dark inner coaming closes the view into the wing roots. Its top lip
# follows the canopy sill, with a flat cabin floor below the seat cushions.
rings, uv = [], []
for x in np.linspace(back,front,66):
    width, keel, waist, crown = profile(x)
    radius = width*.97
    floor = -.068
    rim = waist + (crown-waist)*.255
    lateral = np.r_[np.linspace(-1,1,27),np.linspace(.99,-.99,27)]
    wall = np.maximum(0.,(np.abs(lateral)-.55)/.45)
    heights = floor + (rim-floor)*wall**2
    heights[27:] -= .014
    rings.append(np.column_stack((np.full_like(lateral,x),heights,radius*lateral)))
    uv.append(solid(590,len(lateral)))
body_loft(rings,uv)
for seat_x in (-.16,.49):
    rounded_part((seat_x,0,-.012),(.152,.12,.042),530)
    rounded_part((seat_x-.096,0,.075),(.042,.119,.115),530,-.035)
    rounded_part((seat_x-.135,0,.188),(.039,.071,.035),710)
    for side in (-1,1):
        rounded_part((seat_x-.068,side*.059,.097),(.014,.012,.074),650,-.024)
    rounded_part((seat_x+.185,0,.086),(.049,.133,.084),590)
    for lateral,height in [(-.055,.110),(0,.122),(.055,.110),(0,.077)]:
        rounded_part((seat_x+.140,lateral,height),(.008,.020,.020),360)
        rounded_part((seat_x+.135,lateral,height),(.005,.016,.016),250)

# Cambered wing sections at 12% thickness; tail surfaces use symmetric sections.
u=(1-np.cos(np.linspace(0,np.pi,45)))/2
perim=np.r_[u[::-1],u[1:-1]]
sign=np.r_[np.ones(len(u)),-np.ones(len(u)-2)]
def foil(thickness,camber):
    x=perim
    return 4*camber*x*(1-x)+sign*5*thickness*(.2969*np.sqrt(x)-.126*x-.3516*x*x+.2843*x**3-.1036*x**4)
def surface(stations,side,vertical=False,wing=False):
    rings,uv=[],[]
    for reach,le,chord,datum in stations:
        h=chord*foil(.12 if wing else .095,.014 if wing else 0.)
        axis=np.full_like(h,reach*side)
        section_x = le - chord * perim
        if wing and reach == 0.:
            # Meet the actual curved fuselage at each point around the root.
            width, keel, waist, crown = np.array([profile(x) for x in section_x]).T
            z = (datum+h)/body_depth - waist
            radius = np.where(z > 0., crown-waist, waist-keel)
            axis = side*.99*body_beam*width*np.sqrt(np.maximum(0., 1.-(z/radius)**2))
        rings.append(np.column_stack((section_x,axis if vertical else datum+h,datum+h if vertical else axis)))
        span=stations[-1][0]
        v=np.where(sign>0,4+130*perim,154+124*perim)
        if vertical:
            uv.append(np.column_stack(((4+500*perim)/1024,np.full_like(h,(436-54*reach/span)/512))))
        else:
            uv.append(np.column_stack((np.full_like(h,.005+.99*reach/span),v/512)))
    mesh.loft(rings,uv)

for side in (-1,1):
    surface([(0.,.16,.57,.0),(1.30,.14,.53,.023),(2.55,.062,.39,.048),
             (3.32,-.035,.24,.071),(3.46,-.074,.15,.078),(3.5,-.118,.06,.081)],side,wing=True)
    surface([(0,-1.86,.41,.14),(.35,-1.91,.34,.145),(.73,-2.025,.18,.15),(.78,-2.075,.095,.15)],side)
surface([(.07,-1.45,.91,0),(.11,-1.57,.81,0),(.15,-1.72,.67,0),(.20,-1.81,.59,0),(.49,-2.02,.38,0),
         (.68,-2.13,.27,0),(.705,-2.17,.19,0)],1,vertical=True)

def wheel(x,z,r,width):
    rings,uv=[],[]
    for y,diameter,colour in [(-1,.55,360),(-.98,.77,250),(-.83,.95,250),(-.45,1,250),
                               (.45,1,250),(.83,.95,250),(.98,.77,250),(1,.55,360)]:
        rings.append(np.column_stack((x+r*diameter*np.cos(angle),z+r*diameter*np.sin(angle),np.full_like(angle,y*width/2))))
        uv.append(solid(colour,len(angle)))
    mesh.loft(rings,uv)
wheel(.08,-.277*body_depth,.135,.085)
wheel(-2.24,-.042*body_depth,.047,.034)
mesh.save(dest/"Fox7mEffile.glb",paint)
