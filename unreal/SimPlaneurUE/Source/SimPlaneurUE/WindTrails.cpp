#include "WindTrails.h"
#include "ValleyAir.h"
#include "Components/LineBatchComponent.h"

namespace
{
    FVector UnrealPoint(const FVector& P) { return FVector(P.X, P.Y, -P.Z) * 100.; }
}

void FWindTrails::Clear(ULineBatchComponent& Lines)
{
    if (Trails.IsEmpty()) return;
    Lines.Flush();
    Trails.Reset();
    Random.Reset();
    LastDraw = -1.;
}

void FWindTrails::Seed(FTrail& Trail, const FVector& Focus, double Now, bool Near)
{
    const double Spread = Near ? .22 : Radius/90.*Random.FRandRange(.55f, 1.f);
    FVector Head = Focus + Spread * FVector(Random.FRandRange(-65.f, 65.f), Random.FRandRange(-58.f, 58.f), Random.FRandRange(-23.f, 23.f));
    Head.Z = FMath::Min(Head.Z, -ValleyAir::SurfaceAltitude(Head.X, Head.Y) - Random.FRandRange(2.f, 7.f));
    Trail.Points[0] = Head;
    Trail.HeadWindNED = ValleyAir::Velocity(Head, Now);
    // Reconstruct a short history on birth, including the bend at the edge of lift.
    for (int32 I = 1; I < Knots; ++I)
    {
        const FVector Flow = I == 1 ? Trail.HeadWindNED : ValleyAir::Velocity(Head, Now - (I - 1) * Interval);
        Head -= Interval * Flow;
        Trail.Points[I] = Head;
    }
    Trail.Expires = Now + Random.FRandRange(7.f, 19.f);
}

void FWindTrails::Update(ULineBatchComponent& Lines, const FVector& Aircraft, const FVector& FlightDirection, double Now,const FVector& Eye,double Range)
{
    Radius=Range;
    if (Now - LastDraw < 1. / 30.) return;
    LastDraw = Now;
    // Replenish some parcels in the air the pilot is about to cross. Once seeded
    // each parcel travels with atmospheric velocity alone, independent of the view.
    const FVector Ahead = Aircraft + FlightDirection * 17.;
    constexpr int32 NearCount = 16;
    if (Trails.IsEmpty())
    {
        Trails.SetNum(64);
        for (int32 I = 0; I < Trails.Num(); ++I)
            Seed(Trails[I], I < NearCount ? Ahead : Eye, Now, I < NearCount);
        LastKnot = Now;
    }
    while (Now - LastKnot >= Interval)
    {
        const double NextKnot = LastKnot + Interval;
        for (int32 Index = 0; Index < Trails.Num(); ++Index)
        {
            FTrail& Trail = Trails[Index];
            const bool Near = Index < NearCount;
            const FVector& Focus = Near ? Ahead : Eye;
            const FVector Old = Trail.Points[0];
            const FVector Next = Old + Interval * Trail.HeadWindNED;
            if (LastKnot >= Trail.Expires || FVector::DistSquared(Next, Focus) > FMath::Square(Near ? 22. : Radius) ||
                -Next.Z < ValleyAir::SurfaceAltitude(Next.X, Next.Y) + .5)
            {
                Seed(Trail, Focus, NextKnot, Near);
                continue;
            }
            for (int32 I = Knots - 1; I > 0; --I) Trail.Points[I] = Trail.Points[I - 1];
            Trail.Points[0] = Next;
            Trail.HeadWindNED = ValleyAir::Velocity(Next, NextKnot);
        }
        LastKnot = NextKnot;
    }

    const double Fraction = (Now - LastKnot) / Interval;
    TArray<FBatchedLine, TInlineAllocator<640>> Segments;
    for (const FTrail& Trail : Trails)
    {
        const FVector& Flow = Trail.HeadWindNED;
        FVector Front = UnrealPoint(Trail.Points[0] + Flow * (Now - LastKnot));
        const double LiftTint = FMath::SmoothStep(.15, .85, -Flow.Z);
        const FLinearColor Breeze(.16f, .81f, 1.f), Thermal(1.f, .61f, .12f);
        const FLinearColor Tint = FMath::Lerp(Breeze, Thermal, float(LiftTint));
        for (int32 I = 1; I < Knots; ++I)
        {
            const FVector Back = UnrealPoint(FMath::Lerp(Trail.Points[I], Trail.Points[I - 1], Fraction));
            const float Taper = 1.f - float(I - 1) / (Knots - 1);
            FLinearColor Color = Tint;
            Color.A = .12f + .78f * Taper;
            if (FVector::DistSquared(Back,UnrealPoint(Eye))<=FMath::Square(Radius*100.) && FVector::DistSquared(Front,UnrealPoint(Eye))<=FMath::Square(Radius*100.))
                Segments.Emplace(Back, Front, Color, -1.f, 1.2f + 3.3f * Taper, SDPG_World);
            Front = Back;
        }
    }
    Lines.Flush();
    Lines.DrawLines(Segments);
}
