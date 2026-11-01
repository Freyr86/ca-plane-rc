import importlib.util
from pathlib import Path
import math

spec=importlib.util.spec_from_file_location('server',Path(__file__).resolve().parents[1]/'simulation/server.py')
server=importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)

def test_free_flight_finite_and_descending():
    sim=server.Simulator('rc_demo')
    for _ in range(1200):
        state=sim.step()
        assert all(math.isfinite(v) for v in state.values() if isinstance(v, (int, float)))
    assert 0 < state['alt'] < 80
    assert 5 < state['speed'] < 40
    assert state['north'] > 20

def test_reset_and_roll_control():
    sim=server.Simulator('rc_demo')
    for _ in range(60):
        state=sim.step(aileron=.3)
    assert state['roll'] > .01
    sim.reset()
    assert abs(sim.state()['alt']-80) < .01
    assert sim.state()['north'] == 0

def test_replay_is_deterministic():
    a,b=server.Simulator('rc_demo'),server.Simulator('rc_demo')
    for _ in range(120):
        sa=a.step(elevator=-.05)
        sb=b.step(elevator=-.05)
    assert sa == sb
