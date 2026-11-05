#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "WinchStation.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "FlightMenu.h"

void ASlopeFlightPawn::TestWinch(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if(!bHeldForLaunch && !Flight.IsStopped())
        Elevator=FMath::Clamp((Flight.WorldPose().Rotator().Pitch-(Bungee.Connected()?45.:5.))*.025,-.65,.65);
    if(!bAppearanceReady || Elapsed<5.f+MenuProofStep*2.f) return;
    using State=FBungeeLaunch::EState;
    bool OK=true;
    auto Place=[&](FVector2D P)
    {
        FHitResult Ground;
        if(!FindPilotGround(FVector(P,0),Ground)) {OK=false;return;}
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
    };
    auto Tap=[&]{HandleEquipmentInput(false,true,true,.05f);};
    auto Capture=[&](const TCHAR* Name)
    {
        if(bHeldForLaunch) AdvanceFlight(0.f);
        UpdateEquipment(0.f);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false,false);
    };
    switch(MenuProofStep)
    {
    case 0:
        Bungee=FBungeeLaunch(); LaunchInstallations=FLaunchInstallations(); ActiveBungeeId=INDEX_NONE;
        SaveBungee(); SetWinchPowerLimit(1.f);
        SelectedModel=PreviewModel=0; Flight=HangarModels[0]; BuildFallbackTopy();
        Place(Airfield::Point(6200,0)); LaunchHeading=135; WalkPitch=-34; ResetFlight();
        SelectLaunchEquipment(ELaunchEquipment::Winch); UpdateEquipment(0.f); Tap();
        OK &= Bungee.IsWinch() && Bungee.State==State::LayingOut && bInBackpack;
        break;
    case 1:
        Place(Airfield::Point(-6200,0)); Tap();
        OK &= Bungee.State==State::Ready && LaunchInstallations.Count(ELaunchEquipment::Winch)==1;
        MenuProofLocation=Bungee.Departure; Bungee=FBungeeLaunch(); LoadBungee();
        OK &= Bungee.IsWinch() && Bungee.State==State::Ready && WinchStations.Num()==1;
        break;
    case 2:
        Place(FVector2D(Bungee.Anchor)+FVector2D(450,-450));
        LaunchHeading=(Bungee.Anchor-PilotPosition()).Rotation().Yaw; WalkPitch=-17;
        Capture(TEXT("winch-station.png"));
        break;
    case 3:
        Place(FVector2D(MenuProofLocation)); ToggleBackpack();
        OK &= Bungee.State==State::Attached && !bInBackpack && BungeeTension()==0.;
        Capture(TEXT("winch-ready.png"));
        break;
    case 4:
        BeginHandLaunch();
        OK &= Bungee.State==State::Towing && !bHeldForLaunch;
        OK &= LaunchInstallations.Find(ActiveBungeeId)->Launches==1;
        break;
    case 5:
        OK &= !Flight.IsStopped() && Flight.Altitude()>MenuProofLocation.Z*.01+4.;
        Capture(TEXT("winch-climb.png"));
        break;
    case 6: case 7: case 8:
        OK &= !Flight.IsStopped();
        break;
    case 9:
        OK &= Bungee.State==State::Ready;
        ResetFlight(); Place(FVector2D(MenuProofLocation)); Tap();
        OK &= Bungee.State==State::Attached;
        Bungee.Release(); LoadBungee();
        OK &= Bungee.State==State::Ready && Bungee.IsWinch() && LaunchInstallations.Find(ActiveBungeeId)->Launches==1;
        OpenMenu(); if(Menu.IsValid()) Menu->OpenMapFromFlight(); else OK=false;
        break;
    case 10:
        Capture(TEXT("winch-map.png"));
        break;
    case 11:
        Fly(); ResetFlight(); Place(FVector2D(Bungee.Anchor)+FVector2D(150,0));
        HandleEquipmentInput(true,true,false,.6f); HandleEquipmentInput(true,false,false,.6f);
        HandleEquipmentInput(false,false,true,.01f);
        OK &= !Bungee.Installed() && WinchStations.IsEmpty() && LaunchInstallations.Entries().IsEmpty();
        break;
    default:
        UE_LOG(LogTemp,Display,TEXT("WINCH_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    UE_LOG(LogTemp,Display,TEXT("WINCH_PROOF step=%d success=%d tension=%.2f reel=%.2f paid=%.1f speed=%.2f height=%.2f state=%d stopped=%d notice=%s"),
        MenuProofStep,OK,BungeeTension(),Bungee.Winch.ReelSpeed,Bungee.Winch.PaidOut,Flight.Airspeed(),Flight.Altitude()-MenuProofLocation.Z*.01,int32(Bungee.State),Flight.IsStopped(),*SiteNotice);
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
