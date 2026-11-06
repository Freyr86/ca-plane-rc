#include "ValleyAir.h"
#include "GliderDynamics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include <limits>

namespace
{
    FVector Above(double North, double East, double Height)
    {
        return FVector(North,East,-ValleyAir::SurfaceAltitude(North,East)-Height);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherRegionsTest,"SimPlaneur.Weather.LocalShowers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherRegionsTest::RunTest(const FString&)
{
    ValleyAir::FWeatherSettings Weather;
    Weather.Sky=5;
    Weather.CloudBaseMeters=900.;
    const auto Cells=ValleyAir::WeatherCells(0.,Weather);
    TestTrue(TEXT("World includes several separated shower cells"),Cells.Num()>2);
    if(Cells.IsEmpty()) return false;
    const FVector2D Centre=Cells[0].Centre;
    const FVector Wet=Above(Centre.X,Centre.Y,2.);
    const double Rain=ValleyAir::RainAt(Wet,0.,Weather);
    TestTrue(TEXT("Initial site lies within a useful rain core"),Rain>.5);
    TestEqual(TEXT("A sheltered altitude above cloud has no rain"),
        ValleyAir::RainAt(FVector(Centre,-1100.),0.,Weather),0.);
    TestEqual(TEXT("Rain is absent under the terrain"),
        ValleyAir::RainAt(Above(Centre.X,Centre.Y,-10.),0.,Weather),0.);
    int32 WetCount=0,DryCount=0;
    for(int32 North=-4000;North<=4000;North+=400)
        for(int32 East=-4000;East<=4000;East+=400)
        {
            const double Value=ValleyAir::RainAt(Above(North,East,2.),0.,Weather);
            TestTrue(TEXT("All rain samples are bounded"),FMath::IsFinite(Value) && Value>=0. && Value<=1.);
            WetCount+=Value>.1;
            DryCount+=Value<.001;
        }
    TestTrue(TEXT("Wet and dry areas coexist in one world"),WetCount>10 && DryCount>100);
    for(double Radius:{Cells[0].RadiusMeters*.25,Cells[0].RadiusMeters*.6,Cells[0].RadiusMeters})
    {
        const FVector Edge=Above(Centre.X+Radius,Centre.Y,2.);
        const double Before=ValleyAir::RainAt(Edge-FVector(.02,0,0),30.,Weather);
        const double After=ValleyAir::RainAt(Edge+FVector(.02,0,0),30.,Weather);
        TestTrue(TEXT("Entering and leaving an averses zone has no abrupt edge"),FMath::Abs(After-Before)<.001);
        TestTrue(TEXT("Rain transitions smoothly in time"),
            FMath::Abs(ValleyAir::RainAt(Edge,30.02,Weather)-ValleyAir::RainAt(Edge,30.,Weather))<.001);
    }
    ValleyAir::RainAt(Above(3000,2200,2),900.,Weather);
    TestEqual(TEXT("Observer order does not alter precipitation"),ValleyAir::RainAt(Wet,0.,Weather),Rain);
    const auto Later=ValleyAir::WeatherCells(10.,Weather);
    const FVector2D Drift=Later[0].Centre-Centre;
    const double Bearing=FMath::DegreesToRadians(Weather.WindFromDegrees);
    TestTrue(TEXT("Cells move downwind"),FVector2D::DotProduct(Drift,FVector2D(-FMath::Cos(Bearing),-FMath::Sin(Bearing)))>0.);
    Weather.WindSpeedMps=0.;
    TestTrue(TEXT("Calm leaves weather cells stationary"),ValleyAir::WeatherCells(10000.,Weather)[0].Centre.Equals(Centre,1.e-8));
    Weather.RainStrength=0.;
    TestEqual(TEXT("Rain slider zero stops all showers"),ValleyAir::RainAt(Wet,0.,Weather),0.);
    Weather.RainStrength=.65;
    Weather.CloudCoverage=0.;
    TestEqual(TEXT("Removing cloud cover also removes rain"),ValleyAir::RainAt(Wet,0.,Weather),0.);
    const double ClearHeating=ValleyAir::SolarHeating(Weather);
    Weather.CloudCoverage=.55;
    TestTrue(TEXT("Removing cover restores solar heating"),ClearHeating>ValleyAir::SolarHeating(Weather));
    for(int32 Sky=0;Sky<5;++Sky)
    {
        Weather.Sky=Sky;
        TestEqual(TEXT("Dry weather presets never produce rain"),ValleyAir::RainAt(Wet,0.,Weather),0.);
    }
    Weather.Sky=4; Weather.CloudBaseMeters=300.;
    const double LowBase=ValleyAir::CloudBaseAt({3000.,2200.},Weather);
    TestTrue(TEXT("Stratus fringe remains close to the configured cloud altitude"),LowBase>=255. && LowBase<=300.);
    Weather.LowClouds=false;
    TestEqual(TEXT("Disabling low fringe preserves the base altitude"),ValleyAir::CloudBaseAt({3000.,2200.},Weather),300.);
    UE_LOG(LogTemp,Display,TEXT("WEATHER_REGIONS wet=%d dry=%d cells=%d core=%.2f"),WetCount,DryCount,Cells.Num(),Rain);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherSettingsTest,"SimPlaneur.Weather.SettingsRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherSettingsTest::RunTest(const FString&)
{
    const auto Previous=ValleyAir::Settings();
    const FString PreviousIni=GGameUserSettingsIni;
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("Automation");
    IFileManager::Get().MakeDirectory(*Folder,true);
    const FString TestIni=FPaths::CreateTempFilename(*Folder,TEXT("Weather"),TEXT(".ini"));
    // CreateTempFilename only reserves a name. Register this isolated config
    // branch before writing: UE ignores Set* for an unknown, nonexistent INI.
    GConfig->Add(TestIni,FConfigFile());
    GGameUserSettingsIni=TestIni;
    ValleyAir::FWeatherSettings Set;
    Set.Sky=6; Set.CloudBaseMeters=375.; Set.CloudCoverage=.81; Set.RainStrength=.73; Set.LowClouds=false;
    ValleyAir::SetSettings(Set);
    ValleyAir::SaveSettings();
    TestTrue(TEXT("Weather settings are written to disk"),IFileManager::Get().FileExists(*TestIni));
    GConfig->UnloadFile(TestIni);
    ValleyAir::SetSettings({});
    ValleyAir::LoadSettings();
    const auto Reloaded=ValleyAir::Settings();
    TestEqual(TEXT("Storm preset survives restart"),Reloaded.Sky,6);
    TestEqual(TEXT("Cloud altitude survives restart"),Reloaded.CloudBaseMeters,375.);
    TestTrue(TEXT("Rain, coverage and low layer option survive restart"),
        FMath::IsNearlyEqual(Reloaded.CloudCoverage,.81,1.e-8) && FMath::IsNearlyEqual(Reloaded.RainStrength,.73,1.e-8) && !Reloaded.LowClouds);
    Set.Sky=123; Set.CloudBaseMeters=-100.; Set.CloudCoverage=3.; Set.RainStrength=-2.;
    ValleyAir::SetSettings(Set);
    const auto Limited=ValleyAir::Settings();
    TestTrue(TEXT("Invalid persisted ranges are clamped"),Limited.Sky==6 && Limited.CloudBaseMeters==80. && Limited.CloudCoverage==1. && Limited.RainStrength==0.);
    Set.CloudBaseMeters=Set.CloudCoverage=Set.RainStrength=std::numeric_limits<double>::quiet_NaN();
    ValleyAir::SetSettings(Set);
    const auto Finite=ValleyAir::Settings();
    TestTrue(TEXT("Non-finite weather settings revert to usable defaults"),
        Finite.CloudBaseMeters==650. && Finite.CloudCoverage==.55 && Finite.RainStrength==.65);
    GGameUserSettingsIni=PreviousIni;
    ValleyAir::SetSettings(Previous);
    GConfig->UnloadFile(TestIni);
    IFileManager::Get().Delete(*TestIni);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherFieldTest,"SimPlaneur.Physics.WeatherField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherFieldTest::RunTest(const FString&)
{
    ValleyAir::FWeatherSettings Lighting;
    TestTrue(TEXT("Morning sun in east, evening in west"),ValleyAir::SunDirection(480).Y>0. && ValleyAir::SunDirection(960).Y<0.);
    TestTrue(TEXT("Noon sun above southern horizon"),ValleyAir::SunDirection(720).Z>.8 && ValleyAir::SunDirection(720).X<0.);
    const double ReferenceHeating=ValleyAir::SolarHeating(Lighting);
    Lighting.Sky=2;
    TestTrue(TEXT("Overcast weakens solar heating"),ValleyAir::SolarHeating(Lighting)<ReferenceHeating*.5);
    Lighting.SolarMinutes=0;
    TestEqual(TEXT("Solar convection stops at night"),ValleyAir::SolarHeating(Lighting),0.);
    ValleyAir::FWeatherSettings Calm;
    Calm.WindSpeedMps=0.; Calm.ThermalStrength=0.; Calm.Turbulence=1.;
    ValleyAir::FWeatherSettings Weather;
    for (const FVector2D XY : {FVector2D(80,-18),FVector2D(120,-18),FVector2D(-470,330),FVector2D(900,-1200)})
    {
        for (double Height : {0.,2.,20.,100.,500.})
        {
            const FVector Position=Above(XY.X,XY.Y,Height);
            TestTrue(TEXT("Calm without convection has no residual wind or gust"),
                ValleyAir::Sample(Position,37.,Calm).VelocityNED.IsNearlyZero(1.e-9));
            const auto Air=ValleyAir::Sample(Position,37.,Weather);
            TestFalse(TEXT("All weather samples are finite"),Air.VelocityNED.ContainsNaN());
            TestTrue(TEXT("Wind diagnostics sum to the actual vertical air velocity"),
                FMath::Abs(-Air.VelocityNED.Z-Air.RidgeLiftMps-Air.ThermalLiftMps+Air.TurbulenceNED.Z)<1.e-8);
            TestTrue(TEXT("Sampling does not depend on previous calls"),
                Air.VelocityNED.Equals(ValleyAir::Sample(Position,37.,Weather).VelocityNED,1.e-10));
        }
        TestTrue(TEXT("No thermal vertical velocity enters through the ground"),
            FMath::Abs(ValleyAir::Sample(Above(XY.X,XY.Y,0.),37.,Weather).ThermalLiftMps)<1.e-8);
    }
    const FVector2D Face(80.,-18.), Breeze(-4.4,-.94);
    const auto Up=ValleyAir::TerrainFlow(Face,20.,Breeze);
    const auto Reverse=ValleyAir::TerrainFlow(Face,20.,-Breeze);
    TestTrue(TEXT("Wind reversal reverses terrain-induced lift"),Up.Z<0. && Reverse.Z>0.);
    TestTrue(TEXT("Linear terrain response preserves directional symmetry"),Up.Equals(-Reverse,1.e-8));
    TestTrue(TEXT("No orographic lift without horizontal wind"),
        ValleyAir::TerrainFlow(Face,20.,FVector2D::ZeroVector).IsNearlyZero(1.e-9));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherCoherenceTest,"SimPlaneur.Physics.WeatherCoherence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherCoherenceTest::RunTest(const FString&)
{
    ValleyAir::FWeatherSettings Weather;
    double MaxSpaceJump=0.,MaxTimeJump=0.;
    // Cross regular world-grid boundaries in both directions, including negative
    // coordinates. Individual wing samples must see a continuous shared field.
    for (int32 N=-12;N<=12;++N)
    {
        for (double Height : {10.,50.,150.})
        {
            for (const FVector2D XY : {FVector2D(N*100.,-18.),FVector2D(80.,N*100.)})
            {
                const FVector Position=Above(XY.X,XY.Y,Height);
                for (double Time : {0.,45.,180.})
                {
                    const FVector Base=ValleyAir::Sample(Position,Time,Weather).VelocityNED;
                    MaxTimeJump=FMath::Max(MaxTimeJump,(Base-ValleyAir::Sample(Position,Time+.02,Weather).VelocityNED).Size());
                    for (FVector Step : {FVector(.02,0,0),FVector(0,.02,0),FVector(0,0,.02)})
                    {
                        const FVector Before=ValleyAir::Sample(Position-Step,Time,Weather).VelocityNED;
                        const FVector After=ValleyAir::Sample(Position+Step,Time,Weather).VelocityNED;
                        MaxSpaceJump=FMath::Max(MaxSpaceJump,(After-Before).Size());
                    }
                }
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("WEATHER_COHERENCE max_spatial_delta_4cm=%.5f max_temporal_delta_20ms=%.5f m/s"),MaxSpaceJump,MaxTimeJump);
    TestTrue(TEXT("A four-centimetre displacement has no wind discontinuity"),MaxSpaceJump<.12);
    TestTrue(TEXT("A 20 ms time step has no gust discontinuity"),MaxTimeJump<.12);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherLiftEnvelopeTest,"SimPlaneur.Physics.WeatherLiftEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherLiftEnvelopeTest::RunTest(const FString&)
{
    ValleyAir::FWeatherSettings Ridge;
    Ridge.ThermalStrength=0.; Ridge.Turbulence=0.;
    double Sum=0.,Peak=0.;
    int32 Count=0,Useful=0;
    // The real default pilot is at (64,-18), not the legacy flight fixture's
    // origin. Measure a useful band ahead of that site at 20 metres AGL.
    for (double North : {80.,100.,120.})
        for (double East : {-28.,-18.,-8.})
        {
            const double Lift=ValleyAir::Sample(Above(North,East,20.),30.,Ridge).RidgeLiftMps;
            Sum+=Lift; Peak=FMath::Max(Peak,Lift); ++Count;
            if (Lift>.65) ++Useful;
        }
    UE_LOG(LogTemp,Display,TEXT("WEATHER_RIDGE default_site height_agl=20 mean=%.3f peak=%.3f useful=%d/%d"),Sum/Count,Peak,Useful,Count);
    TestTrue(TEXT("Slope lift covers a band rather than one isolated point"),Useful>=6);
    TestTrue(TEXT("Default slope band has usable average lift"),Sum/Count>.8);

    // Start trimmed approximately into wind over the face, already airborne.
    // An altitude gain alone can be a zoom climb. Demand positive air-relative
    // specific energy as well, with unobstructed flight and no thermal energy.
    struct FRidgeSetup { const TCHAR* Name; int32 CgMm; };
    const FRidgeSetup Setups[]={{TEXT("Topy4522"),0},{TEXT("K3600"),0},{TEXT("K3600"),94}};
    for (const auto& Setup : Setups)
    {
        FGliderDynamics Model;
        if (!TestTrue(TEXT("Load ridge-pass aircraft"),Model.LoadModel(
            FPaths::ProjectContentDir()/TEXT("FlightModels")/(FString(Setup.Name)+TEXT(".json"))))) return false;
        if (Setup.CgMm!=0) Model.SetLoading({Model.StandardLoading().MassGrams,Setup.CgMm});
        const FVector Start=Above(80.,-18.,20.);
        const FVector Air=ValleyAir::Sample(Start,30.,Ridge).VelocityNED;
        const FVector GroundVelocity=Air+FVector(8.,0.,.5);
        Model.SetWind(Air);
        Model.LaunchFrom(FTransform(FRotator(0,0,0),100.*FVector(Start.X,Start.Y,-Start.Z)),
            FVector(GroundVelocity.X,GroundVelocity.Y,-GroundVelocity.Z));
        const double InitialHeight=Model.Altitude(), InitialSpeed=Model.Airspeed();
        double MinAGL=20.;
        for (int32 I=0;I<12*120;++I)
        {
            const FVector Centre=Model.PositionNED(), Offset=Model.RightTipOffsetNED();
            const double Time=30.+I*FGliderDynamics::TimeStep;
            Model.SetWind(ValleyAir::Sample(Centre,Time,Ridge).VelocityNED,
                ValleyAir::Sample(Centre+Offset,Time,Ridge).VelocityNED-ValleyAir::Sample(Centre-Offset,Time,Ridge).VelocityNED);
            Model.Step(0,0);
            const FVector P=Model.PositionNED();
            MinAGL=FMath::Min(MinAGL,-P.Z-ValleyAir::SurfaceAltitude(P.X,P.Y));
        }
        const double HeightGain=Model.Altitude()-InitialHeight;
        const double EnergyGain=HeightGain+(FMath::Square(Model.Airspeed())-FMath::Square(InitialSpeed))/(2.*9.80665);
        UE_LOG(LogTemp,Display,TEXT("WEATHER_RIDGE_PASS %s cg_mm=%d ridge_only dt=12s altitude_gain=%.2f energy_gain_m=%.2f min_agl=%.2f"),
            Setup.Name,Model.Loading().CgMm,HeightGain,EnergyGain,MinAGL);
        TestTrue(TEXT("Ridge-only pass gains altitude without borrowing all the gain from airspeed"),HeightGain>1. && EnergyGain>1.);
        TestTrue(TEXT("Pass remains clear of the terrain"),MinAGL>2.);
    }

    ValleyAir::FWeatherSettings Convection;
    Convection.WindSpeedMps=0.; Convection.Turbulence=0.;
    // Sample one 30 m-radius turn at 7.5 m/s around the source in windless
    // convection. Keep a fixed altitude, so terrain relief cannot fake lift.
    constexpr int32 Bearings=72;
    constexpr double Radius=30., OrbitSpeed=7.5;
    const FVector Centre=Above(65.,10.,60.);
    double OrbitSum=0., OrbitMin=TNumericLimits<double>::Max();
    for (int32 I=0;I<Bearings;++I)
    {
        const double Angle=2.*UE_PI*I/Bearings;
        const FVector Point=Centre+Radius*FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.);
        const double Time=30.+Angle*Radius/OrbitSpeed;
        const double Lift=ValleyAir::Sample(Point,Time,Convection).ThermalLiftMps;
        OrbitSum+=Lift;
        OrbitMin=FMath::Min(OrbitMin,Lift);
    }
    UE_LOG(LogTemp,Display,TEXT("WEATHER_THERMAL_ORBIT radius_m=30 height_above_source_m=60 mean_lift=%.3f min_lift=%.3f"),
        OrbitSum/Bearings,OrbitMin);
    TestTrue(TEXT("A full thermal turn offers useful lift around the circuit"),OrbitSum/Bearings>.9 && OrbitMin>.6);
    double PeakAway=0.,Lowest=0.;
    for (int32 North=-1000;North<=1000;North+=50)
        for (int32 East=-1000;East<=1000;East+=50)
        {
            if (FMath::Abs(North)<300 && FMath::Abs(East)<300) continue;
            const auto Air=ValleyAir::Sample(Above(North,East,60.),90.,Convection);
            PeakAway=FMath::Max(PeakAway,Air.ThermalLiftMps);
            Lowest=FMath::Min(Lowest,Air.ThermalLiftMps);
        }
    UE_LOG(LogTemp,Display,TEXT("WEATHER_THERMALS away_from_initial_site peak=%.3f sink=%.3f m/s"),PeakAway,Lowest);
    TestTrue(TEXT("Convection exists away from the original three launch-site thermals"),PeakAway>.8);
    TestTrue(TEXT("Convective cores have compensating sinking air"),Lowest<-.03);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindTransportTest,"SimPlaneur.Physics.WindTransport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWindTransportTest::RunTest(const FString&)
{
    constexpr double Updraft=2., Duration=20.;
    for (const TCHAR* Name : {TEXT("Topy4522"),TEXT("K3600")})
    {
        FGliderDynamics Calm;
        if (!TestTrue(TEXT("Load aircraft for moving-air integration"),
            Calm.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels")/(FString(Name)+TEXT(".json"))))) return false;
        FGliderDynamics Rising=Calm;
        const FTransform Pose(FRotator(-1.4,0,0),FVector(8000,-1800,15000));
        Calm.SetWind(FVector::ZeroVector);
        Rising.SetWind(FVector(0,0,-Updraft));
        Calm.LaunchFrom(Pose,FVector(8,0,-.5));
        Rising.LaunchFrom(Pose,FVector(8,0,Updraft-.5));
        TestTrue(TEXT("Both aircraft start with identical air-relative velocity"),
            Calm.RelativeFlowNED().Equals(Rising.RelativeFlowNED(),1.e-10));
        for (int32 I=0;I<int32(Duration/FGliderDynamics::TimeStep);++I)
        {
            Calm.Step(0,0); Rising.Step(0,0);
        }
        const double Gain=Rising.Altitude()-Calm.Altitude();
        UE_LOG(LogTemp,Display,TEXT("WIND_TRANSPORT %s gain=%.3f expected=%.3f calm_sink=%.3f rising_sink=%.3f airspeed_delta=%.4f"),
            Name,Gain,Updraft*Duration,Calm.SinkRate(),Rising.SinkRate(),Rising.Airspeed()-Calm.Airspeed());
        TestTrue(TEXT("Uniform updraft transports the aircraft with the air mass"),FMath::Abs(Gain-Updraft*Duration)<.7);
        TestTrue(TEXT("Updraft does not artificially accelerate air-relative flight"),FMath::Abs(Rising.Airspeed()-Calm.Airspeed())<.15);
        TestTrue(TEXT("Uniform updraft produces net climb for both standard gliders"),Rising.SinkRate()<-.3);
    }
    return !HasAnyErrors();
}
#endif
