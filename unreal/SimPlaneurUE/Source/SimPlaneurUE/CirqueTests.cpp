#include "CirqueLandscape.h"
#include "ValleyLandscape.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCirqueLandscapeTest, "SimPlaneur.World.CirqueLandscape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCirqueLandscapeTest::RunTest(const FString&)
{
    const FVector2D Centre(Cirque::CenterX, Cirque::CenterY);
    const FVector2D Back(Cirque::BackAxisX, Cirque::BackAxisY);
    const FVector2D Crown = Centre + Back * 72000.;
    const FVector2D Mouth = Centre - Back * 72000.;
    const float Floor = Valley::Height(Centre.X, Centre.Y);
    TestTrue(TEXT("Cirque floor is between 100 and 130 metres"), Floor >= 10000.f && Floor <= 13000.f);
    TestTrue(TEXT("Crown remains between 280 and 330 metres"),
        Valley::Height(Crown.X, Crown.Y) >= 28000.f && Valley::Height(Crown.X, Crown.Y) <= 33000.f);
    TestEqual(TEXT("Northeast crown probe is beyond its local rock face"),
        Cirque::Sample(Crown.X, Crown.Y).Cliff, 0.f);
    TestTrue(TEXT("Southwest mouth has no enclosing sill"), Valley::Height(Mouth.X, Mouth.Y) < 13000.f);
    TestEqual(TEXT("Southwest mouth has no cliff mask"), Cirque::Sample(Mouth.X, Mouth.Y).Cliff, 0.f);

    bool ExteriorUnchanged = true, SmoothBoundary = true, MasksBounded = true;
    for (int32 I = 0; I < 72; ++I)
    {
        const float A = I * 2.f * PI / 72.f;
        const FVector2D Direction(FMath::Cos(A), FMath::Sin(A));
        for (float R : {Cirque::TransitionRadius, Cirque::TransitionRadius + 20000.f})
        {
            const FVector2D P = Centre + Direction * R;
            ExteriorUnchanged &= Valley::Height(P.X, P.Y) == Valley::NaturalHeight(P.X, P.Y);
            ExteriorUnchanged &= Cirque::Sample(P.X, P.Y).Influence == 0.f;
        }
        // Compare the modification itself on both sides, independently of the
        // underlying mountain slope: no lip or gradient survives at the join.
        for (float Offset : {-100.f, 0.f, 100.f})
        {
            const FVector2D P = Centre + Direction * (Cirque::TransitionRadius + Offset);
            SmoothBoundary &= Valley::Height(P.X, P.Y) == Valley::NaturalHeight(P.X, P.Y);
        }
        for (float R = 0.f; R < Cirque::TransitionRadius; R += 1000.f)
        {
            const FVector2D P = Centre + Direction * R;
            const auto S = Cirque::Sample(P.X, P.Y);
            for (float M : {S.Influence, S.Cliff, S.Scree, S.Meadow})
                MasksBounded &= FMath::IsFinite(M) && M >= 0.f && M <= 1.f;
            MasksBounded &= S.Cliff <= S.Influence && S.Scree <= S.Influence && S.Meadow <= S.Influence;
        }
    }
    TestTrue(TEXT("All terrain outside the patch remains exactly unchanged"), ExteriorUnchanged);
    TestTrue(TEXT("Patch boundary preserves the original slope without a seam"), SmoothBoundary);
    TestTrue(TEXT("Rock and meadow masks are finite and bounded by patch influence"), MasksBounded);

    float SteepestSlope = 0.f, CliffFoot = 0.f, CliffTop = 0.f;
    for (float R = 50000.f; R < 74000.f; R += 100.f)
    {
        const FVector2D P = Centre + Back * R;
        const float H = Valley::Height(P.X, P.Y);
        const FVector2D Next = P + Back * 100.;
        SteepestSlope = FMath::Max(SteepestSlope, (Valley::Height(Next.X, Next.Y) - H) / 100.f);
        if (Cirque::Sample(P.X, P.Y).Scree > .8f) CliffFoot = FMath::Max(CliffFoot, H);
        if (R >= 71000.f) CliffTop = FMath::Max(CliffTop, H);
    }
    TestTrue(TEXT("Rock face is steep enough to read as a cliff"), SteepestSlope > 4.f);
    TestTrue(TEXT("Exposed cliff above the talus is between 140 and 210 metres"),
        CliffTop - CliffFoot > 14000.f && CliffTop - CliffFoot < 21000.f);

    // Guard the broad visual structure: a constant-radius wall and constant-width
    // talus would turn the geological cirque back into a stadium-shaped ribbon.
    const FVector2D Side(-Back.Y, Back.X);
    float MinRim = UE_BIG_NUMBER, MaxRim = 0.f;
    float MinFan = UE_BIG_NUMBER, MaxFan = 0.f;
    float MinTop = UE_BIG_NUMBER, MaxTop = 0.f;
    int32 Profiles = 0;
    for (int32 I = -17; I <= 17; ++I)
    {
        const float Angle = I * .1f;
        const FVector2D Direction = Back * FMath::Cos(Angle) + Side * FMath::Sin(Angle);
        float PeakCliff = 0.f, LocalRim = 0.f, FirstScree = 0.f;
        for (float R = 35000.f; R < 80000.f; R += 250.f)
        {
            const FVector2D P = Centre + Direction * R;
            const auto S = Cirque::Sample(P.X, P.Y);
            if (S.Cliff > PeakCliff) { PeakCliff = S.Cliff; LocalRim = R; }
            if (FirstScree == 0.f && S.Scree > .2f) FirstScree = R;
        }
        if (PeakCliff < .5f || FirstScree == 0.f) continue;
        ++Profiles;
        MinRim = FMath::Min(MinRim, LocalRim);
        MaxRim = FMath::Max(MaxRim, LocalRim);
        MinFan = FMath::Min(MinFan, LocalRim - FirstScree);
        MaxFan = FMath::Max(MaxFan, LocalRim - FirstScree);
        // Find each ray's own top instead of assuming a circular 650 m rim.
        const FVector2D TopPoint = Centre + Direction * (LocalRim + 4000.f);
        const float TopHeight = Valley::Height(TopPoint.X, TopPoint.Y);
        MinTop = FMath::Min(MinTop, TopHeight);
        MaxTop = FMath::Max(MaxTop, TopHeight);
    }
    TestTrue(TEXT("The rock face remains continuous across the main cirque"), Profiles >= 25);
    TestTrue(TEXT("Buttresses and ravines break the circular wall outline"), MaxRim - MinRim > 9000.f);
    TestTrue(TEXT("Scree fans have visibly different runout distances"), MaxFan - MinFan > 4500.f);
    TestTrue(TEXT("The crown has substantial natural elevation variation"), MaxTop - MinTop > 2200.f);
    return !HasAnyErrors();
}
#endif
