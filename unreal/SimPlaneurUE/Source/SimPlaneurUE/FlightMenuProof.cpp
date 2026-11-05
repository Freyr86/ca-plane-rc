#include "SlopeFlightPawn.h"
#if !UE_BUILD_SHIPPING
#include "FlightMenu.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "InputKeyEventArgs.h"
#include "SlopeWorld.h"
#include "Airfield.h"
#include "CirqueSite.h"
#include "SceneContact.h"
#include "EngineUtils.h"
#include "ValleyLandscape.h"
#include "Camera/CameraTypes.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
    TSharedPtr<SWidget> FindTagged(const TSharedRef<SWidget>& Widget,FName Tag)
    {
        if (Widget->GetTag()==Tag) return Widget;
        FChildren* Children=Widget->GetChildren();
        for (int32 I=0; Children && I<Children->Num(); ++I)
            if (auto Found=FindTagged(Children->GetChildAt(I),Tag)) return Found;
        return nullptr;
    }
}
#endif

void ASlopeFlightPawn::TestMenu(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if (Menu.IsValid() && Menu->IsShowingStartupPoster()) return;
    const bool GalleryProof = FParse::Param(FCommandLine::Get(),TEXT("CoverGalleryProof"));
    if (Elapsed<5.f || (!GalleryProof && Elapsed<5.f+MenuProofStep*1.3f)) return;
    auto Shot=[](const TCHAR* Name)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),Name),true,false,false);
    };
    auto Click=[this](FName Tag)
    {
        auto B=Menu.IsValid() ? FindTagged(Menu.ToSharedRef(),Tag) : nullptr;
        // Buttons below the scrollbox viewport still expose their bound actions.
        if (!B.IsValid() || !B->IsEnabled()) return false;
        if (B->GetType()!=TEXT("SButton")) return false;
        StaticCastSharedPtr<SButton>(B)->SimulateClick();
        return true;
    };
    auto SendKey=[this](FKey Input,EInputEvent Event)
    {
        CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
            nullptr,FInputDeviceId::CreateFromInternalId(0),Input,Event,FPlatformTime::Cycles64()));
    };
    if (FParse::Param(FCommandLine::Get(),TEXT("EquipmentProof")))
    {
        bool OK=true;
        switch(MenuProofStep)
        {
        case 0: OK=Click(TEXT("Settings")) && Click(TEXT("Equipment")); MenuProofMass=BungeeColourIndex; break;
        case 1: OK=Click(TEXT("COULEUR+")) && BungeeColourIndex!=MenuProofMass; MenuProofMass=BungeeColourIndex; break;
        case 2: Shot(TEXT("bungee-colour-settings.png")); break;
        case 3:
            BungeeColourIndex=-1;
            LoadBungee();
            OK=BungeeColourIndex==MenuProofMass && Click(TEXT("Back"));
            break;
        default: UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_MENU complete")); FPlatformMisc::RequestExit(false); return;
        }
        UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_MENU step=%d success=%d"),MenuProofStep,OK);
        if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
        ++MenuProofStep;
        return;
    }
    if (GalleryProof)
    {
        static constexpr struct { const TCHAR* Mesh; const TCHAR* Filename; } Covers[] = {
            {TEXT("TopyCouleur"),TEXT("gallery-topy.png")}, {TEXT("K3600Couleur"),TEXT("gallery-k3600.png")},
            {TEXT("VJX2022"),TEXT("gallery-vjx-2022.png")}, {TEXT("Fox7mEffile"),TEXT("gallery-fox-7m.png")},
            {TEXT("JS3_9m"),TEXT("gallery-js3-9m.png")}, {TEXT("Snipe2"),TEXT("gallery-snipe-2.png")},
            {TEXT("LongBowMk2"),TEXT("gallery-longbow-mk2.png")}, {TEXT("Apus4"),TEXT("gallery-apus-4.png")},
            {TEXT("Nubis"),TEXT("gallery-nubis.png")}
        };
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        const int32 Model = MenuProofStep/2;
        if (Model==UE_ARRAY_COUNT(Covers)) { FPlatformMisc::RequestExit(false); return; }
        if (Model==6 && !AvailableModels[Model]) { MenuProofStep+=2; return; }
        if (MenuProofStep%2==0)
        {
            const bool Selected = Model==0 ? Click(TEXT("Hangar")) : Click(TEXT("MODÈLE+"));
            if (!Selected || ModelIndex()!=Model || !CanSelectModel()) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
            MenuProofWeather=Elapsed;
            ++MenuProofStep;
            return;
        }
        const UStaticMesh* Mesh=ImportedTopy->GetStaticMesh();
        if (!Mesh || Mesh->GetName()!=Covers[Model].Mesh || Elapsed-MenuProofWeather>30.)
        {
            UE_LOG(LogTemp,Error,TEXT("GALLERY_PROOF missing or unready mesh: %s"),Covers[Model].Mesh);
            FPlatformMisc::RequestExitWithStatus(false,3); return;
        }
        // Keep the actual mesh, materials and hangar lighting. Frame each span in the same 64° view.
        TourTime=14.;
        const FTransform View=MenuCameraPose();
        const FQuat Pose=FRotator(-6.,150.,20.).Quaternion();
        const FBox Bounds=Mesh->GetBoundingBox();
        const FTransform MeshToPlane=ImportedTopy->GetRelativeTransform();
        const FVector HalfWing=Pose.RotateVector(MeshToPlane.TransformVector(FVector(0.,Bounds.GetExtent().Y,0.)));
        const double Distance=FMath::Abs(HalfWing.Y)/(.65*FMath::Tan(FMath::DegreesToRadians(32.)));
        const FVector Centre(Distance,HalfWing.Y*HalfWing.X/Distance,HalfWing.Z*HalfWing.X/Distance);
        const FVector Origin=Centre-Pose.RotateVector(MeshToPlane.TransformPosition(Bounds.GetCenter()));
        PlaneVisual->SetWorldLocationAndRotation(View.TransformPosition(Origin),View.GetRotation()*Pose);
        if (!IsAppearanceReady() || Elapsed-MenuProofWeather<2.) return;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),Covers[Model].Filename),false,false,false);
        UE_LOG(LogTemp,Display,TEXT("GALLERY_PROOF %s %s"),ModelName(),Covers[Model].Filename);
        ++MenuProofStep;
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("NavigationProof")))
    {
        bool OK=true;
        switch(MenuProofStep)
        {
        case 0: Shot(TEXT("navigation-prairie.png")); break;
        case 1: OK=Click(TEXT("SITE DE VOL+")) && LaunchSiteNumber()==2; break;
        case 2:
        {
            const FVector Mast(ASlopeWorld::ChaletX+950,ASlopeWorld::ChaletY+450,0);
            FHitResult Ground,Top;
            OK=FindPilotGround(Mast,Ground) && Ground.GetComponent()->ComponentHasTag(TEXT("ground"))
                && GetWorld()->LineTraceSingleByChannel(Top,Ground.ImpactPoint+FVector(0,0,600),
                    Ground.ImpactPoint,SceneContact::Channel,FCollisionQueryParams(SCENE_QUERY_STAT(ChaletMast),false,this))
                && Top.GetComponent()->ComponentHasTag(TEXT("mast"));
            const double Height=Top.ImpactPoint.Z-Ground.ImpactPoint.Z;
            OK=OK && FMath::IsNearlyEqual(Height,370.,1.);
            UE_LOG(LogTemp,Display,TEXT("CHALET_MAST height_cm=%.1f old_ground_offset_cm=%.1f"),
                Height,ASlopeWorld::TerrainHeightCm(Mast.X,Mast.Y)-Ground.ImpactPoint.Z);
            Shot(TEXT("navigation-chalet.png")); break;
        }
        case 3: OK=Click(TEXT("OpenMap")); break;
        case 4: OK=Click(TEXT("MapIn")) && Click(TEXT("MapCentre")); break;
        case 5: Shot(TEXT("navigation-atlas.png")); break;
        case 6: OK=Click(TEXT("MapOut")) && Click(TEXT("Back")) && Click(TEXT("Hangar")); break;
        case 7: OK=Click(TEXT("ZOOM DU PLANEUR+")) && HangarZoom>1.f; break;
        case 8:
            OK=Click(TEXT("ZOOM DU PLANEUR-")) && FMath::IsNearlyEqual(HangarZoom,1.f);
            for (int32 I=0; OK && ModelIndex()!=7 && I<UE_ARRAY_COUNT(HangarModels); ++I) OK=Click(TEXT("MODÈLE-"));
            OK=OK && ModelIndex()==7 && Click(TEXT("SelectAircraft")); break;
        case 9:
            if (!IsAppearanceReady() && Elapsed<45.f) return;
            OK=bPosterHome && IsAppearanceReady() && FString(ModelName())==TEXT("FEMO APUS 4") && ImportedTopy->GetStaticMesh() &&
                ImportedTopy->GetStaticMesh()->GetPathName()==TEXT("/Game/Aircraft/Apus4/Apus4.Apus4");
            Shot(TEXT("accueil-apus-selectionne.png")); break;
        case 10: OK=Click(TEXT("Hangar")) && Click(TEXT("MODÈLE+")) && Click(TEXT("Back")) && ModelIndex()==7; break;
        case 11:
            if (!IsAppearanceReady() && Elapsed<50.f) return;
            OK=ImportedTopy->GetStaticMesh() && ImportedTopy->GetStaticMesh()->GetName()==TEXT("Apus4") && Click(TEXT("Fly")); break;
        case 12: SendKey(EKeys::F2,IE_Pressed); break;
        case 13: OK=IsMenuOpen() && Click(TEXT("Back")) && !IsMenuOpen(); OpenMenu(); break;
        case 14:
        {
            OK=Click(TEXT("SITE DE VOL+")) && LaunchSiteNumber()==3
                && FVector2D::Distance(FVector2D(PilotPosition()),Airfield::Pilot)<1.;
            const double Grade=ASlopeWorld::TerrainHeightCm(Airfield::Centre.X,Airfield::Centre.Y);
            auto Surface=[&](FVector2D P,FName Kind)
            {
                FHitResult Hit;
                return GetWorld()->LineTraceSingleByChannel(Hit,FVector(P,Grade+2200),FVector(P,Grade-1000),
                    SceneContact::Channel,FCollisionQueryParams(SCENE_QUERY_STAT(AirfieldProof),false,this))
                    && Hit.GetComponent()->ComponentHasTag(Kind)
                    && (Kind!=TEXT("ground") || FMath::Abs(Hit.ImpactPoint.Z-Grade)<2.);
            };
            for (double Along : {-Airfield::HalfLength,0.,Airfield::HalfLength})
                for (double Side : {-Airfield::HalfWidth,0.,Airfield::HalfWidth})
                    OK=OK && Surface(Airfield::Point(Along,Side),TEXT("ground"));
            OK=OK && Surface(Airfield::Hut,TEXT("hut")) && Surface(Airfield::Sock,TEXT("mast"));
            break;
        }
        case 15: Shot(TEXT("site-03-accueil.png")); break;
        case 16: OK=Click(TEXT("OpenMap")) && Click(TEXT("MapIn")) && Click(TEXT("MapIn")); break;
        case 17: Shot(TEXT("site-03-carte.png")); break;
        case 18:
            OK=Click(TEXT("Back")) && Click(TEXT("Explore")) && IsNoclip();
            NoclipPosition=FVector(Airfield::Point(-5300,-13500),PilotSite.Z+7300);
            NoclipRotation=(FVector(Airfield::Centre,PilotSite.Z)-NoclipPosition).Rotation(); break;
        case 19: Shot(TEXT("site-03-piste.png")); break;
        case 20:
            NoclipPosition=FVector(Airfield::Point(-950,350),PilotSite.Z+390);
            NoclipRotation=(FVector(Airfield::Point(200,Airfield::HutSide),PilotSite.Z+165)-NoclipPosition).Rotation(); break;
        case 21: Shot(TEXT("site-03-cabanon.png")); break;
        case 22:
        {
            OpenMenu();
            OK=Click(TEXT("SITE DE VOL+")) && LaunchSiteNumber()==4
                && FVector2D::Distance(FVector2D(PilotPosition()),CirqueSite::Pilot)<1.
                && FMath::Abs(FMath::FindDeltaAngleDegrees(LaunchHeading,CirqueSite::Heading))<.01f;
            const FTransform First=MenuCameraPose();
            const double SavedTime=TourTime;
            TourTime+=12.;
            const FTransform Later=MenuCameraPose();
            TourTime=SavedTime;
            const FVector Eye=First.GetLocation(),NextEye=Later.GetLocation();
            OK=OK && FVector::Dist(Eye,NextEye)>100.
                && Eye.Z>ASlopeWorld::TerrainHeightCm(Eye.X,Eye.Y)+849.
                && NextEye.Z>ASlopeWorld::TerrainHeightCm(NextEye.X,NextEye.Y)+849.;
            break;
        }
        case 23: Shot(TEXT("site-04-accueil.png")); break;
        case 24: OK=Click(TEXT("OpenMap")) && Click(TEXT("MapCentre")); break;
        case 25: Shot(TEXT("site-04-carte.png")); break;
        case 26: OK=Click(TEXT("Back")) && Click(TEXT("SITE DE VOL+")) && LaunchSiteNumber()==1
                && Click(TEXT("SITE DE VOL-")) && LaunchSiteNumber()==4
                && Click(TEXT("SITE DE VOL-")) && LaunchSiteNumber()==3 && Click(TEXT("Fly")) && IsWalking(); break;
        case 27: SendKey(EKeys::SpaceBar,IE_Pressed); break;
        case 28: OK=!IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone();
            SendKey(EKeys::SpaceBar,IE_Released); OpenMenu(); OK=OK && Click(TEXT("Credits")); break;
        case 29: Shot(TEXT("club-credits.png")); break;
        case 30: OK=Click(TEXT("Back")) && FindTagged(Menu.ToSharedRef(),TEXT("Credits")).IsValid();
            FPlatformMisc::RequestExitWithStatus(false,OK?0:3); break;
        }
        UE_LOG(LogTemp,Display,TEXT("NAVIGATION_TEST step=%d pass=%d"),MenuProofStep,OK);
        if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
        ++MenuProofStep;
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("FlightSoundProof")))
    {
        bool OK = true;
        switch (MenuProofStep)
        {
        case 0:
        {
            FString SettingsFile;
            OK = FParse::Value(FCommandLine::Get(),TEXT("GameUserSettingsINI="),SettingsFile) && Click(TEXT("Settings")) && Click(TEXT("Vario"));
            if (!OK) break;
            SetVario(false,.3f,-2.f);
            OK = Click(TEXT("SON DU VARIO+")) && Click(TEXT("SEUIL DE MONTÉE+")) && Click(TEXT("SEUIL DE DESCENTE+"))
                && VarioEnabled() && FMath::IsNearlyEqual(VarioClimb(),.4f) && FMath::IsNearlyEqual(VarioSink(),-1.9f)
                && Click(TEXT("Back")) && Click(TEXT("Hangar"));
            if (!OK) break;
            if (!Radio.EditingKeyboard()) Radio.CycleDevice(1);
            Radio.BeginLearn(EGliderControl::Motor,false);
            Radio.MenuKey(TEXT("M"),true);
            Radio.MenuKey(TEXT("M"),false);
            ApplyModelSettings(Loading(),UsesAileronSteering(),ControlGains(),FlapTravel(),{true,60});
            OK = Radio.EditingKeyboard() && Radio.Binding(EGliderControl::Motor).Plus==TEXT("M") && Click(TEXT("Back"));
            break;
        }
        case 1:
            OK = Click(TEXT("Fly")) && IsWalking() && WindAudio && WindAudio->IsPlaying() && VarioAudio && !VarioAudio->IsPlaying();
            SendKey(EKeys::Up,IE_Pressed);
            break;
        case 2:
            OK = Flight.TotalServoTravel()>.1 && ServoAudio && !ServoAudio->IsPlaying();
            SendKey(EKeys::Up,IE_Released);
            break;
        case 3:
            SendKey(EKeys::M,IE_Pressed);
            SendKey(EKeys::SpaceBar,IE_Pressed);
            break;
        case 4:
            OK = !IsHeldForLaunch() && Flight.MotorPowerWatts()>0 && MotorAudio && MotorAudio->IsPlaying()
                && WingAudio && WingAudio->IsPlaying();
            SendKey(EKeys::SpaceBar,IE_Released);
            SendKey(EKeys::M,IE_Released);
            SendKey(EKeys::C,IE_Pressed);
            SendKey(EKeys::K,IE_Pressed);
            break;
        case 5:
            OK = !VarioEnabled() && !VarioAudio->IsPlaying() && Flight.MotorPowerWatts()==0 && MotorAudio && !MotorAudio->IsPlaying() && FlightView==EFlightView::Chase;
            SendKey(EKeys::K,IE_Released);
            SendKey(EKeys::C,IE_Released);
            CycleCamera();
            OK = OK && FlightView==EFlightView::Wide;
            CycleCamera();
            break;
        case 6:
        {
            FMinimalViewInfo View;
            CalcCamera(0.f,View);
            OK = FlightView==EFlightView::FPV && View.Rotation.Equals(GetActorRotation(),.01)
                && GetActorTransform().InverseTransformPosition(View.Location).X > 0. && View.FOV==90.f;
            Shot(TEXT("fpv-flight.png"));
            SendKey(EKeys::C,IE_Pressed);
            break;
        }
        case 7:
            OK = FlightView==EFlightView::Ground;
            SendKey(EKeys::C,IE_Released);
            FPlatformMisc::RequestExitWithStatus(false,OK ? 0 : 3);
            break;
        }
        UE_LOG(LogTemp,Display,TEXT("FLIGHT_SOUND_PROOF step=%d pass=%d"),MenuProofStep,OK);
        if (!OK) FPlatformMisc::RequestExitWithStatus(false,3);
        ++MenuProofStep;
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("SpanProof")))
    {
        // Same pilot, pose, range and ground camera for every real flight mesh.
        const int32 Model = MenuProofStep / 4;
        if (Model==6 && !AvailableModels[Model])
        {
            MenuProofStep += 4;
            if (Model+1 == UE_ARRAY_COUNT(HangarModels)) FPlatformMisc::RequestExit(false);
            return;
        }
        bool OK = true;
        switch (MenuProofStep % 4)
        {
        case 0:
            OK = Click(TEXT("Hangar"));
            if (OK && Model > 0) OK = Click(TEXT("MODÈLE+"));
            OK = OK && ModelIndex()==Model && CanSelectModel();
            break;
        case 1:
        {
            if (!IsAppearanceReady() && Elapsed<60.f) return;
            OK = IsAppearanceReady() && Click(TEXT("SelectAircraft")) && Click(TEXT("Fly")) && !IsMenuOpen();
            bHeldForLaunch = false;
            const FTransform Pose(FRotator(0.,160.,14.),PilotPosition()+FVector(1800.,0.,520.));
            Flight.StopAt(Pose);
            PreviousPose = Pose;
            SetActorTransform(Pose);
            break;
        }
        case 2:
        {
            const FBox Box = ImportedTopy->GetStaticMesh()->GetBoundingBox();
            const FTransform MeshToWorld = ImportedTopy->GetComponentTransform();
            FVector Left = Box.GetCenter(), Right = Left;
            Left.Y = Box.Min.Y;
            Right.Y = Box.Max.Y;
            Left = MeshToWorld.TransformPosition(Left);
            Right = MeshToWorld.TransformPosition(Right);
            const double Span = FVector::Distance(Left,Right);
            const double PhysicsSpan = 200.*Flight.RightTipOffsetNED().Size();
            FVector2D L,R;
            APlayerController* PC = CastChecked<APlayerController>(GetController());
            OK = FMath::IsNearlyEqual(Span,PhysicsSpan,.2) && PC->ProjectWorldLocationToScreen(Left,L)
                && PC->ProjectWorldLocationToScreen(Right,R);
            UE_LOG(LogTemp,Display,TEXT("SPAN_PROOF %s visual_cm=%.2f physics_cm=%.2f screen_px=%.1f"),ModelName(),Span,PhysicsSpan,(L-R).Size());
            const TCHAR* Shots[] = {TEXT("span-topy.png"),TEXT("span-k3600.png"),TEXT("span-vjx.png"),TEXT("span-fox.png"),TEXT("span-js3.png"),TEXT("span-snipe2.png"),TEXT("span-longbow.png"),TEXT("span-apus4.png"),TEXT("span-nubis.png")};
            static_assert(UE_ARRAY_COUNT(Shots)==UE_ARRAY_COUNT(HangarModels));
            Shot(Shots[Model]);
            break;
        }
        case 3:
            if (Model+1==UE_ARRAY_COUNT(HangarModels)) FPlatformMisc::RequestExit(false);
            else OpenMenu();
            break;
        }
        ++MenuProofStep;
        if (!OK) FPlatformMisc::RequestExitWithStatus(false,3);
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("NubisLaunchProof")))
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        bool OK = true;
        switch (MenuProofStep)
        {
        case 0:
        {
            FString TestSettings;
            OK = FParse::Value(FCommandLine::Get(),TEXT("GameUserSettingsINI="),TestSettings) && Click(TEXT("Hangar"));
            for (int32 I=0; OK && ModelIndex()!=8 && I<UE_ARRAY_COUNT(HangarModels); ++I) OK=Click(TEXT("MODÈLE-"));
            OK = OK && ModelIndex()==8 && CanSelectModel();
            break;
        }
        case 1:
        {
            if (!IsAppearanceReady() && Elapsed<30.f) return;
            const UStaticMesh* Mesh = ImportedTopy->GetStaticMesh();
            OK = IsAppearanceReady() && ImportedTopy->IsVisible() && Mesh
                && Mesh->GetPathName()==TEXT("/Game/Aircraft/Nubis/Nubis.Nubis")
                && FMath::IsNearlyEqual(Mesh->GetBoundingBox().GetSize().Y * FMath::Abs(ImportedTopy->GetComponentScale().Y),595.,.5)
                && FindTagged(Menu.ToSharedRef(),TEXT("CgPlan")).IsValid();
            Shot(TEXT("nubis-hangar.png"));
            break;
        }
        case 2:
            OK = Click(TEXT("TabAdvanced")) && Click(TEXT("DefaultFlapTravel"))
                && Click(TEXT("TabSetup")) && Click(TEXT("SelectAircraft")) && SelectedModel==8 && bPosterHome;
            break;
        case 3:
            if (!IsAppearanceReady() && Elapsed<35.f) return;
            OK = bPosterHome && IsAppearanceReady();
            Shot(TEXT("nubis-accueil.png"));
            break;
        case 4:
        {
            OK = Click(TEXT("Fly")) && IsWalking() && Flight.HasFlaps() && Flight.HasRudder()
                && FMath::IsNearlyEqual(Flight.RightTipOffsetNED().Size()*2.,5.95,.005);
            // Keep the gallery free of overlays; these flags only live in this test session.
            if (AHUD* Hud = CastChecked<APlayerController>(GetController())->GetHUD()) Hud->bShowHUD = false;
            ShowTrails(false);
            MenuProofWeather = Elapsed;
            break;
        }
        case 5:
            // Poll the released keyboard in game mode before sending the first rising edge.
            if (Elapsed-MenuProofWeather<.25f) return;
            OK = IsWalking() && IsAppearanceReady();
            if (OK) SendKey(EKeys::SpaceBar,IE_Pressed);
            MenuProofWeather = Elapsed;
            break;
        case 6:
            if (Elapsed-MenuProofWeather<1.f || (IsHeldForLaunch() && Elapsed-MenuProofWeather<3.f)) return;
            OK = !IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone() && Flight.Airspeed()>0.;
            UE_LOG(LogTemp,Display,TEXT("NUBIS_RELEASE held=%d throwing=%d key_down=%d launch_input=%.2f stopped=%d contact=%s airspeed=%.2f"),
                IsHeldForLaunch(),IsThrowing(),CastChecked<APlayerController>(GetController())->IsInputKeyDown(EKeys::SpaceBar),
                Radio.Command(EGliderControl::HandLaunch),Flight.IsStopped(),*ContactKind.ToString(),Flight.Airspeed());
            SendKey(EKeys::SpaceBar,IE_Released);
            if (OK) Shot(TEXT("nubis-vol-sol.png"));
            break;
        case 7:
            CycleCamera();
            OK = FlightView==EFlightView::Chase;
            break;
        case 8:
            OK = !Flight.IsStopped() && ContactKind.IsNone() && FlightView==EFlightView::Chase;
            Shot(TEXT("nubis-vol-poursuite.png"));
            break;
        case 9:
            FPlatformMisc::RequestExit(false);
            break;
        }
        UE_LOG(LogTemp,Display,TEXT("NUBIS_LAUNCH_TEST step=%d pass=%d"),MenuProofStep,OK);
        if (!OK) FPlatformMisc::RequestExitWithStatus(false,3);
        ++MenuProofStep;
        return;
    }
    // Isolated visual check using the same hangar buttons and lighting as a player.
    const bool CheckJS3 = FParse::Param(FCommandLine::Get(),TEXT("JS3LaunchProof"));
    const bool CheckDLG = FParse::Param(FCommandLine::Get(),TEXT("DLGLaunchProof"));
    const bool CheckLongBow = FParse::Param(FCommandLine::Get(),TEXT("LongBowLaunchProof"));
    const bool CheckApus = FParse::Param(FCommandLine::Get(),TEXT("ApusLaunchProof"));
    const int32 ProofModel = CheckApus ? 7 : CheckLongBow ? 6 : CheckDLG ? 5 : CheckJS3 ? 4 : 3;
    if (CheckApus || CheckLongBow || CheckDLG || CheckJS3 || FParse::Param(FCommandLine::Get(),TEXT("FoxAppearanceProof")))
    {
        if (MenuProofStep==0 && !Click(TEXT("Hangar"))) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
        if (MenuProofStep==1)
            for (int32 I=0; ModelIndex()!=ProofModel && I<UE_ARRAY_COUNT(HangarModels); ++I)
                if (!Click(TEXT("MODÈLE+"))) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
        if (MenuProofStep==2)
        {
            if (ModelIndex()!=ProofModel || !CanSelectModel()) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
            if (Elapsed<18.f) return;
            if (!IsAppearanceReady()) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
            Shot(CheckApus ? TEXT("apus4-hangar.png") : CheckLongBow ? TEXT("longbow-hangar.png") : CheckDLG ? TEXT("dlg-hangar.png") : CheckJS3 ? TEXT("js3-hangar.png") : TEXT("fox-hangar.png"));
        }
        if (MenuProofStep==4 && Elapsed<22.f) return;
        bool OK = true;
        if (MenuProofStep==4) OK = Click(TEXT("SelectAircraft"));
        if (MenuProofStep==5) OK = Click(TEXT("Fly"));
        if (MenuProofStep==6)
        {
            const FVector Offset = TargetPosition - PilotPosition();
            OK = IsWalking() && FMath::IsNearlyEqual(Offset.Z,145.,.1) && (CheckDLG ? FMath::IsNearlyEqual(Offset.Size2D(),144.,.1) : Offset.Size2D()<70.);
            UE_LOG(LogTemp,Display,TEXT("SCALE_LAUNCH_TEST %s held_height_cm=%.1f"),ModelName(),Offset.Z);
            if (CheckDLG) LaunchHeading = 67.f;
            SendKey(EKeys::SpaceBar,IE_Pressed);
        }
        if (MenuProofStep==7)
        {
            if (IsHeldForLaunch() && Elapsed<30.f) return;
            SendKey(EKeys::SpaceBar,IE_Released);
            OK = !IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone()
                && (CheckDLG ? FMath::IsNearlyEqual((Flight.LocalWind()-Flight.RelativeFlowNED()).Size(),30.16,.1)
                             : CheckApus ? !Flight.HasRudder() && !Flight.HasElevons() && !Flight.MotorSettings().Enabled && FMath::IsNearlyEqual((Flight.LocalWind()-Flight.RelativeFlowNED()).Size(),12.03,.1)
                             : CheckLongBow ? Flight.HasElevons() && Flight.MotorSettings().Enabled && FMath::IsNearlyEqual((Flight.LocalWind()-Flight.RelativeFlowNED()).Size(),8.04,.1)
                             : FMath::IsNearlyEqual(Flight.Airspeed(),CheckJS3 ? 21. : 22.,.1));
            if (CheckDLG)
            {
                const FRotator Aim(0.,LaunchHeading,0.);
                const FVector Release = Aim.UnrotateVector(TargetPosition-PilotPosition());
                const FVector Velocity = Aim.UnrotateVector(Flight.LocalWind()-Flight.RelativeFlowNED());
                OK = OK && Release.X > 100. && FMath::Abs(Release.Y) < 1. && FMath::IsNearlyEqual(Release.Z,145.,1.)
                    && Velocity.X > 0. && FMath::Abs(Velocity.Y) < .05;
                UE_LOG(LogTemp,Display,TEXT("DLG_AIM_TEST forward_cm=%.1f lateral_cm=%.1f lateral_mps=%.3f"),Release.X,Release.Y,Velocity.Y);
            }
            UE_LOG(LogTemp,Display,TEXT("SCALE_LAUNCH_TEST %s released=%d airspeed_mps=%.2f"),ModelName(),OK,Flight.Airspeed());
            FPlatformMisc::RequestExitWithStatus(false,OK ? 0 : 3);
        }
        if (!OK) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
        ++MenuProofStep;
        return;
    }
    auto DragCg = [this]()
    {
        const auto Widget = FindTagged(Menu.ToSharedRef(), TEXT("CgPlan"));
        FWidgetPath Path;
        auto& App = FSlateApplication::Get();
        if (!Widget || !App.GeneratePathToWidgetUnchecked(Widget.ToSharedRef(),Path)) return false;
        // Generated widget paths omit the virtual pointer entries needed by mouse routing.
        TArray<FWidgetAndPointer> PointerWidgets;
        for (int32 I=0; I<Path.Widgets.Num(); ++I) PointerWidgets.Emplace(Path.Widgets[I],TOptional<FVirtualPointerPosition>());
        Path = FWidgetPath(MakeArrayView(PointerWidgets));
        const FGeometry& G = Widget->GetCachedGeometry();
        const FVector2D Scale = G.GetLocalSize()/FVector2D(320,150);
        const FVector2D Start = G.LocalToAbsolute(FVector2D(259,91)*Scale);
        const FVector2D End = G.LocalToAbsolute(FVector2D(259,92.6)*Scale);
        const TSet<FKey> Down{EKeys::LeftMouseButton}, Up;
        const uint32 Pointer = FSlateApplication::CursorPointerIndex;
        App.RoutePointerMoveEvent(Path,FPointerEvent(Pointer,Start,Start,Up,EKeys::Invalid,0,FModifierKeysState()),false);
        App.RoutePointerDownEvent(Path,FPointerEvent(Pointer,Start,Start,Down,EKeys::LeftMouseButton,0,FModifierKeysState()));
        App.RoutePointerMoveEvent(Path,FPointerEvent(Pointer,End,Start,Down,EKeys::Invalid,0,FModifierKeysState()),false);
        App.RoutePointerUpEvent(Path,FPointerEvent(Pointer,End,End,Up,EKeys::LeftMouseButton,0,FModifierKeysState()));
        return Menu->DraftCgMm()==StandardLoading().CgMm+2;
    };
    auto SetGain = [this](FName Tag,int32 Value)
    {
        const auto Widget=FindTagged(Menu.ToSharedRef(),Tag);
        if (!Widget || Widget->GetType()!=TEXT("SSpinBox<int32>")) return false;
        auto Spin=StaticCastSharedPtr<SSpinBox<int32>>(Widget);
        if (Spin->GetMinValue()!=-500 || Spin->GetMaxValue()!=500) return false;
        Spin->SetValue(Value,true);
        return true;
    };
    bool OK=true;
    switch (MenuProofStep)
    {
        case 0: OK=bModelReady && IsMenuOpen() && !HasStartedFlight(); Shot(TEXT("club-accueil.png")); MenuProofLocation=TargetPosition; break;
        case 1:
            MenuProofTopySteering=UsesAileronSteering(); MenuProofTopyGains=ControlGains();
            OK=Click(TEXT("Hangar")); break;
        case 2:
            OK=Click(TEXT("MODÈLE+")) && ModelIndex()==1 && CanSelectModel();
            MenuProofKSteering=UsesAileronSteering(); break;
        case 3:
            MenuProofMass = LimitLoading({Loading().MassGrams + 5, Loading().CgMm}).MassGrams;
            OK=IsAppearanceReady() && Click(TEXT("DefaultCg")) && Click(TEXT("MASSE TOTALE+")) && DragCg()
                && Click(TEXT("MANCHE POUR LES VIRAGES+"))
                && TargetPosition.Equals(MenuProofLocation,.01);
            Shot(TEXT("club-hangar.png")); break;
        case 4:
            OK=Click(TEXT("DefaultCg")) && Click(TEXT("SelectAircraft")) && ModelIndex()==1 && Loading().MassGrams == MenuProofMass
                && Loading().CgMm == StandardLoading().CgMm && UsesAileronSteering()!=MenuProofKSteering;
            break;
        case 5: OK=Click(TEXT("Settings")) && Click(TEXT("Devices")); break;
        case 6:
            Shot(TEXT("club-radio.png"));
            // Let Slate refresh bound enabled attributes before learning a key,
            // including on fresh installations without a connected transmitter.
            OK=Click(TEXT("Refresh")) && Click(TEXT("PÉRIPHÉRIQUE+")); break;
        case 7:
        {
            OK=Radio.EditingKeyboard();
            const FName Original=Radio.Binding(EGliderControl::Rudder).Plus;
            for (const FName Key : {FName(TEXT("P")), Original})
            {
                OK=OK && Click(TEXT("BindPlus"));
                FKeyEvent Event(FKey(Key),FModifierKeysState(),0,false,0,0);
                FSlateApplication::Get().ProcessKeyDownEvent(Event);
                FSlateApplication::Get().ProcessKeyUpEvent(Event);
                OK=OK && Radio.Binding(EGliderControl::Rudder).Plus==Key && !Radio.Learning();
            }
            break;
        }
        case 8: OK=Click(TEXT("PÉRIPHÉRIQUE-")) && Click(TEXT("Back")); break;
        case 9: OK=Click(TEXT("Settings")) && Click(TEXT("Graphics")); break;
        case 10:
            Shot(TEXT("club-graphismes.png"));
            OK=Click(TEXT("SYNCHRONISATION VERTICALE+"));
            for (FName Tag : {FName(TEXT("GrassRange")),FName(TEXT("TrailRange"))})
            {
                auto Slider=FindTagged(Menu.ToSharedRef(),Tag);
                if (!Slider) { OK=false; break; }
                const FGeometry G=Slider->GetCachedGeometry();
                const FVector2D End=G.LocalToAbsolute(FVector2D(G.GetLocalSize().X-1,G.GetLocalSize().Y*.5f));
                const uint32 Pointer=FSlateApplication::CursorPointerIndex;
                Slider->OnMouseButtonDown(G,FPointerEvent(Pointer,End,End,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
                Slider->OnMouseButtonUp(G,FPointerEvent(Pointer,End,End,{},EKeys::LeftMouseButton,0,FModifierKeysState()));
            }
            break;
        case 11:
        {
            const int32 Expected=500;
            OK=Click(TEXT("Apply")) && ASlopeWorld::SavedGrassDistanceMeters()==Expected;
            TActorIterator<ASlopeWorld> World(GetWorld());
            OK=OK && World && World->GrassDistanceMeters()==Expected && TrailDistance==Expected;
            break;
        }
        case 12: OK=Click(TEXT("MODE D’AFFICHAGE+")) && Click(TEXT("Apply")); break;
        case 13: OK=Click(TEXT("Revert")); break;
        case 14: OK=Click(TEXT("Back")); break;
        case 15:
            OK=Click(TEXT("Fly")) && HasStartedFlight() && !IsMenuOpen() && IsHeldForLaunch();
            MenuProofLocation=TargetPosition; break;
        case 16:
            OK=IsHeldForLaunch() && TargetPosition.Equals(MenuProofLocation,.01) && ContactKind.IsNone();
            Shot(TEXT("hand-launch-ready.png"));
            SendKey(EKeys::SpaceBar,IE_Pressed); break;
        case 17:
            SendKey(EKeys::SpaceBar,IE_Released);
            OK=!IsHeldForLaunch() && !Flight.IsStopped() && !TargetPosition.Equals(MenuProofLocation,100.) && ContactKind.IsNone();
            OpenMenu(); MenuProofLocation=TargetPosition; break;
        case 18: OK=TargetPosition.Equals(MenuProofLocation,.1) && Click(TEXT("Fly")); break;
        case 19: OK=!IsMenuOpen() && !TargetPosition.Equals(MenuProofLocation,1.); OpenMenu(); break;
        case 20: OK=Click(TEXT("Hangar")); break;
        case 21: OK=Click(TEXT("MODÈLE-")) && ModelIndex()==0 && UsesAileronSteering()==MenuProofTopySteering; break;
        case 22: OK=Click(TEXT("MODÈLE+")) && ModelIndex()==1 && UsesAileronSteering()!=MenuProofKSteering; break;
        case 23:
            OK=Click(TEXT("MANCHE POUR LES VIRAGES+")) && Click(TEXT("SelectAircraft"))
                && UsesAileronSteering()==MenuProofKSteering && Loading().MassGrams==MenuProofMass;
            break;
        case 24: OK=Click(TEXT("Hangar")); break;
        case 25: OK=Click(TEXT("TabAdvanced")); break;
        case 26:
        {
            const auto Check=FindTagged(Menu.ToSharedRef(),TEXT("MotorEnabled"));
            const auto Power=FindTagged(Menu.ToSharedRef(),TEXT("MotorMaxWatts"));
            if (!Check || !Power) { OK=false; break; }
            auto MotorCheck=StaticCastSharedPtr<SCheckBox>(Check);
            if (!MotorCheck->IsChecked()) MotorCheck->ToggleCheckedState();
            StaticCastSharedPtr<SSpinBox<int32>>(Power)->SetValue(432,true);
            OK=SetGain(TEXT("GainYaw"),500) && SetGain(TEXT("GainRoll"),-500) && SetGain(TEXT("GainPitch"),0)
                && Menu->DraftControlGains()==FGliderControlGains{-500,0,500};
            Shot(TEXT("club-avance.png")); break;
        }
        case 27: OK=Click(TEXT("TabSetup")); break;
        case 28: OK=Click(TEXT("TabAdvanced")) && Menu->DraftControlGains()==FGliderControlGains{-500,0,500}; break;
        case 29:
        {
            OK=Click(TEXT("SelectAircraft")) && ControlGains()==FGliderControlGains{-500,0,500};
            FConfigFile Saved;
            FGliderControlGains ReadBack;
            // The proof launcher isolates user settings with this explicit file override.
            // GGameUserSettingsIni is only a config-cache alias in UE 5.8.
            FString SavedPath;
            OK=OK && FParse::Value(FCommandLine::Get(),TEXT("GameUserSettingsINI="),SavedPath);
            Saved.Read(SavedPath);
            int32 Watts=0;
            bool MotorEnabled=false;
            OK=OK && MotorSettings()==FGliderMotor{true,432} && IsControlInUse(EGliderControl::MotorCut)
                && Saved.GetBool(TEXT("SimPlaneur.Loading.K3600"),TEXT("MotorEnabled"),MotorEnabled) && MotorEnabled
                && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("MotorMaxWatts"),Watts) && Watts==432;
            OK=OK && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("RollGainPercent"),ReadBack.Roll)
                && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("PitchGainPercent"),ReadBack.Pitch)
                && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("YawGainPercent"),ReadBack.Yaw)
                && ReadBack==ControlGains();
            break;
        }
        case 30: OK=Click(TEXT("Hangar")); break;
        case 31: OK=Click(TEXT("MODÈLE-")) && ControlGains()==MenuProofTopyGains && !MotorSettings().Enabled; break;
        case 32: OK=Click(TEXT("MODÈLE+")) && ControlGains()==FGliderControlGains{-500,0,500}; break;
        case 33: OK=Click(TEXT("TabAdvanced")); break;
        case 34:
            OK=Click(TEXT("DefaultGains")) && Menu->DraftControlGains()==FGliderControlGains{}
                && ControlGains()==FGliderControlGains{-500,0,500}; break;
        case 35: OK=Click(TEXT("Back")) && ControlGains()==FGliderControlGains{-500,0,500}; break;
        case 36: OK=Click(TEXT("Hangar")); break;
        case 37: OK=Click(TEXT("TabAdvanced")); break;
        case 38:
            OK=Click(TEXT("DefaultGains")) && Click(TEXT("SelectAircraft"))
                && ControlGains()==FGliderControlGains{} && Loading().MassGrams==MenuProofMass; break;
        case 39: OK=Click(TEXT("Fly")) && IsHeldForLaunch(); break;
        case 40: SendKey(EKeys::SpaceBar,IE_Pressed); break;
        case 41:
            OK=!IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone();
            Shot(TEXT("hand-launch-flight.png"));
            SendKey(EKeys::SpaceBar,IE_Released); SendKey(EKeys::R,IE_Pressed); break;
        case 42:
            Shot(TEXT("launch-site.png"));
            OK=IsHeldForLaunch() && !IsThrowing() && ContactKind.IsNone();
            SendKey(EKeys::R,IE_Released); MenuProofLocation=TargetPosition; break;
        case 43:
            OK=IsHeldForLaunch() && TargetPosition.Equals(MenuProofLocation,.01);
            OpenMenu(); break;
        case 44:
            OK=Click(TEXT("Explore")) && IsNoclip() && !IsMenuOpen();
            NoclipPosition=PilotSite+FVector(780,-720,255);
            NoclipRotation=(PilotSite+FVector(560,30,-90)-NoclipPosition).Rotation();
            MenuProofCamera=NoclipPosition; MenuProofWeather=WeatherSeconds; break;
        case 45:
            Shot(TEXT("site-overview.png"));
            SendKey(EKeys::W,IE_Pressed); SendKey(EKeys::LeftControl,IE_Pressed);
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,100.f,.016f,1,FPlatformTime::Cycles64()));
            break;
        case 46:
            UE_LOG(LogTemp,Display,TEXT("NOCLIP_PROOF travelled=%.1f yaw=%.2f height_above_ground=%.1f plane_drift=%.4f weather_delta=%.4f"),
                FVector::Dist(NoclipPosition,MenuProofCamera),NoclipRotation.Yaw,
                NoclipPosition.Z-ASlopeWorld::TerrainHeightCm(NoclipPosition.X,NoclipPosition.Y),
                FVector::Dist(TargetPosition,MenuProofLocation),WeatherSeconds-MenuProofWeather);
            {
            FHitResult Ground;
            OK=IsNoclip() && TargetPosition.Equals(MenuProofLocation,.01) && WeatherSeconds==MenuProofWeather
                && FVector::Dist2D(NoclipPosition,MenuProofCamera)>800. && FMath::Abs(NoclipRotation.Yaw)>1.f
                && FindPilotGround(NoclipPosition,Ground,true) && FMath::IsNearlyEqual(NoclipPosition.Z-Ground.ImpactPoint.Z,85.,1.);
            }
            SendKey(EKeys::W,IE_Released); SendKey(EKeys::LeftControl,IE_Released);
            SendKey(EKeys::SpaceBar,IE_Pressed); SendKey(EKeys::LeftShift,IE_Pressed);
            MenuProofCamera=NoclipPosition; break;
        case 47:
            OK=NoclipPosition.Z>MenuProofCamera.Z+20000. && IsHeldForLaunch() && !IsThrowing();
            SendKey(EKeys::SpaceBar,IE_Released); SendKey(EKeys::LeftShift,IE_Released);
            OpenMenu(); MenuProofCamera=NoclipPosition; break;
        case 48:
            OK=NoclipPosition.Equals(MenuProofCamera,.01) && Click(TEXT("Fly")) && IsNoclip();
            Shot(TEXT("noclip-exploration.png")); break;
        case 49: SendKey(EKeys::N,IE_Pressed); break;
        case 50:
        {
            FMinimalViewInfo View;
            CalcCamera(0.f,View);
            OK=!IsNoclip() && IsHeldForLaunch() && !IsThrowing()
                && FVector2D(PilotPosition()-MenuProofCamera).Size()<.01
                && View.Location.Equals(PilotPosition()+FVector(0,0,170),.01)
                && FMath::Abs(FMath::FindDeltaAngleDegrees(View.Rotation.Yaw,NoclipRotation.Yaw))<.01
                && TargetPosition.Equals(HandPose().GetLocation(),.01)
                && !TargetPosition.Equals(MenuProofLocation,100.);
            SendKey(EKeys::N,IE_Released);
            MenuProofLocation=TargetPosition;
            Shot(TEXT("relocated-site-ready.png")); break;
        }
        case 51:
            OK=IsWalking(); MenuProofCamera=PilotPosition(); MenuProofHeading=LaunchHeading; MenuProofPitch=WalkPitch;
            SendKey(EKeys::W,IE_Pressed);
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,100.f,.016f,1,FPlatformTime::Cycles64()));
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseY,50.f,.016f,1,FPlatformTime::Cycles64()));
            break;
        case 52:
        {
            FHitResult Ground;
            const double Walked=FVector2D(PilotPosition()-MenuProofCamera).Size();
            OK=IsWalking() && Walked>50. && Walked<250.
                && FMath::Abs(FMath::FindDeltaAngleDegrees(MenuProofHeading,LaunchHeading))>5.
                && FMath::Abs(WalkPitch-MenuProofPitch)>2.
                && FindPilotGround(PilotPosition(),Ground) && PilotPosition().Equals(Ground.ImpactPoint,.1);
            UE_LOG(LogTemp,Display,TEXT("WALK_PROOF distance_cm=%.1f heading=%.1f pitch=%.1f"),Walked,LaunchHeading,WalkPitch);
            SendKey(EKeys::W,IE_Released);
            MenuProofCamera=PilotPosition(); MenuProofLocation=TargetPosition;
            MenuProofHeading=LaunchHeading; MenuProofPitch=WalkPitch;
            Shot(TEXT("walking-ready.png"));
            SendKey(EKeys::SpaceBar,IE_Pressed); break;
        }
        case 53:
            OK=!IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone()
                && FVector::DotProduct(TargetPosition-MenuProofLocation,FRotator(0,LaunchHeading,0).Vector())>500.;
            SendKey(EKeys::SpaceBar,IE_Released);
            Shot(TEXT("relocated-site-flight.png"));
            SendKey(EKeys::W,IE_Pressed); SendKey(EKeys::L,IE_Pressed);
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,200.f,.016f,1,FPlatformTime::Cycles64()));
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseY,-100.f,.016f,1,FPlatformTime::Cycles64()));
            break;
        case 54:
            OK=!IsWalking() && PilotPosition().Equals(MenuProofCamera,.01)
                && LaunchHeading==MenuProofHeading && WalkPitch==MenuProofPitch;
            SendKey(EKeys::W,IE_Released); SendKey(EKeys::L,IE_Released); SendKey(EKeys::R,IE_Pressed); break;
        case 55:
            OK=IsWalking() && TargetPosition.Equals(MenuProofLocation,.01);
            SendKey(EKeys::R,IE_Released); ToggleNoclip(); MenuProofCamera=PilotPosition(); break;
        case 56:
            NoclipPosition=FVector(Valley::LakeX,Valley::LakeY,100000.);
            ToggleNoclip();
            OK=IsNoclip() && !SiteNotice.IsEmpty() && PilotPosition().Equals(MenuProofCamera,.01); break;
        case 57:
            NoclipPosition=FVector(20000000.,20000000.,100000.);
            ToggleNoclip();
            OK=IsNoclip() && !SiteNotice.IsEmpty() && PilotPosition().Equals(MenuProofCamera,.01); break;
        case 58:
            NoclipPosition=MenuProofCamera+FVector(0,0,10000.);
            ToggleNoclip();
            OK=!IsNoclip() && IsHeldForLaunch() && PilotPosition().Equals(MenuProofCamera,.01);
            OpenMenu(); break;
        case 59: OK=Click(TEXT("Weather")); break;
        case 60:
            OK=Click(TEXT("DefaultWeather")) && Click(TEXT("HEURE SOLAIRE+")) && Click(TEXT("ÉTAT DU CIEL+")) && Click(TEXT("FORCE DU VENT+")) && Click(TEXT("VENT VENANT DU+"))
                && Click(TEXT("ACTIVITÉ THERMIQUE-")) && Click(TEXT("TURBULENCE+")) && Click(TEXT("ApplyWeather"));
            Shot(TEXT("club-meteo.png")); break;
        case 61:
        {
            const auto Weather=ValleyAir::Settings();
            OK=FMath::IsNearlyEqual(Weather.WindSpeedMps,5.) && FMath::IsNearlyEqual(Weather.WindFromDegrees,15.)
                && FMath::IsNearlyEqual(Weather.ThermalStrength,.75) && FMath::IsNearlyEqual(Weather.Turbulence,.25);
            ValleyAir::LoadSettings();
            OK=OK && ValleyAir::Settings().SolarMinutes==870 && ValleyAir::Settings().Sky==2 && FMath::IsNearlyEqual(ValleyAir::Settings().WindSpeedMps,5.) && Click(TEXT("DefaultWeather")) && Click(TEXT("ApplyWeather"));
            break;
        }
        case 62: OK=Click(TEXT("Back")); break;
        case 63:
        {
            FHitResult Ground;
            OK=Click(TEXT("SITE DE VOL+")) && LaunchSiteNumber()==2 && FindPilotGround(PilotPosition(),Ground)
                && Ground.GetComponent()->ComponentHasTag(TEXT("terrace")) && Click(TEXT("Fly")) && IsWalking();
            break;
        }
        case 64: SendKey(EKeys::SpaceBar,IE_Pressed); break;
        case 65:
            OK=!IsHeldForLaunch() && !Flight.IsStopped() && ContactKind.IsNone();
            SendKey(EKeys::SpaceBar,IE_Released); OpenMenu(); break;
        case 66:
            OK=Click(TEXT("Explore")) && IsNoclip();
            NoclipPosition=PilotSite+FVector(1600,1500,560);
            NoclipRotation=(PilotSite+FVector(-140,100,135)-NoclipPosition).Rotation(); break;
        case 67: Shot(TEXT("site-02-chalet.png")); break;
        case 68: OpenMenu(); OK=Click(TEXT("SITE DE VOL-")) && LaunchSiteNumber()==1 && IsHeldForLaunch(); break;
        case 69: UE_LOG(LogTemp,Display,TEXT("MENU_PROOF quit via menu")); OK=Click(TEXT("Quit")); break;
        default: OK=false; break;
    }
    UE_LOG(LogTemp,Display,TEXT("MENU_PROOF step=%d pass=%d"),MenuProofStep,OK);
    ++MenuProofStep;
    if (!OK) FPlatformMisc::RequestExitWithStatus(false,3);
#endif
}
