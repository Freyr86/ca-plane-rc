import json
from pathlib import Path
import socket
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
UNREAL = ROOT / "unreal" / "SimPlaneurUE"


def test_unreal_project_declares_runtime_and_required_plugins():
    project = json.loads((UNREAL / "SimPlaneurUE.uproject").read_text(encoding="utf-8"))
    assert project["Modules"][0]["Name"] == "SimPlaneurUE"
    enabled = {plugin["Name"] for plugin in project["Plugins"] if plugin["Enabled"]}
    assert {"ProceduralMeshComponent", "PythonScriptPlugin", "EditorScriptingUtilities"} <= enabled


def test_unreal_obj_has_expected_centimetre_dimensions_and_axes():
    vertices = []
    path = UNREAL / "SourceAssets" / "Topy4522V0_Unreal.obj"
    for line in path.read_text(encoding="ascii").splitlines():
        if line.startswith("v "):
            vertices.append(tuple(float(value) for value in line.split()[1:]))
    assert vertices
    extents = [max(row[axis] for row in vertices) - min(row[axis] for row in vertices) for axis in range(3)]
    assert 84.0 < extents[0] < 88.0  # length, forward X
    assert 118.0 < extents[1] < 120.0  # span, right Y
    assert 10.0 < extents[2] < 25.0


def test_unreal_style_udp_roundtrip_with_jsbsim_server():
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()
    process = subprocess.Popen(
        [sys.executable, str(ROOT / "simulation" / "server.py"), "--port", str(port), "--model", "topy_4522"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.settimeout(0.15)
    payload = json.dumps({"aileron": 0.0, "elevator": 0.1, "rudder": -0.1}).encode()
    state = None
    try:
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline and state is None:
            client.sendto(payload, ("127.0.0.1", port))
            try:
                data, _ = client.recvfrom(4096)
                state = json.loads(data)
            except (TimeoutError, ConnectionResetError):
                pass
        assert state is not None
        assert state["model"] == "topy_4522"
        assert all(key in state for key in ("north", "east", "alt", "roll", "pitch", "yaw", "speed", "sink"))
    finally:
        client.close()
        process.terminate()
        process.wait(timeout=5)
