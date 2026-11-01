"""Generate the provisional Graupner Topy 4522 model for JSBSim."""

from __future__ import annotations

import importlib.util
import json
import math
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / "models" / "topy_4522"


def load_analysis_module():
    path = ROOT / "simulation" / "analyze_topy_v0.py"
    spec = importlib.util.spec_from_file_location("topy_analysis_for_builder", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def interpolate(x: float, points: list[tuple[float, float]]) -> float:
    ordered = sorted(points)
    if x <= ordered[0][0]:
        return ordered[0][1]
    if x >= ordered[-1][0]:
        return ordered[-1][1]
    for left, right in zip(ordered, ordered[1:]):
        if left[0] <= x <= right[0]:
            f = (x - left[0]) / (right[0] - left[0])
            return left[1] + f * (right[1] - left[1])
    raise AssertionError("Interpolation interval not found")


def build_aero_tables(reference: dict, analysis) -> tuple[list[tuple[float, float]], list[float], list[list[float]]]:
    groups = analysis.parse_uiuc_drag(MODEL_DIR / "airfoils" / "S4083A.DRG")
    area = reference["published_geometry_si"]["wing_area_m2"]
    span = reference["published_geometry_si"]["wingspan_m"]
    chord = area / span
    aspect_ratio = span * span / area
    e = 0.75
    misc_cd = 0.010

    # Build the attached-flow 3-D lift curve from the nominal V0 polar:
    # profile incidence plus finite-wing induced incidence.
    nominal = analysis.analyze(0.320, analysis.SCENARIOS[1], reference, groups)
    attached: list[tuple[float, float]] = []
    for cl_target in [i / 20 for i in range(0, 24)]:  # 0.00 .. 1.15
        nearest = min(nominal, key=lambda row: abs(float(row["cl"]) - max(0.20, cl_target)))
        if cl_target < 0.20:
            # Extrapolate the measured positive-lift slope toward zero lift.
            alpha_deg = -2.46 + cl_target / 0.084
        else:
            profile_alpha = float(nearest["alpha_profile_deg"])
            induced_alpha = math.degrees(cl_target / (math.pi * e * aspect_ratio))
            alpha_deg = profile_alpha + induced_alpha
        attached.append((alpha_deg, cl_target))

    lift_points = [
        (-30.0, -0.20),
        (-20.0, -0.40),
        (-12.0, -0.52),
        (-8.0, -0.46),
        (-5.0, -0.22),
    ] + attached + [
        (15.0, 1.12),
        (16.5, 0.96),
        (20.0, 0.68),
        (30.0, 0.32),
    ]
    # Remove nearly duplicate alpha entries while preserving the later stall points.
    dedup: dict[float, float] = {}
    for alpha, cl in lift_points:
        dedup[round(alpha, 3)] = cl
    lift_points = sorted(dedup.items())

    speeds_mps = [3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 12.0, 15.0]
    drag_rows: list[list[float]] = []
    for alpha_deg, cl in lift_points:
        values: list[float] = []
        for speed in speeds_mps:
            reynolds = speed * chord / analysis.NU
            _, profile_cd, _ = analysis.profile_at(groups, cl, reynolds)
            induced_cd = cl * cl / (math.pi * e * aspect_ratio)
            positive_stall = max(0.0, alpha_deg - 14.0)
            negative_stall = max(0.0, -8.0 - alpha_deg)
            stall_cd = 0.004 * (positive_stall ** 1.55 + negative_stall ** 1.45)
            values.append(min(1.20, profile_cd + induced_cd + misc_cd + stall_cd))
        drag_rows.append(values)
    return lift_points, speeds_mps, drag_rows


def product(*items: str) -> str:
    return "<product>" + "".join(items) + "</product>"


def prop(name: str) -> str:
    return f"<property>{name}</property>"


def value(number: float) -> str:
    return f"<value>{number:.9g}</value>"


def term(coefficient: float, *properties: str) -> str:
    return product(value(coefficient), *(prop(name) for name in properties))


def force_function(name: str, expression: str, moment_axis: str | None = None) -> str:
    factors = [prop("aero/qbar-psf"), prop("metrics/Sw-sqft")]
    if moment_axis:
        factors.append(prop("metrics/bw-ft" if moment_axis in ("roll", "yaw") else "metrics/cbarw-ft"))
    return f'<function name="aero/{name}">' + product(*factors, expression) + "</function>"


def table_1d(property_name: str, rows: list[tuple[float, float]]) -> str:
    data = "\n".join(f"{x:.7f} {y:.7f}" for x, y in rows)
    return f'<table><independentVar lookup="row">{property_name}</independentVar><tableData>{data}</tableData></table>'


def table_2d(row_property: str, column_property: str, rows: list[tuple[float, list[float]]], columns: list[float]) -> str:
    header = " ".join(f"{column:.6f}" for column in columns)
    body = "\n".join(f"{row:.7f} " + " ".join(f"{value_:.7f}" for value_ in values) for row, values in rows)
    return (
        f'<table><independentVar lookup="row">{row_property}</independentVar>'
        f'<independentVar lookup="column">{column_property}</independentVar>'
        f"<tableData>{header}\n{body}</tableData></table>"
    )


def generate() -> Path:
    reference = json.loads((MODEL_DIR / "reference-data.json").read_text(encoding="utf-8"))
    analysis = load_analysis_module()
    lift_points_deg, speeds_mps, drag_values = build_aero_tables(reference, analysis)
    geometry = reference["published_geometry_si"]
    lift_table = table_1d(
        "aero/alpha-rad",
        [(math.radians(alpha), cl) for alpha, cl in lift_points_deg],
    )
    drag_table = table_2d(
        "aero/alpha-rad",
        "velocities/vt-fps",
        [(math.radians(alpha), values) for (alpha, _), values in zip(lift_points_deg, drag_values)],
        [speed / 0.3048 for speed in speeds_mps],
    )

    # Provisional inertias derived from a component-mass approximation. They are
    # intentionally visible here and in assumptions-v0.json for later replacement.
    # Topy flight-feel trial: 15% less rotational inertia on each axis.
    ixx, iyy, izz = 0.014875, 0.0102, 0.024225
    trim_alpha = math.radians(3.5)
    cma = -0.75
    cm0 = -cma * trim_alpha

    flight_control = """
 <flight_control name="Topy provisional V-tail">
  <channel name="V-tail mixing and servo rate">
   <summer name="fcs/vtail-left-command-norm">
    <input>fcs/elevator-cmd-norm</input><input>fcs/rudder-cmd-norm</input>
    <clipto><min>-1</min><max>1</max></clipto>
   </summer>
   <summer name="fcs/vtail-right-command-norm">
    <input>fcs/elevator-cmd-norm</input><input>-fcs/rudder-cmd-norm</input>
    <clipto><min>-1</min><max>1</max></clipto>
   </summer>
   <actuator name="fcs/vtail-left-actuator">
    <input>fcs/vtail-left-command-norm</input><rate_limit>3.0</rate_limit>
    <clipto><min>-1</min><max>1</max></clipto><output>fcs/vtail-left-pos-norm</output>
   </actuator>
   <actuator name="fcs/vtail-right-actuator">
    <input>fcs/vtail-right-command-norm</input><rate_limit>3.0</rate_limit>
    <clipto><min>-1</min><max>1</max></clipto><output>fcs/vtail-right-pos-norm</output>
   </actuator>
   <summer name="fcs/vtail-pitch-sum"><input>fcs/vtail-left-pos-norm</input><input>fcs/vtail-right-pos-norm</input></summer>
   <pure_gain name="fcs/vtail-pitch-pos-norm"><input>fcs/vtail-pitch-sum</input><gain>0.5</gain></pure_gain>
   <summer name="fcs/vtail-yaw-difference"><input>fcs/vtail-left-pos-norm</input><input>-fcs/vtail-right-pos-norm</input></summer>
   <pure_gain name="fcs/vtail-yaw-pos-norm"><input>fcs/vtail-yaw-difference</input><gain>0.5</gain></pure_gain>
  </channel>
 </flight_control>"""

    axes = {
        "LIFT": force_function("lift", lift_table),
        "DRAG": force_function(
            "drag",
            "<sum>" + drag_table + term(0.012, "fcs/vtail-pitch-pos-norm", "fcs/vtail-pitch-pos-norm") + term(0.010, "fcs/vtail-yaw-pos-norm", "fcs/vtail-yaw-pos-norm") + "</sum>",
        ),
        "SIDE": force_function(
            "side",
            "<sum>" + term(-0.34, "aero/beta-rad") + term(0.04, "fcs/vtail-yaw-pos-norm") + "</sum>",
        ),
        "ROLL": force_function(
            "roll",
            "<sum>" + term(-0.085, "aero/beta-rad") + term(-0.80, "aero/bi2vel", "velocities/p-aero-rad_sec") + term(-0.03, "fcs/vtail-yaw-pos-norm") + "</sum>",
            "roll",
        ),
        "PITCH": force_function(
            "pitch",
            "<sum>" + value(cm0) + term(cma, "aero/alpha-rad") + term(-7.0, "aero/ci2vel", "velocities/q-aero-rad_sec") + term(-0.09, "fcs/vtail-pitch-pos-norm") + "</sum>",
            "pitch",
        ),
        "YAW": force_function(
            "yaw",
            "<sum>" + term(0.18, "aero/beta-rad") + term(-0.60, "aero/bi2vel", "velocities/r-aero-rad_sec") + term(-0.04, "fcs/vtail-yaw-pos-norm") + "</sum>",
            "yaw",
        ),
    }

    xml = f'''<?xml version="1.0"?>
<fdm_config name="Graupner Topy 4522 V0 - PROVISIONAL" version="2.0">
 <fileheader>
  <author>Simulateur RC</author>
  <description>Provisional Topy model using UIUC S4083(A) low-Re wind-tunnel drag data. CG, inertias and stability/control derivatives are assumptions, not measurements.</description>
 </fileheader>
 <metrics>
  <wingarea unit="M2">{geometry['wing_area_m2']}</wingarea>
  <wingspan unit="M">{geometry['wingspan_m']}</wingspan>
  <chord unit="M">{geometry['wing_area_m2']/geometry['wingspan_m']:.8f}</chord>
  <location name="AERORP" unit="M"><x>0</x><y>0</y><z>0</z></location>
  <location name="EYEPOINT" unit="M"><x>0</x><y>0</y><z>0</z></location>
  <location name="VRP" unit="M"><x>0</x><y>0</y><z>0</z></location>
 </metrics>
 <mass_balance>
  <ixx unit="KG*M2">{ixx}</ixx><iyy unit="KG*M2">{iyy}</iyy><izz unit="KG*M2">{izz}</izz>
  <ixy unit="KG*M2">0</ixy><ixz unit="KG*M2">0</ixz><iyz unit="KG*M2">0</iyz>
  <emptywt unit="KG">0.320</emptywt>
  <location name="CG" unit="M"><x>0</x><y>0</y><z>0</z></location>
 </mass_balance>
 <ground_reactions/>
{flight_control}
 <aerodynamics>
  {''.join(f'<axis name="{axis}">{function}</axis>' for axis, function in axes.items())}
 </aerodynamics>
</fdm_config>
'''
    destination = MODEL_DIR / "topy_4522.xml"
    destination.write_text(xml, encoding="utf-8")
    assumptions = {
        "status": "provisional_unvalidated",
        "measured_or_published": {
            "mass_kg": 0.320,
            "wing_area_m2": geometry["wing_area_m2"],
            "wingspan_m": geometry["wingspan_m"],
            "candidate_profile_data": "UIUC S4083(A) drag polar"
        },
        "assumed": {
            "center_of_gravity_from_root_le_m": 0.061,
            "inertia_kg_m2": {"ixx": ixx, "iyy": iyy, "izz": izz},
            "span_efficiency": 0.75,
            "misc_drag_coefficient": 0.010,
            "trim_alpha_deg": 3.5,
            "vtail_servo_full_travel_time_s": 1 / 3,
            "stability_and_control_derivatives": "explicit in topy_4522.xml"
        }
    }
    (MODEL_DIR / "assumptions-v0.json").write_text(json.dumps(assumptions, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return destination


if __name__ == "__main__":
    print(generate())

