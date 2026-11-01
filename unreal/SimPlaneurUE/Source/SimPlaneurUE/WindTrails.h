#pragma once

#include "CoreMinimal.h"

class ULineBatchComponent;

class FWindTrails
{
public:
    void Update(ULineBatchComponent& Lines, const FVector& AircraftNED, const FVector& FlightDirection, double Now);
    void Clear(ULineBatchComponent& Lines);

private:
    static constexpr int32 Knots = 11;
    static constexpr double Interval = .23;
    struct FTrail
    {
        FVector Points[Knots]; // newest first, world north/east/down in metres
        double Expires;
    };
    TArray<FTrail> Trails;
    FRandomStream Random{4522};
    double LastKnot = 0.;
    double LastDraw = -1.;
    void Seed(FTrail& Trail, const FVector& Focus, double Now, bool Near);
};
