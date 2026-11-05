#include "BungeeLaunch.h"
#include "GliderDynamics.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBungeeForceTest,"SimPlaneur.Physics.BungeeForce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBungeeForceTest::RunTest(const FString&)
{
    FBungeeLaunch B;
    TestFalse(TEXT("Cannot place departure before anchor"),B.SetDeparture(FVector(-7000,0,0),1.));
    B.PlaceAnchor(FVector::ZeroVector);
    TestFalse(TEXT("Reject short layout"),B.SetDeparture(FVector(-300,0,0),1.));
    TestFalse(TEXT("Reject excessive extension"),B.SetDeparture(FVector(-10000,0,0),1.));
    auto SlackLayout=B;
    TestTrue(TEXT("Departure can be placed before tensioning"),SlackLayout.SetDeparture(FVector(-3000,0,0),1.));
    TestTrue(TEXT("Slack layout stays installed"),SlackLayout.State==FBungeeLaunch::EState::Ready);
    TestTrue(TEXT("Slack layout exerts no force"),SlackLayout.ForceWorld(SlackLayout.Departure,FVector::ZeroVector).IsZero());
    TestTrue(TEXT("Accept seventy metre layout"),B.SetDeparture(FVector(-7000,0,0),1.));
    TestTrue(TEXT("Tension equals four weights at 25 m extension"),FMath::IsNearlyEqual(B.ForceWorld(B.Departure,FVector::ZeroVector).X,4.*9.80665,1.e-6));
    TestTrue(TEXT("Slack cable exerts no force"),B.ForceWorld(FVector(-4000,0,0),FVector(-100,0,0)).IsZero());
    TestTrue(TEXT("Force opposes extension"),B.ForceWorld(FVector(7000,0,0),FVector::ZeroVector).X<0.);
    B.State=FBungeeLaunch::EState::Towing;
    TestTrue(TEXT("Climbing below the hook angle stays attached"),B.ReleaseCondition(FVector(3500,0,6000))==FBungeeLaunch::ERelease::None);
    TestTrue(TEXT("Hook releases near the vertical while line is taut"),B.ReleaseCondition(FVector(1000,0,6000))==FBungeeLaunch::ERelease::Angle);
    TestTrue(TEXT("Slack line still releases"),B.ReleaseCondition(FVector(2000,0,1000))==FBungeeLaunch::ERelease::Slack);
    TestTrue(TEXT("Excessive extension releases for safety"),B.ReleaseCondition(FVector(12000,0,0))==FBungeeLaunch::ERelease::Overstretch);
    B.Release();
    TestTrue(TEXT("Angle cannot trigger while laying out or carrying"),B.ReleaseCondition(FVector(1000,0,6000))==FBungeeLaunch::ERelease::None);
    TestTrue(TEXT("Release preserves installation"),B.State==FBungeeLaunch::EState::Ready && B.Departure.X==-7000.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBungeeFlightTest,"SimPlaneur.Physics.BungeeFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBungeeFlightTest::RunTest(const FString&)
{
    FGliderDynamics Glider;
    if(!TestTrue(TEXT("Load Topy"),Glider.LoadModel(Glider.TopyModelPath()))) return false;
    FBungeeLaunch B;
    B.PlaceAnchor(FVector(0,0,10000));
    B.SetDeparture(FVector(-7000,0,10000),Glider.Loading().MassGrams*.001);
    Glider.Reset(FVector::ZeroVector);
    Glider.LaunchFrom(FTransform(FRotator(12,0,0),FVector(-7000,0,10145)),FVector(8,0,1));
    auto Free=Glider;
    double Peak=Glider.Altitude();
    bool Released=false;
    B.State=FBungeeLaunch::EState::Towing;
    for(int I=0;I<1200;++I)
    {
        const FVector Position=Glider.WorldPose().GetLocation();
        if(B.ReleaseCondition(Position)!=FBungeeLaunch::ERelease::None) Released=true;
        const FVector V=Glider.VelocityNED();
        const FVector Force=Released?FVector::ZeroVector:B.ForceWorld(Position,FVector(V.X,V.Y,-V.Z));
        Glider.Step(-.2,0.,0.,0.,FVector(Force.X,Force.Y,-Force.Z));
        Free.Step(-.2,0.);
        Peak=FMath::Max(Peak,Glider.Altitude());
        if(I==60) TestTrue(TEXT("Tow accelerates aircraft"),Glider.Airspeed()>Free.Airspeed()+2.);
    }
    AddInfo(FString::Printf(TEXT("Bungee peak %.1f m above launch, released %d"),Peak-101.45,Released));
    TestTrue(TEXT("Tow produces climb"),Peak>106.45);
    TestTrue(TEXT("Cable slackens after climb"),Released);
    TestFalse(TEXT("Flight remains finite"),Glider.WorldPose().ContainsNaN());
    return true;
}
#endif
