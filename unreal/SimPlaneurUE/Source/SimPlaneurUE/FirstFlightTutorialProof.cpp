#include "SlopeFlightPawn.h"

#if !UE_BUILD_SHIPPING
#include "FlightMenu.h"
#include "Camera/CameraTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"

namespace
{
    TSharedPtr<SWidget> TutorialTaggedWidget(const TSharedRef<SWidget>& Widget,FName Tag)
    {
        if(Widget->GetTag()==Tag) return Widget;
        FChildren* Children=Widget->GetChildren();
        for(int32 I=0;Children && I<Children->Num();++I)
            if(auto Found=TutorialTaggedWidget(Children->GetChildAt(I),Tag)) return Found;
        return nullptr;
    }
    bool SameTutorialWeather(const ValleyAir::FWeatherSettings& A,const ValleyAir::FWeatherSettings& B)
    {
        return FMath::IsNearlyEqual(A.WindSpeedMps,B.WindSpeedMps)
            && FMath::IsNearlyEqual(A.WindFromDegrees,B.WindFromDegrees)
            && FMath::IsNearlyEqual(A.ThermalStrength,B.ThermalStrength)
            && FMath::IsNearlyEqual(A.Turbulence,B.Turbulence)
            && A.SolarMinutes==B.SolarMinutes && A.Sky==B.Sky
            && FMath::IsNearlyEqual(A.CloudBaseMeters,B.CloudBaseMeters)
            && FMath::IsNearlyEqual(A.CloudCoverage,B.CloudCoverage)
            && FMath::IsNearlyEqual(A.RainStrength,B.RainStrength) && A.LowClouds==B.LowClouds;
    }
}
#endif

