#pragma once
#include "CoreMinimal.h"

enum class EControlGroup : uint8 { Pilot, Trim, Wing, Motor, Equipment, Simulator, Aux, Count };
enum class EControlType : uint8 { Signed, Range, Trim, Switch, Phase, Pulse };
enum class EGliderControl : uint8
{
    Rudder, Elevator, Aileron, LowRates, TrimRudder, TrimElevator, TrimAileron,
    Flaps, Camber, Spoilers, Butterfly, Phase, LaunchMode, LandingMode, Differential, SnapFlap,
    Motor, MotorCut, Pylon, Gear, WheelBrake, TowRelease, Ballast, Gyro,
    Relaunch, Camera, Instruments, Menu, Meadow, Sidestep, Aux1, Aux2, Aux3, Aux4, WindTrails, HandLaunch, Noclip, Count
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
