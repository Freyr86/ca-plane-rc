"""Generate a dimensionally plausible, explicitly provisional Topy visual model."""

from pathlib import Path
import json
import math

import numpy as np
import rhino3dm as rh
import trimesh


ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "models" / "topy_4522"
GAME_ASSETS = ROOT / "game" / "assets"
UNREAL_SOURCE_ASSETS = ROOT / "unreal" / "SimPlaneurUE" / "SourceAssets"
CREAM = [242, 239, 220, 255]
RED = [196, 45, 43, 255]
DARK = [25, 31, 38, 255]
scene = trimesh.Scene()
parts = []
construction_curves = []


def closed_loft(rings):
    count = len(rings[0])
    vertices = np.array(rings).reshape((-1, 3)).tolist()
    faces = []
    for index in range(len(rings) - 1):
        for point in range(count):
            a = index * count + point
            b = index * count + (point + 1) % count
            c = (index + 1) * count + (point + 1) % count
            d = (index + 1) * count + point
            faces.extend([[a, b, c], [a, c, d]])
    for index in (0, len(rings) - 1):
        center = len(vertices)
        vertices.append(np.mean(rings[index], axis=0).tolist())
        for point in range(count):
            faces.append([center, index * count + point, index * count + (point + 1) % count])
    mesh = trimesh.Trimesh(vertices, faces, process=True)
    mesh.fix_normals()
    return mesh


def add(name, mesh, color, pivot=None, axis=None):
    if pivot is None:
        pivot = [0, 0, 0]
    mesh.visual = trimesh.visual.texture.TextureVisuals(
        material=trimesh.visual.material.PBRMaterial(
            baseColorFactor=color, metallicFactor=0.0, roughnessFactor=0.58
        )
    )
    local = mesh.copy()
    local.apply_translation(-np.array(pivot))
    transform = np.eye(4)
    transform[:3, 3] = pivot
    scene.add_geometry(local, node_name=name, geom_name=name, transform=transform)
    parts.append(dict(name=name, mesh=mesh, color=color, pivot=pivot, axis=axis))


def read_s4083():
    points = []
    for line in (DEST / "airfoils" / "s4083-uiuc.dat").read_text().splitlines()[1:]:
        fields = line.split()
        if len(fields) == 2:
            point = (float(fields[0]), float(fields[1]))
            if not points or point != points[-1]:
                points.append(point)
    if points[0] == points[-1]:
        points.pop()
    return points


S4083 = read_s4083()
WING_STATIONS = [
    (0.000, 0.186, -0.105, 0.000),
    (0.200, 0.184, -0.102, 0.000),
    (0.400, 0.165, -0.091, 0.025),
    (0.595, 0.135, -0.076, 0.075),
]


def station_values(span_position):
    for left, right in zip(WING_STATIONS, WING_STATIONS[1:]):
        if left[0] <= span_position <= right[0]:
            fraction = (span_position - left[0]) / (right[0] - left[0])
            return tuple(left[i] + fraction * (right[i] - left[i]) for i in range(1, 4))
    return WING_STATIONS[-1][1:]


def wing_section(span_position, side):
    chord, leading_edge, height = station_values(span_position)
    return [[side * span_position, height + y * chord, leading_edge + x * chord] for x, y in S4083]


for side, label in ((-1, "left"), (1, "right")):
    span_samples = sorted(set(np.linspace(0.008, 0.595, 18).tolist() + [0.2, 0.4]))
    rings = [wing_section(float(span_position), side) for span_position in span_samples]
    add("wing_" + label, closed_loft(rings), CREAM)
    for span_position in (0.0, 0.2, 0.4, 0.595):
        construction_curves.append((f"wing_section_{label}_{span_position}", wing_section(span_position, side)))


TAIL_DIHEDRAL = math.radians(35.0)  # 110-degree included V angle, provisional.


def symmetric_profile(x, thickness=0.05):
    return 5 * thickness * (
        0.2969 * math.sqrt(max(0, x)) - 0.1260 * x - 0.3516 * x * x
        + 0.2843 * x ** 3 - 0.1036 * x ** 4
    )


def tail_geometry(distance):
    fraction = distance / 0.19
    chord = 0.115 + fraction * (0.070 - 0.115)
    leading_edge = 0.315 + fraction * 0.045
    return chord, leading_edge