void ASlopeFlightPawn::TestFirstFlightTutorial(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    using S=EFirstFlightStep;
    static int32 Phase=0;
    static float NextAt=5.f;
    static ValleyAir::FWeatherSettings OriginalWeather;
    static int32 OriginalModel=0,OriginalPreview=0,OriginalSite=0;
    static FVector OriginalPilot;
    static FVector2D OriginalOffset;
    static bool OriginalTrails=false;
    static TArray<FString> OriginalWeatherSection;
    if(Elapsed>100.f)
    {
        UE_LOG(LogTemp,Error,TEXT("TUTORIAL_PROOF timeout phase=%d stage=%d"),Phase,int(Tutorial.Step));
        FPlatformMisc::RequestExitWithStatus(false,3); return;
    }
    if(!bAppearanceReady || Elapsed<NextAt || FScreenshotRequest::IsScreenshotRequested()) return;
    bool OK=true;
    auto Click=[&](FName Tag)
    {
        auto Widget=Menu.IsValid()?TutorialTaggedWidget(Menu.ToSharedRef(),Tag):nullptr;
        if(!Widget.IsValid() || !Widget->IsEnabled() || Widget->GetType()!=TEXT("SButton")) return false;
        StaticCastSharedPtr<SButton>(Widget)->SimulateClick(); return true;
    };
    auto Shot=[](const TCHAR* Name)
    { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false,false); };
    auto AdvanceLesson=[&](float Seconds)
    { for(int32 I=0;I<FMath::CeilToInt(Seconds/.05f);++I) UpdateFirstFlightTutorial(.05f); };
    auto SafeAirborne=[&]()
    {
        ClearGroundPhysics(); ContactKind=NAME_None;
        const FTransform Pose(FRotator(0,TutorialHeading,0),Tutorial.LaunchPoint+FVector(0,0,5000));
        Flight.LaunchFrom(Pose,Pose.GetRotation().GetForwardVector()*9.);
        bHeldForLaunch=false; bInBackpack=false;
        PreviousPose=Pose; TargetPosition=Pose.GetLocation(); SetActorTransform(Pose);
    };
    auto Launch=[&]()
    {
        LaunchHeading=TutorialHeading; WalkPitch=0;
        BeginHandLaunch();
        for(int32 I=0;I<8;++I) AdvanceFlight(.05f);
        UpdateFirstFlightTutorial(.05f);
        return !bHeldForLaunch && !Flight.IsStopped();
    };
    switch(Phase)
    {
    case 0:
        OriginalWeather=ValleyAir::Settings(); OriginalModel=SelectedModel; OriginalPreview=PreviewModel;
        OriginalSite=SelectedSite; OriginalPilot=PilotSite; OriginalOffset=PilotOffset; OriginalTrails=bShowWindTrails;
        GConfig->GetSection(TEXT("SimPlaneur.Weather"),OriginalWeatherSection,GGameUserSettingsIni);
        OriginalWeatherSection.Sort();
        if(!Menu.IsValid()) OpenMenu();
        OK=Click(TEXT("Tutorial")); Shot(TEXT("tutorial-menu.png"));
        break;
    case 1:
        OK=Click(TEXT("TutorialStart"));
        OK &= Tutorial.Step==S::Walk && SelectedModel==0 && SelectedSite==2 && bInBackpack;
        OK &= FMath::IsNearlyEqual(TutorialWeather.WindSpeedMps,3.)
            && FMath::IsNearlyEqual(TutorialWeather.ThermalStrength,.25)
            && FMath::IsNearlyEqual(TutorialWeather.Turbulence,.015);
        break;
    case 2:
        // Loading a new mesh can defer the normal menu-to-game transition.
        if(bMenuOpen) Fly();
        OK &= !bMenuOpen && Tutorial.Step==S::Walk && !bShowWindTrails;
        Shot(TEXT("tutorial-walk.png"));
        break;
    case 3:
    {
        PilotSite=Tutorial.WalkTarget; PilotOffset=FVector2D::ZeroVector;
        UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Windsock;
        const FRotator Look=(Tutorial.WindsockTarget-(PilotPosition()+FVector(0,0,170))).Rotation();
        LaunchHeading=Look.Yaw; WalkPitch=Look.Pitch;
        AdvanceLesson(1.7f);
        OK &= Tutorial.Step==S::Airflow;
        break;
    }
    case 4:
        ShowTrails(true); AdvanceLesson(2.2f);
        OK &= Tutorial.Step==S::Controls && !Tutorial.ControlsReady();
        Shot(TEXT("tutorial-controls.png"));
        break;
    case 5:
        Rudder=-.85f; Elevator=-.85f; Tutorial.ObserveControls(Rudder,Elevator); UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.RudderLeft && Tutorial.ElevatorUp && !Tutorial.ElevatorDown && Tutorial.Step==S::Controls;
        Rudder=.85f; Elevator=.85f; Tutorial.ObserveControls(Rudder,Elevator); UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.RudderRight && Tutorial.ElevatorDown && Tutorial.Step==S::Controls;
        Rudder=Elevator=0; Tutorial.ObserveControls(Rudder,Elevator); UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Launch && bInBackpack;
        ToggleBackpack(); AdvanceFlight(0);
        OK &= !bInBackpack && bHeldForLaunch;
        OK &= Launch() && Tutorial.Step==S::Fly;
        SafeAirborne();
        break;
    case 6:
    {
        SafeAirborne(); Tutorial.bRecoveryNeeded=Tutorial.bCrashed=false; Tutorial.Go(S::Fly);
        const float BeforeEarlyLanding=Tutorial.FlightSeconds;
        const FTransform EarlyLanding(FRotator(0,TutorialHeading,0),Tutorial.LaunchPoint+FVector(0,0,20));
        Flight.StopAt(EarlyLanding); SetActorTransform(EarlyLanding);
        UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Retrieve && Tutorial.FlightSeconds==BeforeEarlyLanding;
        PilotSite=Tutorial.LandingTarget; PilotOffset=FVector2D::ZeroVector;
        ToggleBackpack(); UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Launch && !bInBackpack && Tutorial.FlightSeconds==BeforeEarlyLanding;
        OK &= Launch() && Tutorial.Step==S::Fly;
        SafeAirborne();
        const float BeforePause=Tutorial.FlightSeconds;
        bMenuOpen=true; AdvanceLesson(1.f); bMenuOpen=false;
        OK &= Tutorial.FlightSeconds==BeforePause;
        // Offscreen camera orientation alone must not trigger recovery.
        LaunchHeading+=180.; WalkPitch=-70.; AdvanceLesson(.5f);
        OK &= !Tutorial.bRecoveryNeeded;
        const FTransform Far(FRotator::ZeroRotator,Tutorial.LaunchPoint+FVector(45000,0,20000));
        Flight.LaunchFrom(Far,FVector(10,0,0)); SetActorTransform(Far);
        AdvanceLesson(5.4f);
        OK &= Tutorial.bRecoveryNeeded && !Tutorial.bCrashed;
        const float LostTime=Tutorial.FlightSeconds;
        AdvanceLesson(1.f); OK &= Tutorial.FlightSeconds==LostTime;
        RecoverFirstFlightTutorial();
        OK &= Tutorial.Step==S::Launch && bHeldForLaunch && !Tutorial.bRecoveryNeeded
            && Tutorial.FlightSeconds==LostTime && PilotPosition().Equals(Tutorial.LaunchPoint,1.);
        OK &= Launch() && Tutorial.Step==S::Fly;
        SafeAirborne(); Tutorial.FlightSeconds=150.f; UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Landing && FMath::IsNearlyEqual(ValleyAir::Settings().WindSpeedMps,TutorialWeather.WindSpeedMps);
        AdvanceLesson(3.f);
        OK &= FMath::IsNearlyEqual(ValleyAir::Settings().WindSpeedMps,TutorialWeather.WindSpeedMps);
        AdvanceLesson(12.f);
        OK &= ValleyAir::Settings().WindSpeedMps<TutorialWeather.WindSpeedMps && ValleyAir::Settings().WindSpeedMps>1.2;
        Shot(TEXT("tutorial-landing.png"));
        break;
    }
    case 7:
    {
        // A tree contact is recoverable even before rigid-body settling completes.
        ContactKind=TEXT("tree"); ImpactDamage=2; UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.bRecoveryNeeded && Tutorial.bCrashed;
        const float Before=Tutorial.FlightSeconds;
        AdvanceLesson(1.f); OK &= Tutorial.FlightSeconds==Before;
        RecoverFirstFlightTutorial();
        OK &= Tutorial.Step==S::Launch && !Tutorial.bRecoveryNeeded && ImpactDamage==0;
        OK &= Launch() && Tutorial.Step==S::Landing;
        SafeAirborne();
        AdvanceLesson(26.f);
        OK &= FMath::IsNearlyEqual(ValleyAir::Settings().WindSpeedMps,1.2,.001);
        // A slightly hard ground landing still completes the lesson with a warning.
        ClearGroundPhysics(); ContactKind=TEXT("ground"); ImpactDamage=1;
        const FTransform Landed(FRotator(0,TutorialHeading,0),Tutorial.LandingTarget+FVector(0,0,20));
        Flight.StopAt(Landed); SetActorTransform(Landed); bHeldForLaunch=false; bInBackpack=false;
        UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Retrieve && !Tutorial.bRecoveryNeeded && Tutorial.bRoughLanding;
        PilotSite=Tutorial.LandingTarget; PilotOffset=FVector2D::ZeroVector;
        ToggleBackpack(); UpdateFirstFlightTutorial(.05f);
        OK &= Tutorial.Step==S::Complete && bInBackpack && Tutorial.bRoughLanding;
        Shot(TEXT("tutorial-complete.png"));
        break;
    }
    case 8:
    {
        TArray<FString> CurrentSection;
        GConfig->GetSection(TEXT("SimPlaneur.Weather"),CurrentSection,GGameUserSettingsIni); CurrentSection.Sort();
        OK &= CurrentSection==OriginalWeatherSection;
        OpenMenu(); OK &= Click(TEXT("Tutorial")) && Click(TEXT("TutorialExit"));
        OK &= !IsTutorialActive() && SameTutorialWeather(ValleyAir::Settings(),OriginalWeather)
            && SelectedModel==OriginalModel && PreviewModel==OriginalPreview && SelectedSite==OriginalSite
            && PilotSite.Equals(OriginalPilot,.01) && PilotOffset.Equals(OriginalOffset,.01)
            && bShowWindTrails==OriginalTrails;
        break;
    }
    default:
        UE_LOG(LogTemp,Display,TEXT("TUTORIAL_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_PROOF phase=%d success=%d stage=%d airtime=%.2f wind=%.2f recovery=%d"),
        Phase,OK,int(Tutorial.Step),Tutorial.FlightSeconds,ValleyAir::Settings().WindSpeedMps,Tutorial.bRecoveryNeeded);
    if(!OK) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
    ++Phase; NextAt=Elapsed+1.2f;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFlightControlCheck,"SimPlaneur.Tutorial.ControlsRequireFourDirectionsAndNeutral",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstFlightControlCheck::RunTest(const FString& Parameters)
{
    FFirstFlightTutorial State;
    State.ObserveControls(0,-.9f);
    TestTrue(TEXT("Negative elevator is pull / cabrer"),State.ElevatorUp);
    TestFalse(TEXT("Pull alone must not validate push / piquer"),State.ElevatorDown);
    State=FFirstFlightTutorial();
    State.ObserveControls(0,.9f);
    TestTrue(TEXT("Positive elevator is push / piquer"),State.ElevatorDown);
    TestFalse(TEXT("Push alone must not validate pull / cabrer"),State.ElevatorUp);
    State=FFirstFlightTutorial();
    State.ObserveControls(.1f,-.1f);
    TestFalse(TEXT("Noise around centre cannot validate the controls"),State.ControlsReady());
    State.ObserveControls(.9f,.9f); State.ObserveControls(-.9f,-.9f);
    TestFalse(TEXT("Return both controls to centre before launch"),State.ControlsReady());
    State.ObserveControls(0,0);
    TestTrue(TEXT("Four excursions followed by neutral validate"),State.ControlsReady());
    State.FlightSeconds=93.f; State.LostSeconds=8.f; State.StageSeconds=10.f;
    State.Go(EFirstFlightStep::Launch);
    TestEqual(TEXT("Recovery preserves accumulated airtime"),State.FlightSeconds,93.f);
    TestEqual(TEXT("New stage resets visibility grace"),State.LostSeconds,0.f);
    TestEqual(TEXT("New stage resets its own timer"),State.StageSeconds,0.f);
    // Anchor tutorial terminology to the actual Topy dynamics, not only its flags.
    FGliderDynamics Neutral;
    if(!TestTrue(TEXT("Load the Topy flight model"),Neutral.LoadModel(FGliderDynamics::TopyModelPath()))) return false;
    Neutral.Reset(FVector::ZeroVector);
    Neutral.LaunchFrom(FTransform(FRotator::ZeroRotator,FVector(0,0,10000)),FVector(8,0,0));
    FGliderDynamics Pull=Neutral, Push=Neutral;
    for(int32 I=0;I<60;++I) { Neutral.Step(0,0); Pull.Step(-.4,0); Push.Step(.4,0); }
    TestTrue(TEXT("Negative elevator raises the real Topy nose"),Pull.WorldPose().Rotator().Pitch>Neutral.WorldPose().Rotator().Pitch+1.);
    TestTrue(TEXT("Positive elevator lowers the real Topy nose"),Push.WorldPose().Rotator().Pitch<Neutral.WorldPose().Rotator().Pitch-1.);
    return !HasAnyErrors();
}
#endif
