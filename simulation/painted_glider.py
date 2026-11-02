"""Loft closed cross-sections into a single textured glTF mesh, in metres."""
import io
import json
import struct

import numpy as np


class PaintedGlider:
    def __init__(self):
        self.vertices = []
        self.normals = []
        self.uvs = []
        self.faces = []
        self.glass_faces = []

    def loft(self, rings, uv_rings, glass_panels=None):
        points = np.asarray(rings)
        rows, cols = points.shape[:2]
        mesh = points.reshape(-1, 3)
        idx = np.arange(rows * cols).reshape(rows, cols)
        a, b = idx[:-1], np.roll(idx[:-1], -1, axis=1)
        c, d = np.roll(idx[1:], -1, axis=1), idx[1:]
        triangles = np.concatenate([
            np.stack([a, b, c], axis=-1).reshape(-1, 3),
            np.stack([a, c, d], axis=-1).reshape(-1, 3),
            [[0, j + 1, j] for j in range(1, cols - 1)],
            [[idx[-1, 0], idx[-1, j], idx[-1, j + 1]] for j in range(1, cols - 1)],
        ])
        cross = np.cross(mesh[triangles[:, 1]] - mesh[triangles[:, 0]],
                         mesh[triangles[:, 2]] - mesh[triangles[:, 0]])
        # Reflected wings have the opposite winding. Use signed volume to
        # orient each closed loft before averaging area-weighted normals.
        if np.sum(mesh[triangles[:, 0]] * cross) < 0:
            triangles = triangles[:, ::-1]
            cross = -cross
        normals = np.zeros_like(mesh)
        for column in triangles.T:
            np.add.at(normals, column, cross)
        normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-15)
        triangles = triangles + len(self.vertices)
        if glass_panels is not None:
            # Each panel contributes one triangle to each of the two side batches.
            mask = np.concatenate((glass_panels.ravel(), glass_panels.ravel(),
                                   np.zeros(2 * (cols - 2), dtype=bool)))
            self.glass_faces.extend(triangles[mask])
            triangles = triangles[~mask]
        self.faces.extend(triangles)
        self.vertices.extend(mesh)
        self.normals.extend(normals)
        self.uvs.extend(np.reshape(uv_rings, (-1, 2)))

    def save(self, path, paint):
        png = io.BytesIO()
        paint.save(png, format="PNG")
        buffer = bytearray()
        views = []
        accessors = []

        def append_view(data):
            buffer.extend(b"\0" * (-len(buffer) % 4))
            views.append(dict(buffer=0, byteOffset=len(buffer), byteLength=len(data)))
            buffer.extend(data)
            return len(views) - 1

        for values, kind, dtype in [(self.vertices, "VEC3", "<f4"), (self.normals, "VEC3", "<f4"),
                                    (self.uvs, "VEC2", "<f4"), (self.faces, "SCALAR", "<u4")]:
            array = np.asarray(values, dtype=dtype)
            indices = kind == "SCALAR"
            entry = dict(bufferView=append_view(array.tobytes()), componentType=5125 if indices else 5126,
                         count=array.size if indices else len(array), type=kind)
            if values is self.vertices:
                entry.update(min=array.min(axis=0).tolist(), max=array.max(axis=0).tolist())
            accessors.append(entry)
        if self.glass_faces:
            glass_indices = np.asarray(self.glass_faces, dtype="<u4")
            accessors.append(dict(bufferView=append_view(glass_indices.tobytes()), componentType=5125,
                                  count=glass_indices.size, type="SCALAR"))
        image = append_view(png.getvalue())
        primitive = dict(attributes=dict(POSITION=0, NORMAL=1, TEXCOORD_0=2), indices=3, material=0)
        finish = dict(baseColorTexture=dict(index=0), metallicFactor=0, roughnessFactor=.34)
        primitives = [primitive]
        materials = [dict(name=path.stem + "_Peinture", pbrMetallicRoughness=finish)]
        if self.glass_faces:
            primitives.append(dict(primitive, indices=4, material=1))
            materials.append(dict(name=path.stem + "_Verriere", alphaMode="BLEND",
                                  pbrMetallicRoughness=dict(baseColorFactor=[.48,.69,.78,.26],
                                                           metallicFactor=0., roughnessFactor=.12)))
        doc = dict(
            asset={"version": "2.0"}, scene=0, scenes=[{"nodes": [0]}],
            nodes=[dict(name=path.stem, mesh=0)], meshes=[dict(name=path.stem, primitives=primitives)],
            materials=materials,
            textures=[dict(source=0)], images=[dict(name=path.stem + "_Livree", bufferView=image, mimeType="image/png")],
            buffers=[dict(byteLength=len(buffer))], bufferViews=views, accessors=accessors,
        )
        header = json.dumps(doc, separators=(",", ":")).encode()
        header += b" " * (-len(header) % 4)
        buffer.extend(b"\0" * (-len(buffer) % 4))
        prefix = struct.pack("<4sIIII", b"glTF", 2, 28 + len(header) + len(buffer), len(header), 0x4e4f534a)
        path.write_bytes(prefix + header + struct.pack("<I4s", len(buffer), b"BIN\0") + buffer)
