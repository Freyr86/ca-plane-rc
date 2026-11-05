#include "BungeeLaunch.h"
#include "GliderDynamics.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWinchForceTest,"SimPlaneur.Physics.WinchForce",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWinchForceTest::RunTest(const FString&)
{
    FWinchDrive W; W.Start(150.,1.);
    for(int I=0;I<120;++I) W.Step(1./120.,150.,0.,0.);
    TestEqual(TEXT("Power off leaves reel stopped"),W.ReelSpeed,0.);
    TestEqual(TEXT("Power off pays out no line"),W.PaidOut,150.);
    TestEqual(TEXT("No artificial launch force with power off"),W.Tension,0.);
    W.Start(150.,1.);
    W.Step(1./120.,150.,0.,1.);
    TestTrue(TEXT("Motor starts progressively"),W.ReelSpeed<.2 && W.Tension<2.);
    for(int I=0;I<600;++I) W.Step(1./120.,150.,0.,1.);
    TestTrue(TEXT("Load slows reel without unlimited tension"),W.ReelSpeed<1. && W.Tension>20. && W.Tension<=W.MaximumForce()*1.35);
    W.Step(1./120.,140.,0.,1.);
    TestEqual(TEXT("Slack cable cannot push aircraft"),W.Tension,0.);
    for(int I=0;I<120;++I) W.Step(1./120.,20.,0.,0.);
    TestTrue(TEXT("Sustained slack triggers release after initial tension"),W.Slack());
    FBungeeLaunch B; B.Type=ELaunchEquipment::Winch; B.PlaceAnchor(FVector::ZeroVector);
    TestFalse(TEXT("Winch departure must leave launch distance"),B.SetDeparture({-3000,0,0},1.));
    TestTrue(TEXT("150 m winch layout accepted"),B.SetDeparture({-15000,0,0},1.));
    B.BeginTow({-15000,0,145},1.);
    for(int I=0;I<240;++I) B.AdvanceTow(1./120.,{-15000,0,145},FVector::ZeroVector,1.);
    const FVector Force=B.ForceWorld({-15000,0,145},FVector::ZeroVector);
    TestTrue(TEXT("Tow points to anchor, including downward component"),Force.X>0. && Force.Z<0.);
    TestTrue(TEXT("Hook releases near vertical"),B.ReleaseCondition({-1000,0,10000})==FBungeeLaunch::ERelease::Angle);
    B.Winch.PaidOut=FWinchDrive::EndLength;
    TestTrue(TEXT("End stop releases aircraft"),B.ReleaseCondition({-8000,0,2000})==FBungeeLaunch::ERelease::EndOfLine);
    B.Release();
    TestTrue(TEXT("Manual release stops all tow force"),B.ForceWorld({-8000,0,2000},FVector::ZeroVector).IsZero());
    TestTrue(TEXT("Installation remains reusable"),B.State==FBungeeLaunch::EState::Ready);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWinchFlightTest,"SimPlaneur.Physics.WinchFlight",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWinchFlightTest::RunTest(const FString&)
{
    for(const TCHAR* Name:{TEXT("Topy"),TEXT("K3600")})
    {
        FGliderDynamics G;
        const FString Path=FString(Name)==TEXT("Topy")?G.TopyModelPath():FPaths::ProjectContentDir()/TEXT("FlightModels/K3600.json");
        if(!TestTrue(TEXT("Load winch test aircraft"),G.LoadModel(Path))) return false;
        G.Reset(FVector::ZeroVector);
        const FTransform Start(FRotator(12,0,0),FVector(-15000,0,10145));
        G.LaunchFrom(Start,Start.GetRotation().GetForwardVector()*8.);
        FBungeeLaunch B; B.Type=ELaunchEquipment::Winch;
        B.PlaceAnchor({0,0,10000}); B.SetDeparture({-15000,0,10000},G.Loading().MassGrams*.001);
        B.BeginTow(Start.GetLocation(),G.Loading().MassGrams*.001);
        double Peak=G.Altitude(),Min=Peak,ReleaseHeight=0.,PeakSpeed=0.;
        bool Released=false;
        for(int I=0;I<2400;++I)
        {
            const FVector P=G.WorldPose().GetLocation(), V=G.VelocityNED();
            B.AdvanceTow(G.TimeStep,P,{V.X,V.Y,-V.Z},1.);
            if(!Released && B.ReleaseCondition(P)!=FBungeeLaunch::ERelease::None)
            { Released=true; ReleaseHeight=G.Altitude()-100.; B.Release(); }
            const FVector F=B.ForceWorld(P,{V.X,V.Y,-V.Z});
            // Pilot script holds a climb attitude with the normal elevator.
            const double Target=Released?5.:45.;
            const double Elevator=FMath::Clamp((G.WorldPose().Rotator().Pitch-Target)*.025,-.65,.65);
            G.Step(Elevator,0.,0.,0.,{F.X,F.Y,-F.Z});
            Peak=FMath::Max(Peak,G.Altitude()); PeakSpeed=FMath::Max(PeakSpeed,G.Airspeed());
            if(!Released) Min=FMath::Min(Min,G.Altitude());
        }
        AddInfo(FString::Printf(TEXT("WINCH_FLIGHT %s peak=%.1f release=%.1f min=%.2f speed=%.1f released=%d"),Name,Peak-100.,ReleaseHeight,Min-100.,PeakSpeed,Released));
        TestTrue(TEXT("Winch climbs with pilot elevator input"),Peak>125.);
        TestTrue(TEXT("No impact during tow"),Min>100.);
        TestTrue(TEXT("Automatic release completes launch"),Released && ReleaseHeight>15.);
        TestFalse(TEXT("Flight stays finite"),G.WorldPose().ContainsNaN());
    }
    return true;
}
#endif
