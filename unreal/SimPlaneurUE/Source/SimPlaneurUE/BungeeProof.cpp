#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "SlopeWorld.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Camera/CameraTypes.h"
#include "FlightMenu.h"

void ASlopeFlightPawn::TestBungee(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if(!bAppearanceReady || Elapsed < 5.f+MenuProofStep*2.f) return;
    using State=FBungeeLaunch::EState;
    bool OK=true;
    auto Place=[&](FVector2D P)
    {
        FHitResult Ground;
        if(!FindPilotGround(FVector(P,0),Ground)) { OK=false; return; }
        PilotSite=Ground.ImpactPoint;
        PilotOffset=FVector2D::ZeroVector;
    };
    auto Capture=[&](const TCHAR* Name)
    {
        // Scripted actions run after the normal pawn tick. Synchronize the held
        // aircraft and cable before capturing the new pilot position.
        if (bHeldForLaunch) AdvanceFlight(0.f);
        UpdateEquipment(0.f);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false,false);
    };
    switch(MenuProofStep)
    {
    case 0:
        Bungee=FBungeeLaunch();
        LaunchInstallations=FLaunchInstallations(); ActiveBungeeId=INDEX_NONE;
        LaunchInstallations.Save(GGameUserSettingsIni,ActiveBungeeId);
        BungeeColourIndex=0;
        CycleBungeeColour(1);
        BungeeColourIndex=0;
        LoadBungee();
        OK &= BungeeColourIndex==1;
        SelectedModel=PreviewModel=0;
        Flight=HangarModels[0];
        BuildFallbackTopy();
        Place(Airfield::Point(3250,0));
        LaunchHeading=135.f;
        WalkPitch=-34.f;
        ResetFlight();
        SelectBungee();
        UpdateEquipment(0.f);
        HandleEquipmentInput(false,true,true,.05f); // quick E tap within one frame
        OK &= Bungee.State==State::LayingOut && bInBackpack;
        break;
    case 1:
        Place(Airfield::Point(500,0)); // choose the departure before tensioning
        HandleEquipmentInput(false,true,true,.05f);
        OK &= Bungee.State==State::Ready;
        MenuProofLocation=Bungee.Departure;
        Bungee=FBungeeLaunch();
        LoadBungee();
        OK &= Bungee.State==State::Ready && Bungee.Departure.Equals(MenuProofLocation,.1);
        Capture(TEXT("bungee-layout.png"));
        break;
    case 2:
        Place(Airfield::Point(-500,0));
        OK &= Bungee.State==State::Ready && EquipmentLineEnd().Equals(MenuProofLocation,.1);
        ToggleBackpack(); // F3 away from the departure must not attach remotely
        OK &= !bInBackpack && Bungee.State==State::Ready && EquipmentLineEnd().Equals(MenuProofLocation,.1);
        ToggleBackpack();
        Place(FVector2D(MenuProofLocation));
        ToggleBackpack(); // F3 at the departure takes out AND attaches the glider
        OK &= Bungee.State==State::Attached && !bInBackpack && BungeeTension()==0.;
        Place(Airfield::Point(-3500,0));
        OK &= Bungee.Departure.Equals(MenuProofLocation,.1) && BungeeTension()>0.
            && EquipmentLineEnd().Equals(BungeeHandPose().GetLocation(),.1);
        Capture(TEXT("bungee-dial.png"));
        break;
    case 3:
    {
        const FVector PreviousDeparture=Bungee.Departure;
        Place(Airfield::Point(-3800,0));
        LaunchHeading=(Bungee.Anchor-PilotPosition()).Rotation().Yaw;
        BeginHandLaunch();
        OK &= Bungee.State==State::Towing && !bHeldForLaunch && FVector::Distance(Bungee.Departure,PreviousDeparture)>200.;
        OK &= LaunchInstallations.Find(ActiveBungeeId) && LaunchInstallations.Find(ActiveBungeeId)->Launches==1;
        MenuProofLocation=Bungee.Departure;
        Elevator=-.2f;
        break;
    }
    case 4:
        OK &= !Flight.IsStopped() && Flight.Altitude()>MenuProofLocation.Z*.01+4.;
        Capture(TEXT("bungee-flight.png"));
        break;
    case 5:
    {
        Bungee.Release();
        Place(Airfield::Point(-2000,0));
        const FTransform Landing(FRotator::ZeroRotator,PilotPosition()+FVector(1500,500,30));
        const double ExpectedYaw=(Landing.GetLocation()-PilotPosition()).Rotation().Yaw;
        LaunchHeading=ExpectedYaw+120.;
        StopFlightAt(Landing);
        SetActorTransform(Flight.WorldPose());
        FMinimalViewInfo View;
        CalcCamera(0.f,View);
        OK &= FMath::Abs(FMath::FindDeltaAngleDegrees(View.Rotation.Yaw,ExpectedYaw))<.01;
        Place(FVector2D(Landing.GetLocation()));
        ToggleBackpack();
        OK &= bInBackpack && bHeldForLaunch && Bungee.State==State::Ready;
        Place(FVector2D(MenuProofLocation));
        HandleEquipmentInput(true,true,false,.1f);
        HandleEquipmentInput(false,false,true,.1f);
        OK &= Bungee.State==State::Attached && !bInBackpack;
        break;
    }
    case 6:
        Bungee.Release();
        Bungee=FBungeeLaunch();
        LoadBungee();
        OK &= Bungee.State==State::Ready && Bungee.Departure.Equals(MenuProofLocation,.1);
        HandleEquipmentInput(true,true,false,.6f);
        HandleEquipmentInput(true,false,false,.6f);
        HandleEquipmentInput(false,false,true,.05f);
        OK &= !Bungee.Installed() && LaunchInstallations.Entries().IsEmpty();
        break;
    case 7:
        Bungee.PlaceAnchor(PilotPosition());
        HandleEquipmentInput(true,true,false,.6f);
        HandleEquipmentInput(true,false,false,.6f);
        HandleEquipmentInput(false,false,true,.05f);
        OK &= !Bungee.Installed();
        break;
    case 8:
    {
        // Two distant sites must remain available. Returning to an older site
        // selects that installation, not the last one laid out elsewhere.
        auto EntryAt=[&](FVector2D Anchor,FVector2D Departure)
        {
            FLaunchInstallation E;
            E.Anchor=FVector(Anchor,ASlopeWorld::TerrainHeightCm(Anchor.X,Anchor.Y));
            E.Departure=FVector(Departure,ASlopeWorld::TerrainHeightCm(Departure.X,Departure.Y));
            E.Stiffness=.5; E.Colour=2;
            return E;
        };
        const auto First=EntryAt(Airfield::Point(3500,0),Airfield::Point(-3500,0));
        const int32 FirstId=LaunchInstallations.Place(First).Id;
        const FVector2D Hill(ASlopeWorld::PilotX,ASlopeWorld::PilotY);
        const int32 SecondId=LaunchInstallations.Place(EntryAt(Hill,Hill+FVector2D(5000,0))).Id;
        ActivateBungee(SecondId);
        Place(FVector2D(First.Departure));
        SelectNearbyBungee();
        InteractEquipment(false);
        OK &= ActiveBungeeId==FirstId && Bungee.State==State::Attached && LaunchInstallations.Entries().Num()==2;
        SaveBungee();
        Capture(TEXT("bungee-multiple-sites.png"));
        break;
    }
    case 9:
        Bungee.Release();
        LoadBungee();
        OK &= LaunchInstallations.Entries().Num()==2 && Bungee.State==State::Ready;
        OpenMenu();
        if(Menu.IsValid()) Menu->OpenMapFromFlight(); else OK=false;
        break;
    case 10:
        Capture(TEXT("bungee-installations-map.png"));
        break;
    default:
        UE_LOG(LogTemp,Display,TEXT("BUNGEE_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0);
        return;
    }
    UE_LOG(LogTemp,Display,TEXT("BUNGEE_PROOF step=%d success=%d tension=%.2f speed=%.2f height=%.2f stopped=%d"),MenuProofStep,OK,BungeeTension(),Flight.Airspeed(),Flight.Altitude()-MenuProofLocation.Z*.01,Flight.IsStopped());
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
