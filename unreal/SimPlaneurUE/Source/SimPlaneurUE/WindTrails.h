#pragma once

#include "CoreMinimal.h"

class ULineBatchComponent;

class FWindTrails
{
public:
    void Update(ULineBatchComponent& Lines, const FVector& AircraftNED, const FVector& FlightDirection, double Now,const FVector& Eye,double Range);
    void Clear(ULineBatchComponent& Lines);

private:
    static constexpr int32 Knots = 11;
    static constexpr double Interval = .23;
    struct FTrail
    {
        FVector Points[Knots]; // newest first, world north/east/down in metres
        FVector HeadWindNED; // At Points[0] and LastKnot, in m/s.
        double Expires;
    };
    TArray<FTrail> Trails;
    FRandomStream Random{4522};
    double Radius=90.;
    double LastKnot = 0.;
    double LastDraw = -1.;
    void Seed(FTrail& Trail, const FVector& Focus, double Now, bool Near);
};
