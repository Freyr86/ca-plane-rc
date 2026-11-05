#pragma once

#include "CoreMinimal.h"

class ASlopeWorld;

namespace CirqueWall
{
    constexpr float Setback=200.f;
    constexpr float Height=75.f;
    constexpr float Width=65.f;
    struct FStation
    {
        FVector2D Centre,Edge,Outward;
        float Angle=0,Distance=0;
    };
    struct FPassage
    {
        FVector2D Centre,Outward;
        float Distance=0,Width=300.f;
    };
    // Cached continuous path, in cm. Outward points toward the safe plateau.
    const TArray<FStation>& Path();
    const TArray<FPassage>& Passages();
    bool Footprint(float X,float Y,float ExtraMarginCm=0.f);
    void Build(ASlopeWorld* Site);
}
