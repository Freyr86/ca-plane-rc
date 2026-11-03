#pragma once

#include "CoreMinimal.h"

// Unreal centimetres. Local X points along the runway to the SE, Y to the SW.
namespace Airfield
{
    inline const FVector2D Centre(31500.,-138200.);
    constexpr double Diagonal=.7071067811865475;
    constexpr double HalfLength=7500., HalfWidth=900.;
    constexpr double HutSide=HalfWidth+1000.;
    inline FVector2D Point(double Along,double Side)
    {
        return Centre+FVector2D(-Along-Side,Along-Side)*Diagonal;
    }
    inline FVector2D Local(double X,double Y)
    {
        const double North=X-Centre.X, East=Y-Centre.Y;
        return FVector2D(East-North,-East-North)*Diagonal;
    }
    inline const FVector2D Hut=Point(0,HutSide), Sock=Point(450,HutSide), Pilot=Point(-550,HalfWidth+250.);
}
