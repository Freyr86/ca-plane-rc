#pragma once
#include "CoreMinimal.h"
class UWorld;
class AActor;

namespace PilotMovement
{
    constexpr float WalkSpeed=550.f, RunSpeed=1625.f, FlightSpeed=WalkSpeed*.8f; // cm/s
    constexpr double MaxStep=55.;
    bool ResolveStep(UWorld* World,const AActor* Pilot,const FVector& Start,const FVector& Ground,FVector& Result);
    inline float Speed(bool InFlight,bool Sprint) {return InFlight?FlightSpeed:Sprint?RunSpeed:WalkSpeed;}
    inline bool IsWalkingKey(FName Key)
    {
        for(const TCHAR* Name:{TEXT("W"),TEXT("Z"),TEXT("S"),TEXT("A"),TEXT("Q"),TEXT("D"),
            TEXT("Up"),TEXT("Down"),TEXT("Left"),TEXT("Right")})
            if(Key==FName(Name)) return true;
        return false;
    }
}
