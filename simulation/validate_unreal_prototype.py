"""Run the native Topy benchmark headlessly through the normal Unreal launcher."""
from pathlib import Path
import subprocess


if __name__ == "__main__":
    launcher = Path(__file__).with_name("launcher_unreal.ps1")
    raise SystemExit(subprocess.call(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "RemoteSigned",
                                      "-File", str(launcher), "--test-physics"]))
