#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "ValleyAir.h"
#include "SlopeWorld.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void ASimPlaneurHUD::DrawWindCompass(const ASlopeFlightPawn& Plane)
{
    FVector Eye;
    FRotator View;
    PlayerOwner->GetPlayerViewPoint(Eye, View);
    const FVector Wind = Plane.GetLocalWind();
    const FVector Relative = Plane.GetRelativeFlow();
    const float Scale = FMath::Clamp(Canvas->SizeX / 1250.f, .82f, 1.3f);
    const FVector2D Origin((Canvas->SizeX - 324.f * Scale) * .5f, 12.f * Scale);
    const FVector2D Centre = Origin + Scale * FVector2D(46, 40);
    const FLinearColor Ice(.28f, .88f, 1.f), Lilac(.86f, .73f, 1.f), Ivory(.93f, .94f, .91f);
    const FLinearColor North(1.f, .43f, .30f), Quiet(.45f, .57f, .59f);
    DrawRect(FLinearColor(.010f, .026f, .032f, .84f), Origin.X, Origin.Y, 324 * Scale, 80 * Scale);
    UFont* Font = GEngine->GetSmallFont();
    auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width)
    {
        DrawLine(A.X, A.Y, B.X, B.Y, Color, Width * Scale);
    };
    auto Point = [&](double Bearing, double Radius)
    {
        return Centre + ValleyAir::CompassDirection(Bearing, View.Yaw) * (Scale * Radius);
    };
    auto Label = [&](const FString& S, FLinearColor Color, FVector2D Pos, float Size, bool Centered)
    {
        if (Centered)
        {
            float W, H;
            GetTextSize(S, W, H, Font, Size * Scale);
            Pos -= FVector2D(W, H) * .5;
        }
        DrawText(S, Color, Pos.X, Pos.Y, Font, Size * Scale);
    };
    const TCHAR* Cardinals[] = {TEXT("N"), TEXT("E"), TEXT("S"), TEXT("O")};
    for (int32 I = 0; I < 24; ++I)
    {
        const int32 Bearing = 15 * I;
        Line(Point(Bearing, 23), Point(Bearing + 15, 23), Quiet, .8f);
        if (I % 6 == 0)
            Label(Cardinals[I / 6], I == 0 ? North : Ivory, Point(Bearing, 31), 1.f, true);
        else
            Line(Point(Bearing, 22), Point(Bearing, I % 3 ? 20.5 : 18.5), Quiet, 1.f);
    }
    // Both pointers indicate where the air comes FROM in world space.
    auto Pointer = [&](const FVector& Flow, FLinearColor Color, double Length)
    {
        if (Flow.SizeSquared2D() < .0025) return;
        const double Bearing = ValleyAir::FromBearing(Flow);
        const FVector2D Tip = Point(Bearing, Length);
        Line(Centre, Tip, Color, 1.8f);
        Line(Tip, Point(Bearing - 12, Length - 4.5), Color, 1.8f);
        Line(Tip, Point(Bearing + 12, Length - 4.5), Color, 1.8f);
    };
    Pointer(Wind, Ice, 22.);
    Pointer(Relative, Lilac, 14.);
    auto Readout = [&](const TCHAR* Title, const FVector& Flow, double Speed, FLinearColor Color, float Y)
    {
        const FString Direction = Flow.SizeSquared2D() < .0025 ? TEXT("—") :
            FString::Printf(TEXT("%03d°"), FMath::RoundToInt(ValleyAir::FromBearing(Flow)) % 360);
        Label(Title, Color, Origin + Scale * FVector2D(92, Y), 1.f, false);
        Label(Direction, Ivory, Origin + Scale * FVector2D(197, Y), 1.f, false);
        Label(FString::Printf(TEXT("%.1f m/s"), Speed), Ivory, Origin + Scale * FVector2D(252, Y), 1.f, false);
    };
    Readout(TEXT("VENT DE"), Wind, Wind.Size2D(), Ice, 8);
    Readout(TEXT("RELATIF DE"), Relative, Relative.Size(), Lilac, 31);
    Label(FString::Printf(TEXT("Air %+.1f   ·   Vario %+.1f m/s"), -Wind.Z, -Plane.GetSinkMps()),
          Ivory, Origin + Scale * FVector2D(92, 55), 1.f, false);
}

void ASimPlaneurHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner)
    {
        return;
    }
    const ASlopeFlightPawn* Plane = Cast<ASlopeFlightPawn>(PlayerOwner->GetPawn());
    if (!Plane) return;
    if (Plane->IsMenuOpen()) return;
    if (Plane->IsNoclip())
    {
        const FVector P=Plane->ExplorationPosition()*.01;
        const FVector Pilot=Plane->PilotPosition()*.01;
        const double FromPilot=FVector2D(P.X-Pilot.X,P.Y-Pilot.Y).Size();
        DrawRect(FLinearColor(.010f,.026f,.032f,.88f),20,20,690,Plane->ExplorationNotice().IsEmpty()?128:154);
        DrawText(TEXT("EXPLORATION NOCLIP  ·  Vol suspendu"),FLinearColor(1.f,.75f,.32f),32,31);
        DrawText(TEXT("Souris : regarder   |   ZQSD / WASD / flèches : se déplacer"),FLinearColor::White,32,55);
        DrawText(TEXT("Espace : monter   Ctrl : descendre   Maj : vitesse ×10   Molette : vitesse"),FLinearColor::White,32,77);
        DrawText(FString::Printf(TEXT("%.0f m/s  |  X %.0f  Y %.0f  Z %.0f m  |  Distance du pilote : %.0f m"),
            Plane->ExplorationSpeed(),P.X,P.Y,P.Z,FromPilot),FLinearColor::White,32,99);
        DrawText(Plane->Radio.Shortcut(EGliderControl::Noclip)+TEXT(" : se poser et marcher   |   Échap : menu"),FLinearColor::White,32,121);
        if (!Plane->ExplorationNotice().IsEmpty())
            DrawText(Plane->ExplorationNotice(),FLinearColor(1.f,.75f,.32f),32,147);
        return;
    }
    DrawWindCompass(*Plane);
    if (Plane->IsHeldForLaunch())
    {
        DrawRect(FLinearColor(.016f,.032f,.027f,.90f),20,Canvas->SizeY-124,650,64);
        const FString Prompt=Plane->IsThrowing() ? TEXT("Lancer en cours…") :
            TEXT("MARCHE · planeur en main   •   ")+Plane->Radio.Shortcut(EGliderControl::HandLaunch)+(Plane->UsesDiscusLaunch() ? TEXT(" : lancer disque") : TEXT(" : lancer"));
        DrawText(Prompt,FLinearColor(1.f,.75f,.32f),32,Canvas->SizeY-112);
        if (Plane->IsWalking()) DrawText(TEXT("WASD / ZQSD : marche lente   |   Souris : regarder et orienter le lancer   |   ")
            +Plane->Radio.Shortcut(EGliderControl::Noclip)+TEXT(" : noclip"),FLinearColor::White,32,Canvas->SizeY-88);
    }
    if (Plane && !Plane->GetContactLabel().IsEmpty())
    {
        DrawRect(FLinearColor(.075f,.038f,.018f,.92f),20,Canvas->SizeY-92,366,32);
        DrawText(Plane->GetContactLabel()+TEXT("   •   Remettre en main : ")+Plane->Radio.Shortcut(EGliderControl::Relaunch),FLinearColor::White,32,Canvas->SizeY-84);
    }
    if (!Plane->ShowInstruments())
    {
        const float Top = Canvas->SizeY - 51.f;
        DrawRect(FLinearColor(.016f,.032f,.027f,.78f),20,Top,406,32);
        DrawText(FString::Printf(TEXT("%s   |   %.0f m   ·   %.1f m/s     |     %s instruments   ·   %s filets"), Plane->ModelName(),Plane->GetAltitudeM(),Plane->GetAirspeedMps(),*Plane->Radio.Shortcut(EGliderControl::Instruments),*Plane->Radio.Shortcut(EGliderControl::WindTrails)), FLinearColor::White,32,Top+8);
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
        TextLine(FString::Printf(TEXT("%s  •  Filets d’air   ·   Bleu : brise   ·   Doré : ascendance"),*Plane->Radio.Shortcut(EGliderControl::WindTrails)));
    }
}
