#include "ValleyAir.h"
#include "GliderDynamics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FValleyAirTest, "SimPlaneur.Physics.ValleyWind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValleyAirTest::RunTest(const FString&)
{
    struct FRestoreWeather
    {
        ValleyAir::FWeatherSettings Previous=ValleyAir::Settings();
        ~FRestoreWeather() { ValleyAir::SetSettings(Previous); }
    } RestoreWeather;
    ValleyAir::SetSettings(ValleyAir::FWeatherSettings{});
    TestEqual(TEXT("A southbound flow comes from north"), ValleyAir::FromBearing(FVector(-2,0,0)), 0.);
    TestEqual(TEXT("A westbound flow comes from east"), ValleyAir::FromBearing(FVector(0,-2,0)), 90.);
    TestTrue(TEXT("Facing east puts north to the left"), ValleyAir::CompassDirection(0,90).Equals({-1,0}, 1.e-6));
    TestTrue(TEXT("Facing south puts north behind"), ValleyAir::CompassDirection(0,180).Equals({0,1}, 1.e-6));
    TestTrue(TEXT("Bearings wrap through north"), ValleyAir::CompassDirection(1,359).Equals(ValleyAir::CompassDirection(2,0), 1.e-6));

    FGliderDynamics Thermal, HorizontalOnly;
    if (!TestTrue(TEXT("Load Topy"), Thermal.LoadModel(Thermal.TopyModelPath()))) return false;
    const FVector Breeze = Thermal.LocalWind();
    const FVector PreviousFlow = Thermal.RelativeFlowNED();
    Thermal.SetWind(Breeze + FVector(0, 2, -1));
    TestTrue(TEXT("Relative flow changes with crosswind and rising air"), Thermal.RelativeFlowNED().Equals(PreviousFlow + FVector(0,2,-1), 1.e-6));
    TestEqual(TEXT("Airspeed is measured against the moving air"), Thermal.Airspeed(), Thermal.RelativeFlowNED().Size());

    Thermal.Reset();
    HorizontalOnly = Thermal;
    Thermal.SetWind(Breeze, FVector(0,0,-.15));
    for (int32 I = 0; I < 24; ++I) Thermal.Step(0,0);
    TestTrue(TEXT("Updraft at right wingtip raises the right wing"), Thermal.WorldPose().Rotator().Roll < -.1);

    Thermal.Reset();
    double PeakLift = 0., PeakGain = 0., BestClimb = 0.;
    for (int32 I = 0; I < 120 * 14; ++I)
    {
        const double Time = I * Thermal.TimeStep;
        ValleyAir::SampleGlider(Thermal, Time);
        FVector Flat = ValleyAir::Velocity(HorizontalOnly.PositionNED(), Time);
        Flat.Z = 0.;
        HorizontalOnly.SetWind(Flat);
        PeakLift = FMath::Max(PeakLift, -Thermal.LocalWind().Z);
        Thermal.Step(0,0);
        HorizontalOnly.Step(0,0);
        PeakGain = FMath::Max(PeakGain, Thermal.Altitude() - HorizontalOnly.Altitude());
        BestClimb = FMath::Max(BestClimb, -Thermal.SinkRate());
    }
    UE_LOG(LogTemp, Display, TEXT("VALLEY_AIR free flight peak_lift=%.2f m/s peak_height_gain=%.2f m best_climb=%.2f m/s"), PeakLift, PeakGain, BestClimb);
    TestTrue(TEXT("Rising air is reachable from launch"), PeakLift > .8);
    TestTrue(TEXT("Topy gains height versus the horizontal-wind run"), PeakGain > 1.);
    const FVector Surface(65, 10, -ValleyAir::SurfaceAltitude(65,10));
    double RidgeLift;
    const FVector AtSurface = ValleyAir::Velocity(Surface, 20., &RidgeLift);
    TestTrue(TEXT("Surface flow follows terrain without thermal lift"), FMath::Abs(AtSurface.Z + RidgeLift) < 1.e-6);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRidgeLiftTest, "SimPlaneur.Physics.RidgeLift",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRidgeLiftTest::RunTest(const FString&)
{
    const FVector2D Face(80., 0.), Breeze(-1.96, -.42);
    const FVector Up = ValleyAir::TerrainFlow(Face, 8., Breeze);
    const FVector Lee = ValleyAir::TerrainFlow(Face, 8., -Breeze);
    TestTrue(TEXT("Exposed meadow lifts air"), -Up.Z > .5);
    TestTrue(TEXT("Reversing the breeze gives downslope flow"), Lee.Equals(-Up, 1.e-7));
    TestTrue(TEXT("Deflection fades aloft"), FMath::Abs(ValleyAir::TerrainFlow(Face, 350., Breeze).Z) < .1);
    TestTrue(TEXT("Flat lake leaves horizontal breeze level"), FMath::Abs(ValleyAir::TerrainFlow({-470.,330.}, 8., Breeze).Z) < .001);
    TestTrue(TEXT("No slope lift in calm air"), ValleyAir::TerrainFlow(Face, 8., FVector2D::ZeroVector).IsZero());

    FGliderDynamics Ridge, Flat;
    if (!Ridge.LoadModel(Ridge.TopyModelPath())) { AddError(TEXT("Could not load Topy")); return false; }
    Flat = Ridge;
    Flat.SetWind(FVector(Breeze, 0.));
    double PeakGain = 0.;
    for (int32 I = 0; I < 120 * 16; ++I)
    {
        const FVector P = Ridge.PositionNED();
        const double AGL = -P.Z - ValleyAir::SurfaceAltitude(P.X, P.Y);
        Ridge.SetWind(ValleyAir::TerrainFlow({P.X, P.Y}, AGL, Breeze));
        Ridge.Step(0,0);
        Flat.Step(0,0);
        PeakGain = FMath::Max(PeakGain, Ridge.Altitude() - Flat.Altitude());
    }
    UE_LOG(LogTemp, Display, TEXT("RIDGE_LIFT breeze=%.2f windward=%.2f lee=%.2f m/s glider_gain=%.2f m"), Breeze.Size(), -Up.Z, -Lee.Z, PeakGain);
    TestTrue(TEXT("Ridge lift alone benefits the glider"), PeakGain > 1.);
    return !HasAnyErrors();
}
#endif
