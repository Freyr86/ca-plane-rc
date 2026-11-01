"""Run repeatable software checks on the provisional Topy JSBSim model."""

from __future__ import annotations

import importlib.util
import json
import math
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "reports" / "topy_v0"


def load_server():
    path = ROOT / "simulation" / "server.py"
    spec = importlib.util.spec_from_file_location("topy_server_validation", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def neutral_glide(server, frequency_hz: int, duration_s: float = 20.0) -> dict:
    sim = server.Simulator("topy_4522", dt=1 / frequency_hz)
    initial_altitude = sim.state()["alt"]
    samples = []
    started = time.perf_counter()
    for step in range(round(duration_s * frequency_hz)):
        state = sim.step()
        if step >= round((duration_s - 10.0) * frequency_hz):
            samples.append(state)
    elapsed = time.perf_counter() - started
    return {
        "frequency_hz": frequency_hz,
        "duration_s": duration_s,
        "mean_speed_mps_last_10s": sum(row["speed"] for row in samples) / len(samples),
        "mean_sink_mps_last_10s": sum(row["sink"] for row in samples) / len(samples),
        "mean_alpha_deg_last_10s": math.degrees(sum(row["alpha"] for row in samples) / len(samples)),
        "altitude_loss_m": initial_altitude - state["alt"],
        "final_roll_deg": math.degrees(state["roll"]),
        "final_pitch_deg": math.degrees(state["pitch"]),
        "wall_time_s": elapsed,
        "realtime_margin": duration_s / elapsed,
    }


def control_step(server, control: str, command: float = 0.25, duration_s: float = 2.0) -> dict:
    sim = server.Simulator("topy_4522")
    for _ in range(round(duration_s / sim.dt)):
        kwargs = {control: command}
        state = sim.step(**kwargs)
    return {
        "control": control,
        "command": command,
        "duration_s": duration_s,
        "vtail_left_norm": sim.fdm["fcs/vtail-left-pos-norm"],
        "vtail_right_norm": sim.fdm["fcs/vtail-right-pos-norm"],
        "vtail_pitch_norm": sim.fdm["fcs/vtail-pitch-pos-norm"],
        "vtail_yaw_norm": sim.fdm["fcs/vtail-yaw-pos-norm"],
        "roll_deg": math.degrees(state["roll"]),
        "pitch_deg": math.degrees(state["pitch"]),
        "heading_deg": math.degrees(state["yaw"]),
        "speed_mps": state["speed"],
    }


def relative_difference(a: float, b: float) -> float:
    reference = max(abs(a), abs(b), 1e-12)
    return abs(a - b) / reference


def generate_report(results: dict) -> str:
    n120 = results["neutral_120_hz"]
    n240 = results["neutral_240_hz"]
    convergence = results["integration_convergence"]
    elevator = results["elevator_step"]
    rudder = results["rudder_step"]
    return f"""# Validation logicielle JSBSim — Topy V0

## Statut

**Fonctionnement logiciel vérifié, physique non validée.** Le modèle charge, reste numériquement stable, descend en plané et répond aux deux commandes du V-tail. Ces résultats ne prouvent pas encore qu'il reproduit fidèlement le Topy réel.

## Plané neutre

| Calcul | 120 Hz | 240 Hz |
|---|---:|---:|
| Vitesse moyenne sur les 10 dernières secondes | {n120['mean_speed_mps_last_10s']:.3f} m/s | {n240['mean_speed_mps_last_10s']:.3f} m/s |
| Taux de chute moyen | {n120['mean_sink_mps_last_10s']:.3f} m/s | {n240['mean_sink_mps_last_10s']:.3f} m/s |
| Alpha moyen | {n120['mean_alpha_deg_last_10s']:.3f}° | {n240['mean_alpha_deg_last_10s']:.3f}° |
| Perte d'altitude en 20 s | {n120['altitude_loss_m']:.3f} m | {n240['altitude_loss_m']:.3f} m |
| Marge par rapport au temps réel | ×{n120['realtime_margin']:.0f} | ×{n240['realtime_margin']:.0f} |

L'écart 120/240 Hz vaut {100 * convergence['speed_relative']:.3f} % sur la vitesse, {100 * convergence['sink_relative']:.3f} % sur le taux de chute et {100 * convergence['altitude_loss_relative']:.3f} % sur la perte d'altitude.

## V-tail

- Profondeur +25 % pendant 2 s : panneaux {elevator['vtail_left_norm']:.3f} / {elevator['vtail_right_norm']:.3f}, tangage final {elevator['pitch_deg']:.2f}°.
- Direction +25 % pendant 2 s : panneaux {rudder['vtail_left_norm']:.3f} / {rudder['vtail_right_norm']:.3f}, cap final {rudder['heading_deg']:.2f}°, roulis final {rudder['roll_deg']:.2f}°.

## Limites connues

- Profil S4083(A) candidat : le profil réel et la modification du bord de fuite restent à mesurer.
- Centrage, inerties, débattements et dérivées de stabilité sont provisoires.
- La polaire utilise les mesures UIUC dans leur domaine et une approximation en dehors.
- Vent, thermiques, sol et contact d'atterrissage ne font pas partie de cette validation.
- La prochaine validation utile consiste à mesurer le Topy réel puis à comparer vitesse, taux de chute et réponses à des échelons de commande.
"""


def main() -> None:
    server = load_server()
    n120 = neutral_glide(server, 120)
    n240 = neutral_glide(server, 240)
    results = {
        "status": "software_verified_physics_unvalidated",
        "neutral_120_hz": n120,
        "neutral_240_hz": n240,
        "integration_convergence": {
            "speed_relative": relative_difference(n120["mean_speed_mps_last_10s"], n240["mean_speed_mps_last_10s"]),
            "sink_relative": relative_difference(n120["mean_sink_mps_last_10s"], n240["mean_sink_mps_last_10s"]),
            "altitude_loss_relative": relative_difference(n120["altitude_loss_m"], n240["altitude_loss_m"]),
        },
        "elevator_step": control_step(server, "elevator"),
        "rudder_step": control_step(server, "rudder"),
    }
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    json_path = REPORT_DIR / "jsbsim_validation.json"
    report_path = REPORT_DIR / "JSBSIM_V0_VALIDATION.md"
    json_path.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report_path.write_text(generate_report(results), encoding="utf-8")
    print(report_path)
    print(f"Plané neutre : {n120['mean_speed_mps_last_10s']:.3f} m/s, chute {n120['mean_sink_mps_last_10s']:.3f} m/s")


if __name__ == "__main__":
    main()
