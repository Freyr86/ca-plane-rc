#pragma once

#include "CoreMinimal.h"

// The cirque is a local deformation of Valley::Height, in Unreal centimetres.
// X points north and Y east. Its open combe faces southwest, bearing 240 degrees.
namespace Cirque
{
    constexpr float CenterX = 268000.f;
    constexpr float CenterY = 160000.f;
    constexpr float RimRadius = 65000.f;
    constexpr float TransitionRadius = 110000.f;
    constexpr float BackAxisX = .5f;
    constexpr float BackAxisY = .8660254038f;

    struct FSample
    {
        float Influence = 0.f;
        float Cliff = 0.f;
        float Scree = 0.f;
        float Meadow = 0.f;
    };

    // Material/vegetation masks follow the same contours as the physical ground.
    // All are [0,1], zero outside the local deformation; Meadow excludes the rock.
    FSample Sample(float X, float Y);
    float ApplyHeight(float X, float Y, float BaseHeight);
    // Upper edge of the actual cliff, before its smooth shoulder becomes pasture.
    // Angle is relative to BackAxis, in radians; return value is world XY in cm.
    FVector2D RimEdge(float Angle);
}
