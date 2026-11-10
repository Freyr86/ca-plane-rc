#pragma once
#include "CoreMinimal.h"

enum class EAircraftResistance : uint8 { Realistic, Resistant, Indestructible };

// Unreal velocities/offsets are cm/s and cm; return the closing speed in m/s.
// Sliding along a surface or separating from it must not count as an impact.
inline double LandingContactSpeed(const FVector& Velocity,const FVector& AngularVelocity,
    const FVector& ContactOffset,const FVector& SurfaceNormal)
{
    const FVector AtContact=Velocity+FVector::CrossProduct(AngularVelocity,ContactOffset);
    return FMath::Max(0.,-FVector::DotProduct(AtContact,SurfaceNormal.GetSafeNormal()))*.01;
}

// Prototype thresholds in m/s normal to the struck surface, not total airspeed.
inline int32 LandingDamage(double NormalSpeed,bool Water=false,bool Obstacle=false,bool Inverted=false,
    EAircraftResistance Resistance=EAircraftResistance::Realistic)
{
    if (Resistance==EAircraftResistance::Indestructible) return 0;
    // Training mode doubles tolerated approach speeds (four times the energy).
    if (Resistance==EAircraftResistance::Resistant) NormalSpeed *= .5;
    if(Water || NormalSpeed>=6.) return 2;
    return NormalSpeed>=2.5 || (Obstacle && NormalSpeed>=1.5) || Inverted ? 1 : 0;
}
