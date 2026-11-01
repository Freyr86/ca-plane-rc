"""List legacy Windows joystick devices and print their current axis values."""

from __future__ import annotations

import ctypes
from ctypes import wintypes
import time


MAXPNAMELEN = 32
MAX_JOYSTICKOEMVXDNAME = 260
JOYERR_NOERROR = 0
JOY_RETURNALL = 0x000000FF


class JOYCAPSW(ctypes.Structure):
    _fields_ = [
        ("wMid", wintypes.WORD),
        ("wPid", wintypes.WORD),
        ("szPname", wintypes.WCHAR * MAXPNAMELEN),
        ("wXmin", wintypes.UINT), ("wXmax", wintypes.UINT),
        ("wYmin", wintypes.UINT), ("wYmax", wintypes.UINT),
        ("wZmin", wintypes.UINT), ("wZmax", wintypes.UINT),
        ("wNumButtons", wintypes.UINT),
        ("wPeriodMin", wintypes.UINT), ("wPeriodMax", wintypes.UINT),
        ("wRmin", wintypes.UINT), ("wRmax", wintypes.UINT),
        ("wUmin", wintypes.UINT), ("wUmax", wintypes.UINT),
        ("wVmin", wintypes.UINT), ("wVmax", wintypes.UINT),
        ("wCaps", wintypes.UINT),
        ("wMaxAxes", wintypes.UINT), ("wNumAxes", wintypes.UINT),
        ("wMaxButtons", wintypes.UINT),
        ("szRegKey", wintypes.WCHAR * MAXPNAMELEN),
        ("szOEMVxD", wintypes.WCHAR * MAX_JOYSTICKOEMVXDNAME),
    ]


class JOYINFOEX(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
        ("dwXpos", wintypes.DWORD), ("dwYpos", wintypes.DWORD),
        ("dwZpos", wintypes.DWORD), ("dwRpos", wintypes.DWORD),
        ("dwUpos", wintypes.DWORD), ("dwVpos", wintypes.DWORD),
        ("dwButtons", wintypes.DWORD), ("dwButtonNumber", wintypes.DWORD),
        ("dwPOV", wintypes.DWORD), ("dwReserved1", wintypes.DWORD),
        ("dwReserved2", wintypes.DWORD),
    ]


winmm = ctypes.WinDLL("winmm")
devices = []
for device_id in range(winmm.joyGetNumDevs()):
    caps = JOYCAPSW()
    if winmm.joyGetDevCapsW(device_id, ctypes.byref(caps), ctypes.sizeof(caps)) != JOYERR_NOERROR:
        continue
    state = JOYINFOEX()
    state.dwSize = ctypes.sizeof(state)
    state.dwFlags = JOY_RETURNALL
    result = winmm.joyGetPosEx(device_id, ctypes.byref(state))
    axes = [state.dwXpos, state.dwYpos, state.dwZpos, state.dwRpos, state.dwUpos, state.dwVpos]
    devices.append((device_id, caps, axes[:], axes[:]))
    print(f"id={device_id} name={caps.szPname!r} axes={caps.wNumAxes} buttons={caps.wNumButtons} result={result}")
    print("  X Y Z R U V =", " ".join(f"{value:5d}" for value in axes))

if devices:
    print("Mesure pendant 10 secondes : bougez les manches et interrupteurs...")
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        for device_id, _caps, minima, maxima in devices:
            state = JOYINFOEX()
            state.dwSize = ctypes.sizeof(state)
            state.dwFlags = JOY_RETURNALL
            if winmm.joyGetPosEx(device_id, ctypes.byref(state)) == JOYERR_NOERROR:
                axes = [state.dwXpos, state.dwYpos, state.dwZpos, state.dwRpos, state.dwUpos, state.dwVpos]
                for index, value in enumerate(axes):
                    minima[index] = min(minima[index], value)
                    maxima[index] = max(maxima[index], value)
        time.sleep(0.01)
    for device_id, caps, minima, maxima in devices:
        ranges = " ".join(f"{low:5d}..{high:5d}" for low, high in zip(minima, maxima))
        print(f"id={device_id} ({caps.wNumButtons} boutons) plages X Y Z R U V = {ranges}")
