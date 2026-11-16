#pragma once
#include "CoreMinimal.h"

enum class EFirstFlightStep : uint8 { Inactive, Walk, Windsock, Airflow, Controls, Launch, Fly, Landing, Retrieve, Complete };
enum class EBungeeTutorialStep : uint8 { Inactive, Select, Anchor, Layout, Attach, Tension, Launch, Tow, Glide, Retrieve, Pack, Complete };

struct FFirstFlightTutorial
{
    EFirstFlightStep Step=EFirstFlightStep::Inactive;
    EBungeeTutorialStep BungeeStep=EBungeeTutorialStep::Inactive;
    FVector BungeeAnchorTarget=FVector::ZeroVector, BungeeDepartureTarget=FVector::ZeroVector;
    float FlightSeconds=0, StageSeconds=0, LostSeconds=0;
    bool bRecoveryNeeded=false, bCrashed=false, bRoughLanding=false;
    bool RudderLeft=false, RudderRight=false, ElevatorUp=false, ElevatorDown=false;
    float Rudder=0, Elevator=0;
    FVector WalkTarget=FVector::ZeroVector, WindsockTarget=FVector::ZeroVector;
    FVector LaunchPoint=FVector::ZeroVector, LandingTarget=FVector::ZeroVector;
    bool BungeeActive() const { return BungeeStep!=EBungeeTutorialStep::Inactive; }
    bool Active() const { return Step!=EFirstFlightStep::Inactive || BungeeActive(); }
    void Go(EBungeeTutorialStep Next) { BungeeStep=Next; StageSeconds=LostSeconds=0; }
    void Go(EFirstFlightStep Next) { Step=Next; StageSeconds=LostSeconds=0; }
    void ObserveControls(float Direction,float Pitch)
    {
        Rudder=Direction; Elevator=Pitch;
        RudderLeft|=Direction<-.6f; RudderRight|=Direction>.6f;
        // Match the flight model: positive elevator is nose-down.
        ElevatorUp|=Pitch<-.6f; ElevatorDown|=Pitch>.6f;
    }
    bool ControlsReady() const { return RudderLeft && RudderRight && ElevatorUp && ElevatorDown && FMath::Abs(Rudder)<.2f && FMath::Abs(Elevator)<.2f; }
};
