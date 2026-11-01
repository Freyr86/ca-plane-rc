"""Regenerate offline JSBSim checkpoints for the native Unreal flight test."""
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from server import Simulator, jsbsim


PROFILES = {
    "glide": [(20, 0, 0)],
    "turn": [(3, 0, 0), (2, 0, -0.35), (4, 0, 0)],
    "elevator": [(3, 0, 0), (1, -0.45, 0), (4, 0, 0)],
    "loop": [(4, 0.85, 0), (4, -1, 0)],
}


def main():
    result = {"jsbsim": jsbsim.__version__, "hz": 120, "profiles": []}
    for name, commands in PROFILES.items():
        sim = Simulator(breeze=True, scene_contacts=True)
        segments = [[seconds * 120, elevator, rudder] for seconds, elevator, rudder in commands]
        samples = []
        tick = 0
        for duration, elevator, rudder in segments:
            for _ in range(duration):
                state = sim.step(elevator=elevator, rudder=rudder)
                tick += 1
                if tick % 30 == 0:
                    pose = [state[k] for k in ("north", "east", "alt")]
                    angles = [math.degrees(state[k]) for k in ("pitch", "yaw", "roll")]
                    samples.append([tick] + [round(x, 7) for x in (*pose, *angles, state["speed"], state["sink"])])
        result["profiles"].append({"name": name, "segments": segments, "samples": samples})
    folder = Path(__file__).resolve().parents[1] / "unreal/SimPlaneurUE/SourceAssets/Tests"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "TopyFlightReference.json").write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