def tail_section(distance, side, low, high):
    chord, leading_edge = tail_geometry(distance)
    x_position = side * distance * math.cos(TAIL_DIHEDRAL)
    y_center = 0.010 + distance * math.sin(TAIL_DIHEDRAL)
    values = np.linspace(low, high, 28)
    ring = []
    for sign, sequence in ((1, values), (-1, values[::-1])):
        for x in sequence:
            thickness = sign * symmetric_profile(float(x), 0.05) * chord
            # Thickness is normal to the panel plane.
            ring.append([
                x_position - side * thickness * math.sin(TAIL_DIHEDRAL),
                y_center + thickness * math.cos(TAIL_DIHEDRAL),
                leading_edge + float(x) * chord,
            ])
    output = []
    for point in ring:
        if not output or np.linalg.norm(np.array(point) - output[-1]) > 1e-9:
            output.append(point)
    if np.linalg.norm(np.array(output[0]) - output[-1]) < 1e-9:
        output.pop()
    return output


for side, label in ((-1, "left"), (1, "right")):
    distances = np.linspace(0.012, 0.19, 9)
    add("vtail_fixed_" + label, closed_loft([tail_section(float(d), side, 0.0, 0.69) for d in distances]), CREAM)
    root_distance, tip_distance, hinge = 0.012, 0.19, 0.71
    root_chord, root_le = tail_geometry(root_distance)
    tip_chord, tip_le = tail_geometry(tip_distance)
    pivot = [
        side * root_distance * math.cos(TAIL_DIHEDRAL),
        0.010 + root_distance * math.sin(TAIL_DIHEDRAL),
        root_le + hinge * root_chord,
    ]
    tip = [
        side * tip_distance * math.cos(TAIL_DIHEDRAL),
        0.010 + tip_distance * math.sin(TAIL_DIHEDRAL),
        tip_le + hinge * tip_chord,
    ]
    axis = (np.array(tip) - np.array(pivot)).tolist()
    add(
        "vtail_" + label,
        closed_loft([tail_section(float(d), side, 0.71, 1.0) for d in distances]),
        RED,
        pivot,
        axis,
    )


# Slender glass-fibre pod and tail boom; dimensions are visual hypotheses.
stations = [
    (-0.430, 0.002, 0.002, -0.012),
    (-0.412, 0.020, 0.026, -0.008),
    (-0.360, 0.034, 0.041, -0.002),
    (-0.270, 0.041, 0.048, 0.000),
    (-0.150, 0.039, 0.044, -0.004),
    (-0.040, 0.030, 0.034, -0.010),
    (0.080, 0.021, 0.025, -0.012),
    (0.230, 0.013, 0.016, -0.004),
    (0.360, 0.008, 0.010, 0.006),
    (0.430, 0.003, 0.004, 0.012),
]
fuselage_rings = []
for z, radius_x, radius_y, center_y in stations:
    fuselage_rings.append([
        [radius_x * math.cos(angle), center_y + radius_y * math.sin(angle), z]
        for angle in np.linspace(0, 2 * math.pi, 48, endpoint=False)
    ])
add("fuselage", closed_loft(fuselage_rings), CREAM)
canopy = trimesh.creation.uv_sphere(count=[18, 28])
canopy.apply_scale([0.031, 0.021, 0.095])
canopy.apply_translation([0, 0.036, -0.270])
add("canopy", canopy, DARK)


DEST.mkdir(parents=True, exist_ok=True)
GAME_ASSETS.mkdir(parents=True, exist_ok=True)
UNREAL_SOURCE_ASSETS.mkdir(parents=True, exist_ok=True)


def to_rhino(point):
    # Rhino: X right, Y forward, Z up. Godot: X right, Y up, Z aft.
    return rh.Point3d(float(point[0]), float(-point[2]), float(point[1]))


file = rh.File3dm()
file.Settings.ModelUnitSystem = rh.UnitSystem.Meters
file.Settings.ModelAbsoluteTolerance = 0.0001
for part in parts:
    layer = rh.Layer()
    layer.Name = part["name"]
    layer.Color = tuple(part["color"])
    layer_index = file.Layers.Add(layer)
    attributes = rh.ObjectAttributes()
    attributes.Name = part["name"]
    attributes.LayerIndex = layer_index
    attributes.SetUserString("status", "TOPY V0 - GEOMETRIE PROVISOIRE")
    attributes.SetUserString("units", "metres")
    if part["axis"]:
        attributes.SetUserString("hinge_godot_m", json.dumps(part["pivot"]))
    mesh = rh.Mesh()
    for point in part["mesh"].vertices:
        vertex = to_rhino(point)
        mesh.Vertices.Add(vertex.X, vertex.Y, vertex.Z)
    for face in part["mesh"].faces:
        mesh.Faces.AddFace(*(int(index) for index in face))
    mesh.Normals.ComputeNormals()
    mesh.Compact()
    assert mesh.IsValid, part["name"]
    file.Objects.AddMesh(mesh, attributes)

