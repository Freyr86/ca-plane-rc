#pragma once
#include "CoreMinimal.h"

// Prototype thresholds in m/s normal to the struck surface, not total airspeed.
inline int32 LandingDamage(double NormalSpeed,bool Water=false,bool Obstacle=false,bool Inverted=false)
{
    if(Water || NormalSpeed>=6.) return 2;
    return NormalSpeed>=2.5 || (Obstacle && NormalSpeed>=1.5) || Inverted ? 1 : 0;
}
