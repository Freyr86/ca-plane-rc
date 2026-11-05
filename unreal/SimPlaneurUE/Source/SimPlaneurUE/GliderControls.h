#pragma once
#include "CoreMinimal.h"

enum class EControlGroup : uint8 { Pilot, Trim, Wing, Motor, Equipment, Simulator, Aux, Count };
enum class EControlType : uint8 { Signed, Range, CenteredRange, Trim, Switch, Pulse };
enum class EGliderControl : uint8
{
    Rudder, Elevator, Aileron, LowRates, TrimRudder, TrimElevator, TrimAileron,
    Flaps, Spoilers, Butterfly,
    Motor, MotorCut, Pylon, Gear, WheelBrake, TowRelease, Ballast,
    Relaunch, Camera, Instruments, Menu, Meadow, Sidestep, Aux1, Aux2, Aux3, Aux4, WindTrails, HandLaunch, Noclip, Vario, Backpack, Equipment, Winch, WinchPower, Count
};
struct FControlSpec
{
    const TCHAR* Id;
    const TCHAR* Label;
    EControlGroup Group;
    EControlType Type;
    bool OnTopy;
    const TCHAR* KeyPlus = nullptr;
    const TCHAR* KeyMinus = nullptr;
};
const FControlSpec& ControlSpec(EGliderControl Action);
const TCHAR* GroupLabel(EControlGroup Group);
constexpr int32 ControlCount = int32(EGliderControl::Count);