layer = rh.Layer()
layer.Name = "construction_sections"
layer.Visible = False
layer_index = file.Layers.Add(layer)
for name, points in construction_curves:
    attributes = rh.ObjectAttributes()
    attributes.Name = name
    attributes.LayerIndex = layer_index
    file.Objects.AddCurve(rh.PolylineCurve([to_rhino(point) for point in points + [points[0]]]), attributes)

rhino_path = DEST / "Topy4522V0_Rhino7.3dm"
assert file.Write(str(rhino_path), 7)
glb = scene.export(file_type="glb")
(DEST / "Topy4522V0.glb").write_bytes(glb)
(GAME_ASSETS / "Topy4522V0.glb").write_bytes(glb)

with (DEST / "Topy4522V0.obj").open("w", encoding="ascii") as handle:
    handle.write("# Topy 4522 V0 provisional geometry. Metres. X right, Y forward, Z up.\n")
    offset = 1
    for part in parts:
        handle.write("o " + part["name"] + "\n")
        for point in part["mesh"].vertices:
            vertex = to_rhino(point)
            handle.write(f"v {vertex.X:.7f} {vertex.Y:.7f} {vertex.Z:.7f}\n")
        for face in part["mesh"].faces:
            handle.write("f " + " ".join(str(int(index) + offset) for index in face) + "\n")
        offset += len(part["mesh"].vertices)

# Unreal uses centimetres, X forward, Y right and Z up. The source geometry is
# X right, Y up and Z aft, so this dedicated OBJ avoids hidden import transforms.
unreal_obj_path = UNREAL_SOURCE_ASSETS / "Topy4522V0_Unreal.obj"
with unreal_obj_path.open("w", encoding="ascii") as handle:
    handle.write("# Topy 4522 V0 provisional geometry. Centimetres. X forward, Y right, Z up.\n")
    offset = 1
    for part in parts:
        handle.write("o " + part["name"] + "\n")
        for point in part["mesh"].vertices:
            handle.write(f"v {-point[2]*100:.7f} {point[0]*100:.7f} {point[1]*100:.7f}\n")
        # Interchange's OBJ importer expects a UV index when it builds vertex data.
        # A neutral UV is enough here because this provisional mesh uses flat materials.
        for _ in part["mesh"].vertices:
            handle.write("vt 0.0 0.0\n")
        # Axis conversion changes handedness, hence the reversed winding.
        for face in part["mesh"].faces:
            indices = [int(index) + offset for index in reversed(face)]
            handle.write("f " + " ".join(f"{index}/{index}" for index in indices) + "\n")
        offset += len(part["mesh"].vertices)

metadata = {
    "name": "Graupner Topy 4522 V0",
    "status": "provisional_unvalidated_geometry",
    "span_m": 1.19,
    "length_m": 0.86,
    "wing_area_m2_published": 0.2078,
    "root_chord_m_published": 0.186,
    "tip_chord_m_published": 0.135,
    "mass_kg": 0.320,
    "geometry_profile": "S4083 coordinates; fixed trailing-edge modification not represented",
    "assumptions": "Planform stations, polyhedral heights, fuselage shape and V-tail angle are provisional",
    "rhino_axes": "X right, Y forward, Z up",
    "godot_axes": "X right, Y up, Z aft",
    "parts": [{key: value for key, value in part.items() if key != "mesh"} for part in parts],
}
(DEST / "visual-model.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
(GAME_ASSETS / "topy_aircraft.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")

assert all(part["mesh"].is_watertight for part in parts)
check = rh.File3dm.Read(str(rhino_path))
assert check.ArchiveVersion == 70
assert len(check.Objects) == len(parts) + len(construction_curves)
print("Created Topy V0 visual model:", len(parts), "closed parts")
print("Bounds:", scene.bounds)
print("Rhino archive:", check.ArchiveVersion, len(check.Objects), "objects")

