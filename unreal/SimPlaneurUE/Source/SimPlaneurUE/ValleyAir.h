#pragma once

#include "CoreMinimal.h"

struct FGliderDynamics;

// SI, world north/east/down. All consumers sample the same time and coordinates.
namespace ValleyAir
{
    FVector TerrainFlow(const FVector2D& XY, double HeightAGL, const FVector2D& Breeze);
    FVector Velocity(const FVector& PositionNED, double Seconds, double* RidgeLift = nullptr);
    double SampleGlider(FGliderDynamics& Glider, double Seconds);
    double SurfaceAltitude(double North, double East);

    // Bearing of the origin of a horizontal flow, clockwise from world north.
    double FromBearing(const FVector& FlowNED);
    FVector2D CompassDirection(double Bearing, double CameraYaw);
}
