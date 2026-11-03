"""Build FEMO's green APUS slope glider in metres, with the simulated CG at the origin."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from painted_glider import PaintedGlider


dest = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/Apus4"
dest.mkdir(exist_ok=True)
mesh = PaintedGlider()

# A single atlas gives the two faces independent paint while leaving a continuous
# carbon leading edge. Colour variation represents the woven cloth under resin.
width, height = 2048, 1024
py, px = np.indices((height, width))
over = (px // 4 + py // 4) % 4 < 2
tone = np.where(over, 4 + 3 * np.cos(px * np.pi / 2), 2 * np.cos(py * np.pi / 2))
carbon = np.uint8(34 + tone + 2 * np.sin((px + py) * np.pi / 16))
paint = Image.fromarray(np.stack((carbon - 2, carbon, carbon + 2), axis=-1))
draw = ImageDraw.Draw(paint)
green, silver, white = "#29d735", "#a7abb4", "#f1f0e9"
span_uv = .485
wing_row, wing_depth = .215, .18
tail_row, tail_depth = .610, .122


def pixel(side, span, chord, row, depth):
    return ((.5 + side * span_uv * span) * width, (row + depth * chord) * height)


def ribbon(side, row, depth, inner, outer, color):
    chord = np.linspace(-.015, 1.015, 81)
    left = [pixel(side, inner(t), t, row, depth) for t in chord]
    right = [pixel(side, outer(t), t, row, depth) for t in chord[::-1]]
    draw.polygon(left + right, fill=color)


def ray(t, root, end):
    return root + (end - root) * t ** 1.28 if t > 0 else root


for side in (-1, 1):
    # The two racing photographs show fans opening aft from the wing saddle.
    for row, depth, root, spread in ((wing_row, wing_depth, .109, 1.), (tail_row, tail_depth, .115, 1.94)):
        for start, end, color in ((.143, .182, white), (.187, .216, silver), (.222, .344, green)):
            ribbon(side, row, depth, lambda t: ray(t, root, start * spread),
                   lambda t: ray(t, root + .005, end * spread), color)

        for center, size in ((.43, .009), (.65, .016), (.87, .024)):
            u = ray(center, root + .0025, .283 * spread)
            points = []
            for i in range(10):
                angle = -np.pi / 2 + i * np.pi / 5
                radius = size * (1 if i % 2 == 0 else .41) * spread
                points.append(pixel(side, u + radius * np.cos(angle),
                                    center + radius * 2.9 / spread * np.sin(angle), row, depth))
            draw.polygon(points, fill="#17221a")

    # The upper crescents start in a point at the LE and broaden towards the TE.
    curve_t = np.linspace(0, 1, 11)
    inner_curve = [.49, .54, .585, .62, .646, .665, .678, .682, .674, .647, .59]
    outer_curve = [.49, .59, .64, .683, .715, .737, .751, .759, .756, .745, .715]
    def outer_arc(t):
        return np.interp(t, curve_t, outer_curve)

    ribbon(side, wing_row, wing_depth, lambda t: np.interp(t, curve_t, inner_curve), outer_arc, green)
    for inner, outer, color in ((.005, .041, silver), (.048, .078, white)):
        ribbon(side, wing_row, wing_depth, lambda t: outer_arc(t) + inner * max(t, 0)**.45,
               lambda t: outer_arc(t) + outer * max(t, 0)**.45, color)

    # The photographed green model has two white-edged underside blocks per
    # wing half and just one per stabilizer half.
    for row, depth, bands in ((wing_row, -wing_depth, ((.267, .420), (.575, .718))),
                              (tail_row, -tail_depth, ((.29, .53),))):
        for inner, outer in bands:
            ribbon(side, row, depth, lambda t: inner - .012, lambda t: outer + .012, white)
            ribbon(side, row, depth, lambda t: inner, lambda t: outer, green)

    # Fine aileron joint, ending before the rounded tips.
    for sign in (-1, 1):
        row, depth = wing_row, wing_depth * sign
        line = [pixel(side, s, .72 + .046 * s, row, depth) for s in np.linspace(.038, .922, 60)]
        draw.line(line + [pixel(side, .922, 1, row, depth)], fill="#2c2f2e", width=2)

# Original lettering, independent of the photographs. Shear the lettering to
# evoke the painted signatures seen on the two opposite half wings.
def lettering(text, x, y, size):
    font = ImageFont.load_default(size=size)
    label = Image.new("RGBA", (int(len(text) * size * .9 + size), size * 2))
    ImageDraw.Draw(label).text((size * .3, 0), text, font=font, fill=white)
    label = label.transform(label.size, Image.Transform.AFFINE, (1, .28, -size * .34, 0, 1, 0),
                            resample=Image.Resampling.BICUBIC)
    paint.paste(label, (x, y), label)


# Trace the strokes of a slanted, handwritten "Apus" with quadratic curves.
def signature(strokes, x, y):
    for start, pieces in strokes:
        line = [start]
        for control, end in pieces:
            a, b, c = map(np.array, (line[-1], control, end))
            line.extend((1-t)**2*a + 2*(1-t)*t*b + t*t*c for t in np.linspace(.1, 1, 10))
        draw.line([(x + u * .86, y + v * .86) for u, v in line], fill=white, width=3)

signature([
    ((0, 39), [((18, 21), (37, 0)), ((36, 15), (32, 41))]),
    ((5, 26), [((29, 22), (46, 24))]),
    ((39, 53), [((49, 27), (55, 16)), ((51, 26), (49, 31)), ((62, 12), (72, 17)),
                ((76, 27), (51, 31))]),
    ((76, 17), [((58, 46), (83, 26)), ((96, 12), (92, 17)), ((77, 43), (101, 24))]),
    ((119, 15), [((102, 14), (100, 23)), ((101, 26), (111, 27)), ((116, 34), (94, 38))])
], 503, 252)
lettering("modell und faserverbundtechnik", 528, 303, 9)
lettering("femodesign", 1380, 289, 20)

# The nose sleeve has a warmer woven finish in the overhead and flight photos.
cloth = carbon[810:1000, 1440:1740]
paint.paste(Image.fromarray(np.stack((cloth + 15, cloth + 14, cloth - 7), axis=-1)), (1440, 810))

# The following stations are digitised from the FEMO overhead catalogue view.
# Preserve the measured taper and use the published area to correct foreshortening.
# Stations are fractions of semispan, LE displacement aft, and local chord.
plan = np.array([
    (0, 0, .153), (.08, -.0014, .1491), (.15, -.0022, .1462),
    (.25, -.0043, .1429), (.35, -.0055, .1384), (.45, -.0068, .1334),
    (.55, -.0068, .1269), (.65, -.0055, .1190), (.75, -.0026, .1096),
    (.80, -.0002, .1038), (.85, .0027, .098), (.90, .0089, .0894),
    (.92, .0134, .0844), (.94, .0200, .0778), (.96, .0307, .0671),
    (.97, .0386, .0597), (.98, .0484, .0498), (.99, .0624, .0362),
    (.995, .074, .0247), (.999, .0851, .0136), (1, .096, .0026)])
plan[:, 0] *= .675
area = np.sum(np.diff(plan[:, 0]) * (plan[:-1, 2] + plan[1:, 2]))
plan[:, 1:] *= .184 / area
plan[:, 1] = .060 - plan[:, 1]
stab = [(0, -.550, .067), (.050, -.550, .065), (.084, -.551, .062),
        (.106, -.554, .057), (.124, -.559, .050), (.136, -.568, .040),
        (.143, -.583, .025), (.145, -.604, .004)]
# The fixed fin extends aft of the boom. Each half of the all-moving
# horizontal stabilizer is a single uninterrupted foil.
fin = [(0, -.521, .149), (.020, -.539, .129), (.048, -.561, .104),
       (.078, -.584, .077), (.102, -.604, .052), (.119, -.621, .029),
       (.128, -.635, .012), (.130, -.642, .004)]

u = .5 - .5 * np.cos(np.linspace(0, np.pi, 42))
perimeter = np.r_[u[::-1], u[1:]]
face = np.r_[np.ones(len(u)), -np.ones(len(u) - 1)]
profile = 5 * (.2969*np.sqrt(perimeter) - .126*perimeter - .3516*perimeter**2
               + .2843*perimeter**3 - .1015*perimeter**4)


def foil(stations, side, kind):
    rings, texcoords = [], []
    for reach, le, chord in stations:
        frac = reach / stations[-1][0]
        x = le - perimeter * chord
        if kind == "wing":
            section = face * profile * (.079 - .014 * frac) + .045 * perimeter * (1 - perimeter)
            y = .007 + .021 * reach + section * chord
            row, depth = wing_row, wing_depth
        else:
            section = face * profile * .067
            y = .009 + section * chord
            row, depth = tail_row, tail_depth
        lateral = np.full_like(x, reach * side)
        u_map = np.full_like(x, .5 + span_uv * frac * side)
        v_map = row + depth * perimeter * face
        if kind == "fin":
            lateral = section * chord
            y = np.full_like(x, .001 + reach)
            u_map = .84 + .14 * perimeter
            v_map = np.full_like(x, .77 + .18 * frac)
        rings.append(np.column_stack((x, y, lateral)))
        texcoords.append(np.column_stack((u_map, v_map)))
    mesh.loft(rings, texcoords)


for side in (-1, 1):
    foil(plan, side, "wing")
    foil(stab, side, "stab")
foil(fin, 1, "fin")

# A removable ogival nose, narrow fuselage saddle and uninterrupted tapered boom.
theta = np.linspace(0, 2 * np.pi, 48, endpoint=False)
rings, texcoords = [], []
for x, radius, vertical in [
    (.320, .0003, .0004), (.319, .0021, .0021), (.315, .0043, .0045),
    (.306, .0068, .0070), (.289, .0098, .0099), (.266, .0120, .0122),
    (.236, .0133, .0133), (.181, .0138, .0138), (.105, .0141, .0138),
    (.066, .0143, .0138), (.0582, .0144, .0139), (.058, .0144, .0139), (.053, .0151, .0142),
    (.016, .0150, .0145), (-.063, .0137, .0134), (-.113, .0101, .0101),
    (-.143, .0081, .0081), (-.300, .0073, .0073), (-.494, .0062, .0062),
    (-.566, .0058, .0060), (-.613, .0053, .0054), (-.632, .0034, .0038),
    (-.641, .0004, .0006)]:
    rings.append(np.column_stack((np.full_like(theta, x), vertical*np.sin(theta),
                                  radius*np.cos(theta))))
    sample_u = .72 + (.320 - x) * .44 if x > .058 else .02 + (.058 - x) * .94
    texcoords.append(np.column_stack((np.full_like(theta, sample_u),
                                      .88 + .046*np.cos(theta))))
mesh.loft(rings, texcoords)
mesh.save(dest / "Apus4.glb", paint)
