#include "LandingDamage.h"
#include "GliderDynamics.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLandingDamageTest,"SimPlaneur.Physics.LandingDamage",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLandingDamageTest::RunTest(const FString&)
{
    const FVector Up=FVector::UpVector,Zero=FVector::ZeroVector;
    TestEqual(TEXT("Fast sliding uses only the ground approach speed"),LandingContactSpeed(FVector(1500,0,-80),Zero,Zero,Up),.8);
    TestEqual(TEXT("An outgoing rebound is not another impact"),LandingContactSpeed(FVector(1500,0,120),Zero,Zero,Up),0.);
    TestEqual(TEXT("Rotation can drive a wing tip into the ground"),LandingContactSpeed(Zero,FVector(-2,0,0),FVector(0,100,0),Up),2.);
    const FVector SlopeNormal(-.6,0,.8),AlongSlope(800,0,600);
    TestTrue(TEXT("Following a slope is not a hard landing"),FMath::IsNearlyZero(LandingContactSpeed(AlongSlope,Zero,Zero,SlopeNormal)));
    TestEqual(TEXT("Gentle touchdown is not damage"),LandingDamage(.8),0);
    TestEqual(TEXT("Hard landing is damage"),LandingDamage(3.),1);
    TestEqual(TEXT("Violent impact is crash"),LandingDamage(7.),2);
    TestEqual(TEXT("Tree strike can damage at lower speed"),LandingDamage(2.,false,true),1);
    TestEqual(TEXT("Inverted ground contact damages"),LandingDamage(.5,false,false,true),1);
    TestEqual(TEXT("Water landing is crash"),LandingDamage(.1,true),2);
    FGliderDynamics Topy,Apus;
    if(!TestTrue(TEXT("Load Topy resistance"),Topy.LoadModel(Topy.TopyModelPath())) ||
       !TestTrue(TEXT("Load APUS resistance"),Apus.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/Apus4.json")))) return false;
    TestTrue(TEXT("APUS more resistant than Topy"),Apus.ImpactTolerance()>Topy.ImpactTolerance());
    TestEqual(TEXT("Topy accepts a slightly firm upright touchdown"),LandingDamage(2.3/Topy.ImpactTolerance()),0);
    TestEqual(TEXT("Topy still takes damage on a hard landing"),LandingDamage(3./Topy.ImpactTolerance()),1);
    TestEqual(TEXT("Same impact crashes Topy"),LandingDamage(6.5/Topy.ImpactTolerance()),2);
    TestEqual(TEXT("Same impact damages APUS"),LandingDamage(6.5/Apus.ImpactTolerance()),1);
    const double Unballasted=Apus.ImpactTolerance();
    Apus.SetLoading({Apus.StandardLoading().MassGrams+100,Apus.StandardLoading().CgMm});
    TestTrue(TEXT("Extra mass increases impact energy for unchanged structure"),Apus.ImpactTolerance()<Unballasted);
    return true;
}
#endif
