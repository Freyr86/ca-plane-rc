"""Run through Unreal Python after compiling the new terrain.

Exports the authoritative physical terrain without spawning an actor, opening a
level, or changing a map asset. Then run render_cirque_map.py with normal Python.
"""
import array
import json
import pathlib
import sys

import unreal


def main():
    project = pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    output = project.parents[1] / "work" / "site-atlas" / "cirque-export"
    output.mkdir(parents=True, exist_ok=True)
    size, north, west, span = 897, 5600.0, -5600.0, 11200.0
    step = span / (size - 1)
    heights = array.array("f")
    for row in range(size):
        x = 100.0 * (north - row * step)
        heights.extend(
            unreal.SlopeWorld.terrain_height_cm(x, 100.0 * (west + col * step)) * 0.01
            for col in range(size)
        )
        if row % 112 == 0:
            unreal.log("CIRQUE_MAP_EXPORT row {}/{}".format(row, size - 1))
    if sys.byteorder != "little":
        heights.byteswap()
    target = output / "massif-after-cirque.f32"
    temporary = target.with_suffix(".tmp")
    temporary.write_bytes(heights.tobytes())
    temporary.replace(target)
    metadata = {
        "size": size, "north_m": north, "west_m": west, "span_m": span,
        "step_m": step, "dtype": "<f4", "height_unit": "m",
        "axes": "row: X north to south; column: Y west to east",
        "source": "SlopeWorld.terrain_height_cm",
        "center_x_m": 2680.0, "center_y_m": 1600.0, "radius_m": 1100.0,
        "height_file": target.name,
    }
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    unreal.log("CIRQUE_MAP_EXPORT_READY " + str(target))


if __name__ == "__main__":
    main()
