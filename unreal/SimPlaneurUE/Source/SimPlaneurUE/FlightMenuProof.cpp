#include "SlopeFlightPawn.h"
#if !UE_BUILD_SHIPPING
#include "FlightMenu.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "SlopeWorld.h"
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
    if (Elapsed<5.f+MenuProofStep*1.3f) return;
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
    auto SendKey=[this](FKey Input,EInputEvent Event)
    {
        CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
            nullptr,FInputDeviceId::CreateFromInternalId(0),Input,Event,FPlatformTime::Cycles64()));
    };
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
        case 5: OK=Click(TEXT("Devices")); break;
        case 6: Shot(TEXT("club-radio.png")); OK=Click(TEXT("Refresh")); break;
        case 7:
        {
            OK=Click(TEXT("PÉRIPHÉRIQUE+")) && Radio.EditingKeyboard();
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
        case 9: OK=Click(TEXT("Graphics")); break;
        case 10: Shot(TEXT("club-graphismes.png")); OK=Click(TEXT("SYNCHRONISATION VERTICALE+")); break;
        case 11: OK=Click(TEXT("Apply")); break;
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
            OK=SetGain(TEXT("GainYaw"),500) && SetGain(TEXT("GainRoll"),-500) && SetGain(TEXT("GainPitch"),0)
                && Menu->DraftControlGains()==FGliderControlGains{-500,0,500};
            Shot(TEXT("club-avance.png")); break;
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
            OK=OK && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("RollGainPercent"),ReadBack.Roll)
                && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("PitchGainPercent"),ReadBack.Pitch)
                && Saved.GetInt(TEXT("SimPlaneur.Loading.K3600"),TEXT("YawGainPercent"),ReadBack.Yaw)
                && ReadBack==ControlGains();
            break;
        }
        case 30: OK=Click(TEXT("Hangar")); break;
        case 31: OK=Click(TEXT("MODÈLE-")) && ControlGains()==MenuProofTopyGains; break;
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
            OK=IsHeldForLaunch() && !IsThrowing() && ContactKind.IsNone();
            SendKey(EKeys::R,IE_Released); MenuProofLocation=TargetPosition; break;
        case 43:
            OK=IsHeldForLaunch() && TargetPosition.Equals(MenuProofLocation,.01);
            OpenMenu(); break;
        case 44:
            OK=Click(TEXT("Explore")) && IsNoclip() && !IsMenuOpen();
            MenuProofCamera=NoclipPosition; MenuProofWeather=WeatherSeconds; break;
        case 45:
            SendKey(EKeys::W,IE_Pressed); SendKey(EKeys::LeftControl,IE_Pressed);
            CastChecked<APlayerController>(GetController())->InputKey(FInputKeyEventArgs(
                nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,100.f,.016f,1,FPlatformTime::Cycles64()));
            break;
        case 46:
            UE_LOG(LogTemp,Display,TEXT("NOCLIP_PROOF travelled=%.1f yaw=%.2f height_above_ground=%.1f plane_drift=%.4f weather_delta=%.4f"),
                FVector::Dist(NoclipPosition,MenuProofCamera),NoclipRotation.Yaw,
                NoclipPosition.Z-ASlopeWorld::TerrainHeightCm(NoclipPosition.X,NoclipPosition.Y),
                FVector::Dist(TargetPosition,MenuProofLocation),WeatherSeconds-MenuProofWeather);
            OK=IsNoclip() && TargetPosition.Equals(MenuProofLocation,.01) && WeatherSeconds==MenuProofWeather
                && FVector::Dist(NoclipPosition,MenuProofCamera)>2000. && FMath::Abs(NoclipRotation.Yaw)>1.f
                && NoclipPosition.Z<ASlopeWorld::TerrainHeightCm(NoclipPosition.X,NoclipPosition.Y);
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
        case 59: UE_LOG(LogTemp,Display,TEXT("MENU_PROOF quit via menu")); OK=Click(TEXT("Quit")); break;
        default: OK=false; break;
    }
    UE_LOG(LogTemp,Display,TEXT("MENU_PROOF step=%d pass=%d"),MenuProofStep,OK);
    ++MenuProofStep;
    if (!OK) FPlatformMisc::RequestExitWithStatus(false,3);
#endif
}
