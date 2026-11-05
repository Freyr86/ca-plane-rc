#pragma once

#include "CoreMinimal.h"
#include "LaunchInstallations.h"
#include "WinchLaunch.h"

// A simplified high-start. Distances in metres, forces in newtons.
// The rubber is selected for the aircraft mass when the installation is laid out.
struct FBungeeLaunch
{
    enum class EState { Packed, LayingOut, Ready, Attached, Towing };
    enum class ERelease { None, Slack, Angle, Overstretch, EndOfLine };
    ELaunchEquipment Type = ELaunchEquipment::Bungee;
    FWinchDrive Winch;
    EState State = EState::Packed;
    FVector Anchor = FVector::ZeroVector; // Unreal centimetres
    FVector Departure = FVector::ZeroVector;
    static constexpr double RestLength = 45.;
    // The departure can be placed while the line is still slack. Tension is
    // adjusted afterwards by walking with the attached glider.
    static constexpr double MinLength = 5.;
    static constexpr double MaxLength = 90.;
    static constexpr double ReleaseAngleDegrees = 75.;
    double Stiffness = 1.;

    bool IsWinch() const { return Type==ELaunchEquipment::Winch; }
    double MinimumLength() const { return IsWinch()?FWinchDrive::MinLayout:MinLength; }
    double MaximumLength() const { return IsWinch()?FWinchDrive::MaxLayout:MaxLength; }

    bool Installed() const { return State != EState::Packed; }
    bool Connected() const { return State == EState::Attached || State == EState::Towing; }
    void PlaceAnchor(const FVector& Position) { Anchor = Position; State = EState::LayingOut; }
    bool SetDeparture(const FVector& Position, double MassKg)
    {
        const double Length = FVector::Distance(Position, Anchor) * .01;
        if (State != EState::LayingOut || Length < MinimumLength() || Length > MaximumLength() || MassKg <= 0.) return false;
        Departure = Position;
        Stiffness = MassKg * 9.80665 * 4. / 25.;
        State = EState::Ready;
        return true;
    }
    double ScaleNewtons() const { return IsWinch()?Winch.MaximumForce()*1.35:Stiffness * (MaxLength - RestLength); }
    void BeginTow(const FVector& PositionCm, double MassKg)
    {
        if(IsWinch()) Winch.Start(FVector::Distance(PositionCm,Anchor)*.01,MassKg);
        State=EState::Towing;
    }
    void AdvanceTow(double Dt, const FVector& PositionCm, const FVector& VelocityMps, double Power)
    {
        if(!IsWinch() || State!=EState::Towing) return;
        const FVector Away=(PositionCm-Anchor)*.01;
        Winch.Step(Dt,Away.Size(),FVector::DotProduct(VelocityMps,Away.GetSafeNormal()),Power);
    }
    ERelease ReleaseCondition(const FVector& PositionCm) const
    {
        if (State != EState::Towing) return ERelease::None;
        const FVector Line = (PositionCm-Anchor)*.01;
        if (Line.Size() > MaximumLength()*1.25) return ERelease::Overstretch;
        if (IsWinch())
        {
            if(Winch.Finished()) return ERelease::EndOfLine;
            if(Winch.Slack()) return ERelease::Slack;
        }
        else if (Line.Size() <= RestLength+.1) return ERelease::Slack;
        const double Elevation = FMath::RadiansToDegrees(FMath::Atan2(Line.Z,Line.Size2D()));
        return Elevation >= ReleaseAngleDegrees ? ERelease::Angle : ERelease::None;
    }
    FVector ForceWorld(const FVector& PositionCm, const FVector& VelocityMps) const
    {
        const FVector Away = (PositionCm - Anchor) * .01;
        if(IsWinch()) return State==EState::Towing ? -Away.GetSafeNormal()*Winch.Tension : FVector::ZeroVector;
        const double Extension = Away.Size() - RestLength;
        if (Extension <= 0.) return FVector::ZeroVector;
        const FVector Direction = Away.GetSafeNormal();
        const double ExtensionRate = FVector::DotProduct(VelocityMps, Direction);
        const double Tension = FMath::Clamp(Stiffness * (Extension + .12 * ExtensionRate), 0., ScaleNewtons() * 1.5);
        return -Direction * Tension;
    }
    void Release() { if (Connected()) State = EState::Ready; }
};
