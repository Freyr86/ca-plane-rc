#pragma once

#include "CoreMinimal.h"

namespace Valley
{
    constexpr float WaterLevel = -3000.f;
    constexpr float LakeX = -47000.f;
    constexpr float LakeY = 33000.f;

    // Distances and heights use Unreal centimetres. The shoreline is the zero contour.
    float ShoreDistance(float X, float Y);
    float Height(float X, float Y);
    float Woodland(float X, float Y);
}
