"""Compatibility entry point for tools that used the Python launcher."""
from pathlib import Path
import subprocess
import sys


def main():
    launcher = Path(__file__).with_suffix(".ps1")
    return subprocess.call(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "RemoteSigned",
                            "-File", str(launcher), *sys.argv[1:]])


if __name__ == "__main__":
    raise SystemExit(main())
