#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "ValleyAir.h"
#include "SlopeWorld.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void ASimPlaneurHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner)
    {
        return;
    }
    const ASlopeFlightPawn* Plane = Cast<ASlopeFlightPawn>(PlayerOwner->GetPawn());
    if (!Plane) return;
    UpdateEquipmentOpacity(*Plane);
    if (Plane->IsMenuOpen()) return;
    DrawEquipmentHUD(*Plane);
    const FVector2D XY=Plane->MapPositionXY();
    const float BoxX=Canvas->SizeX-194.f;
    DrawRect(FLinearColor(.010f,.026f,.032f,.88f),BoxX,12,182,48);
    DrawText(Plane->IsNoclip()?TEXT("CAMÉRA  ·  F2 CARTE"):Plane->IsHeldForLaunch()?TEXT("PILOTE  ·  F2 CARTE"):TEXT("PLANEUR  ·  F2 CARTE"),FLinearColor(1,.78f,.4f),BoxX+9,16);
    DrawText(FString::Printf(TEXT("X %+.0f   Y %+.0f m"),XY.X,XY.Y),FLinearColor::White,BoxX+9,35);
    if (Plane->IsNoclip())
    {
        auto Fade=[this](FLinearColor Colour) { Colour.A *= EquipmentOpacity; return Colour; };
        const FVector P=Plane->ExplorationPosition()*.01;
        const FVector Pilot=Plane->PilotPosition()*.01;
        const double FromPilot=FVector2D(P.X-Pilot.X,P.Y-Pilot.Y).Size();
        DrawRect(Fade(FLinearColor(.010f,.026f,.032f,.88f)),20,20,690,Plane->ExplorationNotice().IsEmpty()?128:154);
        DrawText(TEXT("EXPLORATION NOCLIP  ·  Vol suspendu"),Fade(FLinearColor(1.f,.75f,.32f)),32,31);
        DrawText(TEXT("Souris : regarder   |   ZQSD / WASD / flèches : se déplacer"),Fade(FLinearColor::White),32,55);
        DrawText(TEXT("Espace : monter   Ctrl : descendre   Maj : vitesse ×10   Molette : vitesse"),Fade(FLinearColor::White),32,77);
        DrawText(FString::Printf(TEXT("%.0f m/s  |  Distance du pilote : %.0f m"),
            Plane->ExplorationSpeed(),FromPilot),Fade(FLinearColor::White),32,99);
        DrawText(Plane->Radio.Shortcut(EGliderControl::Noclip)+TEXT(" : se poser et marcher   |   Échap : menu"),Fade(FLinearColor::White),32,121);
        if (!Plane->ExplorationNotice().IsEmpty())
            DrawText(Plane->ExplorationNotice(),Fade(FLinearColor(1.f,.75f,.32f)),32,147);
        return;
    }
    DrawWindCompass(*Plane);
    if(Plane->DamageLevel()>0)
    {
        const bool Crash=Plane->DamageLevel()==2;
        const float S=InstrumentScale*FMath::Clamp(Canvas->SizeX/1400.f,.7f,1.4f);
        const float W=240*S,X=Canvas->SizeX*.5f-310*S,Y=220*S;
        DrawRect(FLinearColor(.025f,.12f,.14f,.92f),X,Y,W,46*S);
        DrawRect(Crash?FLinearColor(.95f,.22f,.12f):FLinearColor(1.f,.67f,.18f),X,Y,4*S,46*S);
        DrawText(Crash?TEXT("CRASH"):TEXT("DÉGÂTS"),Crash?FLinearColor(1.f,.35f,.22f):FLinearColor(1.f,.78f,.4f),X+16*S,Y+6*S,GEngine->GetMediumFont(),S);
        DrawText(TEXT("COMMANDES COUPÉES"),FLinearColor(.95f,.91f,.77f),X+16*S,Y+28*S,GEngine->GetSmallFont(),.8f*S);
    }
    if (!Plane->ShowInstruments())
    {
        return;
    }
    const float X = 24.0f;
    const float Y = 100.f * FMath::Clamp(Canvas->SizeX / 1250.f, .82f, 1.3f);
    const float Width = 690.0f;
    const float Height = 279.0f;
    DrawRect(FLinearColor(0.008f, 0.018f, 0.024f, 0.82f), X, Y, Width, Height);
    DrawRect(FLinearColor(0.90f, 0.22f, 0.07f, 1.0f), X, Y, 5.0f, Height);
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    if (!Font)
    {
        return;
    }
    float Line = Y + 14.0f;
    auto TextLine = [this, Font, X, &Line](const FString& Text, const FLinearColor& Color = FLinearColor::White)
    {
        DrawText(Text, Color, X + 17.0f, Line, Font, 1.0f, false);
        Line += 25.0f;
    };
    TextLine(FString::Printf(TEXT("VOL DE PENTE  •  %s  •  UNREAL"),Plane->ModelName()));
    TextLine(TEXT("Moteur maison 6 DDL  ·  120 Hz"), FLinearColor(0.75f, 0.82f, 0.85f));
    if (Plane)
    {
        const FString Status = Plane->IsFlightReady()
            ? FString::Printf(TEXT("Altitude %.1f m  •  Vitesse %.1f m/s  •  Chute %.2f m/s"), Plane->GetAltitudeM(), Plane->GetAirspeedMps(), Plane->GetSinkMps())
            : TEXT("Modèle de vol indisponible");
        TextLine(Status, Plane->IsFlightReady() ? FLinearColor::White : FLinearColor(1.0f, 0.55f, 0.20f));
        TextLine(FString::Printf(TEXT("Pente %+.1f m/s   ·   Thermique / rafales %+.1f m/s"), Plane->GetRidgeLift(), -Plane->GetLocalWind().Z - Plane->GetRidgeLift()));
        FString Controls = Plane->GetControlMode();
        if (Plane->HasFlaps()) Controls += FString::Printf(TEXT("   ·   Volets %.0f°"),Plane->FlapAngleDeg());
        if (Plane->CrowFraction()>.005) Controls += FString::Printf(TEXT("   ·   Croco %.0f %%"),100.*Plane->CrowFraction());
        TextLine(Controls);
        TextLine(FString::Printf(TEXT("Radio  X:%+.2f  Y:%+.2f  Z:%+.2f  Rx:%+.2f"),
            Plane->GetRadioAxis(0), Plane->GetRadioAxis(1), Plane->GetRadioAxis(2), Plane->GetRadioAxis(3)),
            Plane->IsRadioActive() ? FLinearColor(0.35f, 1.0f, 0.45f) : FLinearColor(0.72f, 0.72f, 0.72f));
        TextLine(FString::Printf(TEXT("Radio  Ry:%+.2f  Rz:%+.2f  S1:%+.2f  S2:%+.2f"),
            Plane->GetRadioAxis(4), Plane->GetRadioAxis(5),Plane->GetRadioAxis(6),Plane->GetRadioAxis(7)),
            Plane->IsRadioActive() ? FLinearColor(0.35f, 1.0f, 0.45f) : FLinearColor(0.72f, 0.72f, 0.72f));
        TextLine(FString::Printf(TEXT("%s en main  •  %s caméra (%s)  •  Échap menu  •  %.0f Hz"), *Plane->Radio.Shortcut(EGliderControl::Relaunch),*Plane->Radio.Shortcut(EGliderControl::Camera),Plane->FlightCameraName(),Plane->GetPhysicsHz()));
        TextLine(TEXT("Avant lancer : WASD marcher · souris regarder. Pilote fixe après lancer."));
        TextLine(FString::Printf(TEXT("%s  •  Filets d’air   ·   Bleu : brise   ·   Doré : ascendance   |   %s  •  Vario %s"),*Plane->Radio.Shortcut(EGliderControl::WindTrails),*Plane->Radio.Shortcut(EGliderControl::Vario),Plane->VarioEnabled()?TEXT("actif"):TEXT("coupé")));
    }
}
