import importlib.util
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("server_topy_test", ROOT / "simulation" / "server.py")
server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(server)


def test_topy_trimmed_glide_matches_static_polar_order_of_magnitude():
    sim = server.Simulator("topy_4522")
    initial_altitude = sim.state()["alt"]
    samples = []
    for index in range(20 * 120):
        state = sim.step()
        assert all(math.isfinite(value) for key, value in state.items() if key not in ("grounded", "model"))
        if index >= 10 * 120:
            samples.append(state)
    average_speed = sum(sample["speed"] for sample in samples) / len(samples)
    average_sink = sum(sample["sink"] for sample in samples) / len(samples)
    assert 6.5 < average_speed < 7.5
    assert 0.45 < average_sink < 0.75
    assert sim.state()["alt"] < initial_altitude - 9.0


def test_topy_vtail_mixes_pitch_and_yaw_independently():
    sim = server.Simulator("topy_4522")
    for _ in range(60):
        sim.step(elevator=0.25)
    fdm = sim.fdm
    assert abs(fdm["fcs/vtail-left-pos-norm"] - fdm["fcs/vtail-right-pos-norm"]) < 1e-9
    assert fdm["fcs/vtail-pitch-pos-norm"] > 0.2
    assert abs(fdm["fcs/vtail-yaw-pos-norm"]) < 1e-9

    sim.reset()
    for _ in range(60):
        sim.step(rudder=0.25)
    fdm = sim.fdm
    assert fdm["fcs/vtail-left-pos-norm"] > 0.2
    assert fdm["fcs/vtail-right-pos-norm"] < -0.2
    assert abs(fdm["fcs/vtail-pitch-pos-norm"]) < 1e-9
    assert fdm["fcs/vtail-yaw-pos-norm"] > 0.2


def test_topy_rudder_creates_coordinated_yaw_and_dihedral_roll():
    sim = server.Simulator("topy_4522")
    for _ in range(2 * 120):
        state = sim.step(rudder=0.25)
    assert abs(state["yaw"]) > math.radians(5)
    assert abs(state["roll"]) > math.radians(5)
    assert abs(state["roll"]) < math.radians(40)


def test_topy_replay_is_deterministic():
    first, second = server.Simulator("topy_4522"), server.Simulator("topy_4522")
    for _ in range(240):
        a = first.step(elevator=-0.08, rudder=0.12)
        b = second.step(elevator=-0.08, rudder=0.12)
    assert a == b


def test_scene_contact_below_sea_level_is_latched_until_a_numbered_reset():
    sim = server.Simulator(scene_contacts=True)
    flight = sim.flight_id
    impact = dict(kind="water", north=-470., east=330., alt=-29.95, roll=.02, pitch=-.11, yaw=2.1)
    sim.scene_command(dict(flight_id=flight, contact=impact))
    for _ in range(3):
        state = sim.step(elevator=.8)
        assert (state["contact"],state["grounded"],state["speed"],state["sink"]) == ("water",True,0.,0.)
        assert (state["north"],state["east"],state["alt"]) == (-470.,330.,-29.95)
    sim.scene_command(dict(flight_id=flight,reset=True))
    sim.scene_command(dict(flight_id=flight,contact=impact))
    sim.scene_command(dict(flight_id=flight,reset=True))
    state = sim.step()
    assert sim.flight_id == flight+1 and not state["grounded"] and "contact" not in state
    assert state["alt"]>79 and state["speed"]>5
