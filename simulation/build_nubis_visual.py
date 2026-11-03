"""Reconstruct the 5.95 m Flight-Composites Nubis in its white, lime and grey finish.

The manufacturer supplies the projected area, span and length. Section shapes,
winglet curvature, hatch and paint are reconstructed from the product gallery.
glTF coordinates are metres: X points to the nose, Y up, Z along the span.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from painted_glider import PaintedGlider


destination = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/Nubis"
destination.mkdir(exist_ok=True)
mesh = PaintedGlider()
width, height = 2048, 1024
white, slate, lime = "#f5f6f4", "#758296", "#91d41c"
paint = Image.new("RGB", (width, height), white)
draw = ImageDraw.Draw(paint)

# Atlas bands contain a complete half-surface. Both tips carry the same scheme.
wing_rows = [(12, 228), (248, 464)]
tail_rows = [(485, 588), (607, 710)]
left, right = 12, width - 13


def pixel(row, span, chord):
    return (left + (right-left)*span, row[0] + (row[1]-row[0])*chord)


def band(row, start, end, color, skew):
    draw.polygon([pixel(row, start, -.015), pixel(row, end, -.015),
                  pixel(row, end+skew, 1.015), pixel(row, start+skew, 1.015)], fill=color)


for rows, start, stripes in (
        (wing_rows, .683, [(.548,.555), (.601,.620), (.650,.683), (.726,.771), (.781,.814), (.865,.884), (.901,.919)]),
        (tail_rows, .495, [(.301,.327), (.367,.406), (.454,.495), (.543,.640), (.688,.776), (.813,.865)])):
    for row in rows:
        band(row, start, 1.03, lime, .008)
        for inner, outer in stripes:
            band(row, inner, outer, slate, .008)
        # A fine shadow at the moulded hinge, interrupted at the panel breaks.
        panels = [(.027,.407), (.412,.676), (.680,.910)] if rows is wing_rows else [(.065,.895)]
        for inner, outer in panels:
            draw.line([pixel(row, inner, .731), pixel(row, outer, .761),
                       pixel(row, outer, 1)], fill="#aeb7b7", width=1)

# Four-piece wing joint and the covers of the 370 mm upper-surface airbrakes.
for row in wing_rows:
    draw.line([pixel(row,2.09/2.975,0), pixel(row,2.09/2.975,1)], fill="#b7bdbd", width=1)
row = wing_rows[0]
draw.rounded_rectangle([pixel(row,.267,.477), pixel(row,.267+.370/2.975,.541)],
                       radius=2, outline="#acb3b5", width=1)

# Rudder: two diagonal grey stripes alongside the green cap, as on the fin
# photographed at the slope. Horizontal texture coordinate runs LE to TE.
fin_left, fin_right, fin_top, fin_bottom = 14, 982, 746, 1008
def fin_pixel(chord, reach):
    return (fin_left+(fin_right-fin_left)*chord, fin_bottom-(fin_bottom-fin_top)*reach)

for low, high, color in [(.73,1.40,lime),(.49,.555,slate),(.602,.675,slate)]:
    draw.polygon([fin_pixel(0,low),fin_pixel(1,low-.16),fin_pixel(1,high-.16),fin_pixel(0,high)],fill=color)
draw.line([fin_pixel(.716,.067),fin_pixel(.795,.815)],fill="#b1b8ba",width=1)

# The silver hatch is opaque. Draw the carbon-cloth pattern in its own island
# with mirrored sides and a continuous rim; no photographic pixels are used.
body_left, body_right, body_mid, body_v = 1010, 2036, 878, 117
body_start, body_end = .80, -.16
yy, xx = np.mgrid[738:1015, 995:width]
axial = body_start-(xx-body_left)/(body_right-body_left)*(body_start-body_end)
sin_angle = (body_mid-yy)/body_v
hatch_u = np.clip((axial-.077)/(.699-.077),0,1)
edge = .23 + .77 * (1-np.maximum(0, 1-(2*hatch_u-1)**4)**.5)
inside = (axial>.077) & (axial<.699) & (sin_angle>=edge)
fiber = np.where(((xx+yy)//3+(xx-yy)//4)%2==0, 2.6, -2.6)
silver = 141+fiber+15*np.clip((sin_angle-.20)/.8,0,1)
material = np.array(paint.crop((995,738,width,1015)))
for channel, offset in enumerate((-5, 1, 8)):
    material[:,:,channel] = np.where(inside, silver+offset, material[:,:,channel])
rim = inside & ((sin_angle-edge)<.026)
material[rim] = (92,100,106)
paint.paste(Image.fromarray(material),(995,738))


# Fuselage ordinates are X, half-width, underside and crown relative to the CG.
# Shape-preserving tangents avoid facets along the white shoulder and the boom.
hull = np.array([
    (-1.398,.001,-.004,.002),(-1.373,.007,-.015,.013),(-1.325,.012,-.018,.016),
    (-1.14,.015,-.022,.020),(-.81,.018,-.027,.022),(-.53,.024,-.038,.029),
    (-.34,.034,-.049,.035),(-.20,.047,-.058,.042),(-.065,.059,-.067,.050),
    (.095,.064,-.071,.060),(.220,.066,-.073,.069),(.39,.063,-.070,.073),
    (.53,.056,-.064,.068),(.625,.045,-.056,.059),(.704,.031,-.042,.043),
    (.764,.016,-.025,.025),(.793,.006,-.013,.008),(.800,.0004,-.004,-.002)])


def smooth_sections(table, positions):
    station = table[:,0]
    values = table[:,1:]
    slopes = np.diff(values, axis=0)/np.diff(station)[:,None]
    tangents = np.zeros_like(values)
    tangents[0], tangents[-1] = slopes[0], slopes[-1]
    # A limited harmonic mean at each join prevents overshooting a radius or tip.
    a,b = slopes[:-1], slopes[1:]
    np.divide(2*a*b, a+b, out=tangents[1:-1], where=(a*b>0))
    i = np.clip(np.searchsorted(station, positions, side="right")-1, 0, len(station)-2)
    dx = (station[i+1]-station[i])[:,None]
    t = (positions-station[i])[:,None]/dx
    return ((1+2*t)*(1-t)**2*values[i] + t*(1-t)**2*dx*tangents[i]
            + t*t*(3-2*t)*values[i+1] + t*t*(t-1)*dx*tangents[i+1])


theta = np.linspace(0,2*np.pi,56,endpoint=False)
sine, cosine = np.sin(theta), np.cos(theta)
stations = np.unique(np.r_[np.linspace(hull[0,0],hull[-1,0],112), hull[:,0]])
rings, uvs = [], []
for x, (radius, low, high) in zip(stations,smooth_sections(hull,stations)):
    z = (high+low)*.5+(high-low)*.5*sine
    rings.append(np.column_stack((np.full_like(z,x), z, radius*cosine)))
    u = body_left+(body_right-body_left)*np.clip((body_start-x)/(body_start-body_end),0,1)
    uvs.append(np.column_stack((np.full_like(z,u/width),(body_mid-body_v*sine)/height)))
mesh.loft(rings,uvs)

# Stations are projected half-span (or height for the fin), leading-edge X,
# local chord and vertical datum. Span is measured across the upright winglets.
wing = np.array([
    (0,.090,.269,.018),(.60,.093,.265,.029),(1.25,.090,.237,.046),
    (1.80,.069,.196,.065),(2.22,.035,.157,.085),(2.50,-.005,.120,.107),
    (2.68,-.044,.092,.128),(2.78,-.079,.073,.151),(2.86,-.115,.063,.184),
    (2.91,-.145,.055,.241),(2.95,-.174,.042,.318),(2.975,-.204,.010,.372)])
tail = np.array([(0,-1.093,.225,.073),(.18,-1.093,.211,.074),
                 (.34,-1.107,.177,.076),(.44,-1.136,.120,.079),
                 (.485,-1.169,.066,.081),(.500,-1.203,.005,.082)])
fin = np.array([(-.015,-1.061,.309,0),(.02,-1.009,.382,0),
                (.080,-1.055,.353,0),(.175,-1.108,.302,0),
                (.26,-1.165,.245,0),(.329,-1.224,.179,0),
                (.372,-1.281,.117,0),(.400,-1.344,.048,0),(.405,-1.375,.008,0)])


def surface(table, side, kind):
    reach = np.unique(np.r_[table[:,0],np.linspace(table[0,0],table[-1,0],72 if kind=="wing" else 34)])
    le, chord, rise = smooth_sections(table,reach).T
    if kind=="wing":
        chord *= 1.12/np.sum(np.diff(reach)*(chord[:-1]+chord[1:]))
    section_angle = np.arctan(np.gradient(rise,reach))
    s = (1-np.cos(np.linspace(0,np.pi,39)))/2
    p = np.r_[s[::-1],s[1:-1]]
    face = np.r_[np.ones_like(s),-np.ones_like(s[1:-1])]
    outline = 5*(.2969*np.sqrt(p)-.126*p-.3516*p*p+.2843*p**3-.1036*p**4)
    rings, uvs = [], []
    for r,l,c,h,angle in zip(reach,le,chord,rise,section_angle):
        frac = (r-reach[0])/(reach[-1]-reach[0])
        thick = (.134-.052*frac) if kind=="wing" else (.089 if kind=="fin" else .073)
        camber = .090*p*(1-p)*(1-frac**4) if kind=="wing" else 0.
        foil = c*(thick*outline*face+camber)
        if kind=="fin":
            y = foil
            z = np.full_like(p,r)
            u,v = fin_pixel(p,frac)
            uv = np.column_stack((u/width, np.full_like(p,v/height)))
        else:
            y = side*(r-foil*np.sin(angle))
            z = h+foil*np.cos(angle)
            rows = wing_rows if kind=="wing" else tail_rows
            v = np.where(face>0, rows[0][0]+p*(rows[0][1]-rows[0][0]),
                         rows[1][0]+p*(rows[1][1]-rows[1][0]))
            uv = np.column_stack((np.full_like(p,(left+(right-left)*frac)/width),v/height))
        rings.append(np.column_stack((l-c*p,z,y)))
        uvs.append(uv)
    mesh.loft(rings,uvs)


for side in (-1,1):
    surface(wing,side,"wing")
    surface(tail,side,"tail")
surface(fin,1,"fin")
mesh.save(destination/"Nubis.glb",paint)
