"""Preliminary, traceable glide-polar analysis for the Graupner Topy.

This is deliberately not a flight-dynamics model.  It combines published Topy
dimensions with UIUC wind-tunnel drag measurements for the candidate S4083
section.  Unknown whole-aircraft drag and span efficiency are varied explicitly.
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / "models" / "topy_4522"
REPORT_DIR = ROOT / "reports" / "topy_v0"
RHO = 1.225
NU = 1.5e-5
G = 9.80665


@dataclass(frozen=True)
class Scenario:
    name: str
    span_efficiency: float
    misc_cd: float


SCENARIOS = (
    Scenario("optimiste", 0.85, 0.006),
    Scenario("nominal", 0.75, 0.010),
    Scenario("prudent", 0.65, 0.016),
)


def parse_uiuc_drag(path: Path) -> dict[float, list[tuple[float, float, float]]]:
    """Return Reynolds -> [(alpha_deg, Cl, Cd)] from a UIUC .DRG file."""
    lines = path.read_text(encoding="ascii").splitlines()
    groups: dict[float, list[tuple[float, float, float]]] = {}
    i = 0
    while i < len(lines):
        if lines[i].strip() != "Average Reynolds #:" or i + 1 >= len(lines):
            i += 1
            continue
        reynolds = float(lines[i + 1].strip())
        i += 2
        while i < len(lines) and "alpha /" not in lines[i]:
            i += 1
        i += 1
        rows: list[tuple[float, float, float]] = []
        while i < len(lines):
            fields = lines[i].split()
            if len(fields) < 3:
                break
            try:
                alpha, cl, cd = map(float, fields[:3])
            except ValueError:
                break
            rows.append((alpha, cl, cd))
            i += 1
        if rows:
            groups[reynolds] = rows
    return groups


def lerp(x: float, x0: float, x1: float, y0: float, y1: float) -> float:
    if x1 == x0:
        return y0
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0)


def interpolate_rows_by_cl(rows: list[tuple[float, float, float]], cl: float) -> tuple[float, float]:
    ordered = sorted(rows, key=lambda row: row[1])
    if cl <= ordered[0][1]:
        return ordered[0][0], ordered[0][2]
    if cl >= ordered[-1][1]:
        return ordered[-1][0], ordered[-1][2]
    for left, right in zip(ordered, ordered[1:]):
        if left[1] <= cl <= right[1]:
            alpha = lerp(cl, left[1], right[1], left[0], right[0])
            cd = lerp(cl, left[1], right[1], left[2], right[2])
            return alpha, cd
    raise AssertionError("Cl interpolation interval not found")


def profile_at(groups: dict[float, list[tuple[float, float, float]]], cl: float, reynolds: float) -> tuple[float, float, bool]:
    """Interpolate alpha/Cd in Cl and logarithmic Reynolds space.

    The boolean is true when Reynolds had to be clamped to the measured range.
    """
    usable = sorted(re for re, rows in groups.items() if len(rows) >= 5)
    clamped = reynolds < usable[0] or reynolds > usable[-1]
    target = min(max(reynolds, usable[0]), usable[-1])
    if target <= usable[0]:
        alpha, cd = interpolate_rows_by_cl(groups[usable[0]], cl)
        return alpha, cd, clamped
    if target >= usable[-1]:
        alpha, cd = interpolate_rows_by_cl(groups[usable[-1]], cl)
        return alpha, cd, clamped
    for re0, re1 in zip(usable, usable[1:]):
        if re0 <= target <= re1:
            a0, cd0 = interpolate_rows_by_cl(groups[re0], cl)
            a1, cd1 = interpolate_rows_by_cl(groups[re1], cl)
            f = math.log(target / re0) / math.log(re1 / re0)
            return lerp(f, 0, 1, a0, a1), lerp(f, 0, 1, cd0, cd1), clamped
    raise AssertionError("Reynolds interpolation interval not found")


def analyze(mass_kg: float, scenario: Scenario, reference: dict, groups: dict) -> list[dict[str, float | bool | str]]:
    area = reference["published_geometry_si"]["wing_area_m2"]
    span = reference["published_geometry_si"]["wingspan_m"]
    chord = area / span
    aspect_ratio = span * span / area
    weight = mass_kg * G
    rows: list[dict[str, float | bool | str]] = []
    # This interval stays inside the useful positive-lift measurements.
    for step in range(201):
        cl = 0.20 + step * 0.005
        speed = math.sqrt(2 * weight / (RHO * area * cl))
        reynolds = speed * chord / NU
        alpha, profile_cd, re_clamped = profile_at(groups, cl, reynolds)
        induced_cd = cl * cl / (math.pi * scenario.span_efficiency * aspect_ratio)
        total_cd = profile_cd + induced_cd + scenario.misc_cd
        glide_ratio = cl / total_cd
        flight_path = math.atan2(total_cd, cl)
        sink = speed * math.sin(flight_path)
        horizontal_speed = speed * math.cos(flight_path)
        rows.append(
            {
                "scenario": scenario.name,
                "mass_kg": mass_kg,
                "cl": cl,
                "alpha_profile_deg": alpha,
                "speed_mps": speed,
                "horizontal_speed_mps": horizontal_speed,
                "sink_mps": sink,
                "reynolds": reynolds,
                "profile_cd": profile_cd,
                "induced_cd": induced_cd,
                "misc_cd": scenario.misc_cd,
                "total_cd": total_cd,
                "glide_ratio": glide_ratio,
                "reynolds_clamped": re_clamped,
            }
        )
    return rows


def metrics(rows: list[dict[str, float | bool | str]]) -> dict[str, float]:
    best_glide = max(rows, key=lambda row: float(row["glide_ratio"]))
    measured_rows = [row for row in rows if not bool(row["reynolds_clamped"])]
    min_sink = min(measured_rows, key=lambda row: float(row["sink_mps"]))
    extrapolated_min_sink = min(rows, key=lambda row: float(row["sink_mps"]))
    return {
        "max_glide_ratio": float(best_glide["glide_ratio"]),
        "best_glide_speed_mps": float(best_glide["speed_mps"]),
        "best_glide_sink_mps": float(best_glide["sink_mps"]),
        "min_sink_mps": float(min_sink["sink_mps"]),
        "min_sink_speed_mps": float(min_sink["speed_mps"]),
        "best_glide_reynolds": float(best_glide["reynolds"]),
        "min_sink_reynolds": float(min_sink["reynolds"]),
        "clamped_min_sink_estimate_mps": float(extrapolated_min_sink["sink_mps"]),
        "clamped_min_sink_speed_mps": float(extrapolated_min_sink["speed_mps"]),
    }


def stall_speed(mass_kg: float, area: float, cl_max: float) -> float:
    return math.sqrt(2 * mass_kg * G / (RHO * area * cl_max))


def write_csv(path: Path, rows: list[dict[str, float | bool | str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def svg_path(points: list[tuple[float, float]], xmap, ymap) -> str:
    return " ".join(("M" if i == 0 else "L") + f" {xmap(x):.1f} {ymap(y):.1f}" for i, (x, y) in enumerate(points))


def write_svg(path: Path, rows_by_scenario: dict[str, list[dict[str, float | bool | str]]]) -> None:
    width, height = 920, 520
    left, right, top, bottom = 78, 32, 44, 62
    xmin, xmax, ymin, ymax = 4.0, 12.0, 0.0, 1.25
    xmap = lambda x: left + (x - xmin) / (xmax - xmin) * (width - left - right)
    ymap = lambda y: top + (ymax - y) / (ymax - ymin) * (height - top - bottom)
    colors = {"optimiste": "#2e8b57", "nominal": "#1769aa", "prudent": "#d2691e"}
    chunks = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#fbfcfe"/>',
        '<text x="78" y="25" font-family="sans-serif" font-size="18" font-weight="bold">Topy V0 — polaire de chute estimée à 320 g</text>',
    ]
    for y in (0, 0.25, 0.5, 0.75, 1.0, 1.25):
        py = ymap(y)
        chunks.append(f'<line x1="{left}" y1="{py:.1f}" x2="{width-right}" y2="{py:.1f}" stroke="#dfe4ea"/>')
        chunks.append(f'<text x="{left-12}" y="{py+4:.1f}" text-anchor="end" font-family="sans-serif" font-size="12">{y:.2f}</text>')
    for x in range(4, 13):
        px = xmap(x)
        chunks.append(f'<line x1="{px:.1f}" y1="{top}" x2="{px:.1f}" y2="{height-bottom}" stroke="#edf0f3"/>')
        chunks.append(f'<text x="{px:.1f}" y="{height-bottom+22}" text-anchor="middle" font-family="sans-serif" font-size="12">{x}</text>')
    for name, rows in rows_by_scenario.items():
        valid_points = sorted(
            (float(row["horizontal_speed_mps"]), float(row["sink_mps"]))
            for row in rows if not bool(row["reynolds_clamped"])
        )
        clamped_points = sorted(
            (float(row["horizontal_speed_mps"]), float(row["sink_mps"]))
            for row in rows if bool(row["reynolds_clamped"])
        )
        valid_points = [(x, y) for x, y in valid_points if xmin <= x <= xmax and ymin <= y <= ymax]
        clamped_points = [(x, y) for x, y in clamped_points if xmin <= x <= xmax and ymin <= y <= ymax]
        chunks.append(f'<path d="{svg_path(valid_points, xmap, ymap)}" fill="none" stroke="{colors[name]}" stroke-width="3"/>')
        chunks.append(f'<path d="{svg_path(clamped_points, xmap, ymap)}" fill="none" stroke="{colors[name]}" stroke-width="2" stroke-dasharray="6 5" opacity="0.7"/>')
    chunks.extend(
        [
            f'<line x1="{left}" y1="{top}" x2="{left}" y2="{height-bottom}" stroke="#20252b"/>',
            f'<line x1="{left}" y1="{height-bottom}" x2="{width-right}" y2="{height-bottom}" stroke="#20252b"/>',
            f'<text x="{(left+width-right)/2:.1f}" y="{height-16}" text-anchor="middle" font-family="sans-serif" font-size="14">Vitesse horizontale (m/s)</text>',
            f'<text x="20" y="{(top+height-bottom)/2:.1f}" text-anchor="middle" transform="rotate(-90 20 {(top+height-bottom)/2:.1f})" font-family="sans-serif" font-size="14">Taux de chute (m/s)</text>',
        ]
    )
    for i, name in enumerate(("optimiste", "nominal", "prudent")):
        x = 600 + i * 100
        chunks.append(f'<line x1="{x}" y1="24" x2="{x+25}" y2="24" stroke="{colors[name]}" stroke-width="3"/>')
        chunks.append(f'<text x="{x+30}" y="28" font-family="sans-serif" font-size="12">{name}</text>')
    chunks.append('</svg>')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(chunks), encoding="utf-8")


def write_report(path: Path, all_metrics: dict, area: float) -> None:
    def row(mass: str, name: str, data: dict[str, float]) -> str:
        return (
            f"| {mass} | {name} | {data['max_glide_ratio']:.1f} | "
            f"{data['best_glide_speed_mps']:.2f} | {data['min_sink_mps']:.2f} | "
            f"{data['min_sink_speed_mps']:.2f} |"
        )

    lines = [
        "# Topy V0 — première analyse aérodynamique",
        "",
        "**Statut : hypothèse calculée, non validée en vol.**",
        "",
        "## Résultats",
        "",
        "| Masse | Scénario | Finesse max. | Vitesse de finesse max. (m/s) | Chute mini. (m/s) | Vitesse de chute mini. (m/s) |",
        "|---:|---|---:|---:|---:|---:|",
    ]
    for mass in ("0.320", "0.360"):
        for scenario in SCENARIOS:
            lines.append(row(mass, scenario.name, all_metrics[mass][scenario.name]))
    lines.extend(
        [
            "",
            "### Estimation de la vitesse de décrochage",
            "",
            "La géométrie tridimensionnelle et l'état de surface étant inconnus, `CLmax` est seulement encadré entre 1,05 et 1,25.",
            "",
            "| Masse | avec CLmax 1,05 | avec CLmax 1,25 |",
            "|---:|---:|---:|",
        ]
    )
    for mass in (0.320, 0.360):
        lines.append(f"| {mass:.3f} kg | {stall_speed(mass, area, 1.05):.2f} m/s | {stall_speed(mass, area, 1.25):.2f} m/s |")
    lines.extend(
        [
            "",
            "![Polaire de chute du Topy V0](topy_v0_polar.svg)",
            "",
            "Les traits pleins utilisent directement le domaine Reynolds mesuré. Les traits pointillés emploient la polaire à Reynolds 60 000 comme approximation en dessous de cette limite. Les valeurs de chute minimale du tableau sont limitées aux traits pleins.",
            "",
            "## Méthode",
            "",
            "- dimensions et masses : fiche publiée dans FMT 3/2000 ;",
            "- traînée de profil : mesures UIUC du S4083 (A), interpolées selon `Cl` et le logarithme du Reynolds ;",
            "- traînée induite : `CDi = CL² / (π e AR)` ;",
            "- traînée du fuselage, de l'empennage et des interférences : coefficient forfaitaire explicite ;",
            "- atmosphère : 1,225 kg/m³ et viscosité cinématique 1,5×10⁻⁵ m²/s ;",
            "- calcul en vol plané stabilisé, sans vent.",
            "",
            "| Scénario | Efficacité d'envergure `e` | Traînée supplémentaire `CD` |",
            "|---|---:|---:|",
            "| optimiste | 0,85 | 0,006 |",
            "| nominal | 0,75 | 0,010 |",
            "| prudent | 0,65 | 0,016 |",
            "",
            "## Ce que ce test permet déjà",
            "",
            "La chaîne de données fonctionne avec un profil mesuré au bon ordre de Reynolds. Elle fournit une première plage de vitesses et de performances pour initialiser les essais sans prétendre reproduire exactement l'appareil réel.",
            "",
            "## Limites bloquant la validation",
            "",
            "- le S4083 modifié n'est pas encore confirmé par un relevé de l'aile ;",
            "- la modification fixe du bord de fuite n'est pas incluse ;",
            "- la forme exacte en plan, le dièdre et le vrillage sont inconnus ;",
            "- la traînée du fuselage et de l'empennage est estimée ;",
            "- aucun équilibre de moment, centrage ou réglage de profondeur n'est calculé ;",
            "- cette polaire statique ne prédit pas encore la maniabilité ni la stabilité dynamique.",
            "- la zone de très basse vitesse passe sous Reynolds 60 000 ; elle reste affichée en pointillé mais n'est pas utilisée pour annoncer la chute minimale.",
            "",
            "## Fichiers reproductibles",
            "",
            "- `topy_v0_all_scenarios.csv` : tous les points calculés ;",
            "- `topy_v0_nominal_320g.csv` : courbe nominale principale ;",
            "- `topy_v0_metrics.json` : métriques résumées et hypothèses.",
            "",
            "## Sources",
            "",
            "- [Essai FMT 3/2000](https://rcbookcase.com/data/media/47/03FMT_March_2000.pdf)",
            "- [Données de soufflerie UIUC, volume 2](https://m-selig.ae.illinois.edu/pd/pub/lsat/vol2/)",
            "- [Fiche de conception du S4083](https://m-selig.ae.illinois.edu/ads/ref/s4083_ref.html)",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    reference = json.loads((MODEL_DIR / "reference-data.json").read_text(encoding="utf-8"))
    groups = parse_uiuc_drag(MODEL_DIR / "airfoils" / "S4083A.DRG")
    area = reference["published_geometry_si"]["wing_area_m2"]
    all_rows: list[dict[str, float | bool | str]] = []
    all_metrics: dict[str, dict[str, dict[str, float]]] = {}
    nominal_320: list[dict[str, float | bool | str]] = []
    plot_rows: dict[str, list[dict[str, float | bool | str]]] = {}
    for mass in (0.320, 0.360):
        key = f"{mass:.3f}"
        all_metrics[key] = {}
        for scenario in SCENARIOS:
            rows = analyze(mass, scenario, reference, groups)
            all_rows.extend(rows)
            all_metrics[key][scenario.name] = metrics(rows)
            if mass == 0.320:
                plot_rows[scenario.name] = rows
                if scenario.name == "nominal":
                    nominal_320 = rows
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    write_csv(REPORT_DIR / "topy_v0_all_scenarios.csv", all_rows)
    write_csv(REPORT_DIR / "topy_v0_nominal_320g.csv", nominal_320)
    (REPORT_DIR / "topy_v0_metrics.json").write_text(
        json.dumps(
            {
                "status": "provisional_unvalidated",
                "profile": "S4083(A) clean wind-tunnel data; Topy modification not included",
                "metrics": all_metrics,
                "stall_clmax_range": [1.05, 1.25],
            },
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )
    write_svg(REPORT_DIR / "topy_v0_polar.svg", plot_rows)
    write_report(REPORT_DIR / "TOPY_V0_ANALYSE.md", all_metrics, area)
    nominal = all_metrics["0.320"]["nominal"]
    print(
        "Topy V0 nominal 320 g: "
        f"finesse={nominal['max_glide_ratio']:.1f} à {nominal['best_glide_speed_mps']:.2f} m/s, "
        f"chute mini={nominal['min_sink_mps']:.2f} m/s à {nominal['min_sink_speed_mps']:.2f} m/s"
    )


if __name__ == "__main__":
    main()

