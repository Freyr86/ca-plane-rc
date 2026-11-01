import importlib.util
import math
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("analyze_topy_v0", ROOT / "simulation" / "analyze_topy_v0.py")
analysis = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = analysis
SPEC.loader.exec_module(analysis)


def test_uiuc_drag_file_contains_expected_reynolds_groups():
    groups = analysis.parse_uiuc_drag(ROOT / "models" / "topy_4522" / "airfoils" / "S4083A.DRG")
    assert {60000.0, 100000.0, 199100.0, 299000.0}.issubset(groups)
    assert all(len(groups[reynolds]) >= 10 for reynolds in (60000.0, 100000.0, 199100.0, 299000.0))


def test_nominal_topy_polar_is_finite_and_plausible():
    reference = __import__("json").loads((ROOT / "models" / "topy_4522" / "reference-data.json").read_text())
    groups = analysis.parse_uiuc_drag(ROOT / "models" / "topy_4522" / "airfoils" / "S4083A.DRG")
    rows = analysis.analyze(0.320, analysis.SCENARIOS[1], reference, groups)
    result = analysis.metrics(rows)
    assert len(rows) == 201
    assert all(math.isfinite(float(row[key])) for row in rows for key in ("speed_mps", "sink_mps", "glide_ratio"))
    assert 7.0 < result["max_glide_ratio"] < 25.0
    assert 4.0 < result["best_glide_speed_mps"] < 12.0
    assert 0.1 < result["min_sink_mps"] < 1.5


def test_stall_speed_increases_with_mass_and_decreases_with_clmax():
    area = 0.2078
    assert analysis.stall_speed(0.360, area, 1.10) > analysis.stall_speed(0.320, area, 1.10)
    assert analysis.stall_speed(0.320, area, 1.25) < analysis.stall_speed(0.320, area, 1.05)

