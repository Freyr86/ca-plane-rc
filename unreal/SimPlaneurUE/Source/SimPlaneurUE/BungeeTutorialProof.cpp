#include "SlopeFlightPawn.h"
#if !UE_BUILD_SHIPPING
#include "FlightMenu.h"
#include "Airfield.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Components/SceneComponent.h"
namespace
{
    TSharedPtr<SWidget> BungeeLessonWidget(const TSharedRef<SWidget>& Widget,FName Tag)
    {
        if(Widget->GetTag()==Tag) return Widget;
        FChildren* Children=Widget->GetChildren();
        for(int32 I=0;Children && I<Children->Num();++I)
            if(auto Found=BungeeLessonWidget(Children->GetChildAt(I),Tag)) return Found;
        return nullptr;
    }
}
#endif

void ASlopeFlightPawn::TestBungeeTutorial(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    using S=EBungeeTutorialStep; using B=FBungeeLaunch::EState;
    static int Phase=0, SavedId=INDEX_NONE, SavedModel=0;
    static float NextAt=5.f;
    static ValleyAir::FWeatherSettings SavedWeather;
    if(Elapsed>100.f) { UE_LOG(LogTemp,Error,TEXT("BUNGEE_TUTORIAL_PROOF timeout phase=%d"),Phase); FPlatformMisc::RequestExitWithStatus(false,3); return; }
    if(!bAppearanceReady || Elapsed<NextAt || FScreenshotRequest::IsScreenshotRequested()) return;
    bool OK=true;
    auto Click=[&](FName Tag)
    {
        auto W=Menu.IsValid()?BungeeLessonWidget(Menu.ToSharedRef(),Tag):nullptr;
        if(!W || !W->IsEnabled() || W->GetType()!=TEXT("SButton")) return false;
        StaticCastSharedPtr<SButton>(W)->SimulateClick(); return true;
    };
    auto Shot=[](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false,false); };
    auto Move=[&](FVector Point)
    {
        FHitResult Ground;
        if(!FindPilotGround(Point,Ground)) { OK=false; return; }
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
        if(bHeldForLaunch) AdvanceFlight(0.f);
    };
    auto Advance=[&](){UpdateFirstFlightTutorial(.05f);};
    auto SavedWorldIntact=[&]()
    {
        FLaunchInstallations Saved; int Id=INDEX_NONE;
        return Saved.Load(GGameUserSettingsIni,Id) && Id==SavedId && Saved.Entries().Num()==1
            && Saved.Find(SavedId) && Saved.Find(SavedId)->Launches==3;
    };
    switch(Phase)
    {
    case 0:
    {
        // A saved installation in the lesson area must survive every lesson save.
        LaunchInstallations=FLaunchInstallations();
        FLaunchInstallation E; E.Anchor=FVector(Airfield::Point(3500,0),100); E.Departure=FVector(Airfield::Point(-3500,0),100);
        SavedId=LaunchInstallations.Place(E).Id;
        for(int I=0;I<3;++I) LaunchInstallations.RecordLaunch(SavedId);
        ActivateBungee(SavedId); SaveBungee();
        SavedWeather=ValleyAir::Settings(); SavedModel=SelectedModel;
        OK=Click(TEXT("Tutorial")); Shot(TEXT("bungee-tutorial-menu.png")); break;
    }
    case 1:
        OK=Click(TEXT("BungeeTutorialStart")) && IsBungeeTutorialActive() && SelectedModel==7
            && Tutorial.BungeeStep==S::Select && LaunchInstallations.Entries().IsEmpty() && SavedWorldIntact(); break;
    case 2:
        OK=!bMenuOpen && bInBackpack && !Flight.HasRudder() && Flight.HasAilerons();
        BeginHandLaunch(); OK &= bHeldForLaunch && !IsThrowing();
        SelectLaunchEquipment(ELaunchEquipment::Winch); OK &= !Bungee.IsWinch();
        SelectBungee(); Advance(); OK &= Tutorial.BungeeStep==S::Anchor;
        UpdateEquipment(0.f); InteractEquipment(false); Advance();
        OK &= Tutorial.BungeeStep==S::Layout && Bungee.State==B::LayingOut;
        Shot(TEXT("bungee-tutorial-layout.png")); break;
    case 3:
        Move(Tutorial.BungeeDepartureTarget); InteractEquipment(false); Advance();
        OK=OK && Tutorial.BungeeStep==S::Attach && SavedWorldIntact();
        InteractEquipment(false); Advance(); OK &= Tutorial.BungeeStep==S::Tension && BungeeTension()==0.;
        BeginHandLaunch(); OK &= bHeldForLaunch;
        Shot(TEXT("bungee-tutorial-slack.png")); break;
    case 4:
        Move(Tutorial.LaunchPoint); LaunchHeading=TutorialHeading; Advance();
        OK=OK && Tutorial.BungeeStep==S::Launch && BungeeTutorialReady();
        LaunchHeading+=90; BeginHandLaunch(); Advance();
        OK &= bHeldForLaunch && Tutorial.BungeeStep==S::Tension;
        LaunchHeading=TutorialHeading; Advance(); OK &= Tutorial.BungeeStep==S::Launch;
        Shot(TEXT("bungee-tutorial-tension.png")); break;
    case 5:
    {
        const float Seconds=Tutorial.StageSeconds;
        OpenMenu(); Advance(); OK=Tutorial.StageSeconds==Seconds;
        Fly(); BeginHandLaunch(); Advance();
        OK &= Bungee.State==B::Towing && Tutorial.BungeeStep==S::Tow && !bHeldForLaunch && SavedWorldIntact();
        Elevator=-.2f; Shot(TEXT("bungee-tutorial-tow.png")); break;
    }
    case 6:
        Bungee.Release(); Advance(); OK=Tutorial.BungeeStep==S::Glide;
        ImpactDamage=2; ContactKind=TEXT("tree"); Advance(); OK &= Tutorial.bRecoveryNeeded && Tutorial.bCrashed;
        RecoverFirstFlightTutorial();
        OK &= Tutorial.BungeeStep==S::Attach && !Tutorial.bRecoveryNeeded && Bungee.State==B::Ready && SavedWorldIntact();
        InteractEquipment(false); Advance(); Move(Tutorial.LaunchPoint); LaunchHeading=TutorialHeading; Advance();
        BeginHandLaunch(); Advance(); OK &= Tutorial.BungeeStep==S::Tow;
        break;
    case 7:
    {
        Bungee.Release(); Advance(); OK=Tutorial.BungeeStep==S::Glide;
        const FTransform Land(FRotator(0,TutorialHeading,0),PilotPosition()+FVector(100,0,20));
        StopFlightAt(Land); ClearGroundPhysics(); ImpactDamage=0; ContactKind=TEXT("ground"); SetActorTransform(Flight.WorldPose()); Advance();
        OK &= Tutorial.BungeeStep==S::Retrieve;
        Move(Tutorial.LandingTarget); ToggleBackpack(); Advance(); OK &= Tutorial.BungeeStep==S::Pack;
        Move(Bungee.Departure);
        HandleEquipmentInput(true,true,false,.6f); HandleEquipmentInput(true,false,false,.6f); HandleEquipmentInput(false,false,true,.05f);
        Advance(); OK &= Tutorial.BungeeStep==S::Complete && !Bungee.Installed() && !bBungeeSelected && SavedWorldIntact();
        Shot(TEXT("bungee-tutorial-complete.png")); break;
    }
    case 8:
        OpenMenu(); EndFirstFlightTutorial();
        OK=!IsTutorialActive() && SelectedModel==SavedModel && LaunchInstallations.Entries().Num()==1
            && ActiveBungeeId==SavedId && LaunchInstallations.Find(SavedId)->Launches==3 && SavedWorldIntact()
            && FMath::IsNearlyEqual(ValleyAir::Settings().WindSpeedMps,SavedWeather.WindSpeedMps);
        break;
    case 9:
        // Wait for restored model textures before switching lessons.
        StartFirstFlightTutorial(); OK=Tutorial.Step==EFirstFlightStep::Walk; break;
    case 10:
        OpenMenu(); StartBungeeTutorial(); OK=IsBungeeTutorialActive() && SavedWorldIntact(); break;
    case 11:
        OpenMenu(); EndFirstFlightTutorial();
        OK=!IsTutorialActive() && SavedWorldIntact() && ActiveBungeeId==SavedId;
        UE_LOG(LogTemp,Display,TEXT("BUNGEE_TUTORIAL_PROOF complete success=%d"),OK);
        FPlatformMisc::RequestExitWithStatus(false,OK?0:3); break;
    default: OK=false;
    }
    UE_LOG(LogTemp,Display,TEXT("BUNGEE_TUTORIAL_PROOF phase=%d success=%d stage=%d tension=%.2f"),Phase,OK,int(Tutorial.BungeeStep),BungeeTension());
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++Phase; NextAt=Elapsed+1.5f;
#endif
}
