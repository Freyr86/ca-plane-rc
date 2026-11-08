#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "WinchStation.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    using State = FBungeeLaunch::EState;
    const TCHAR* Settings = TEXT("SimPlaneur.Bungee");
    constexpr double ReachCm = 300.;
    const FLinearColor Colours[] = {FLinearColor(1.f,.68f,.2f), FLinearColor(1.f,.25f,.1f),
        FLinearColor(.92f,.12f,.26f), FLinearColor(.18f,.75f,1.f),
        FLinearColor(.72f,.35f,1.f), FLinearColor(.94f,.96f,.94f)};
    const TCHAR* ColourNames[] = {TEXT("Jaune doré"),TEXT("Orange"),TEXT("Rouge"),TEXT("Bleu ciel"),TEXT("Violet"),TEXT("Blanc")};
}

FLinearColor ASlopeFlightPawn::EquipmentColour(int32 Index) { return Colours[FMath::Clamp(Index,0,int32(UE_ARRAY_COUNT(Colours))-1)]; }
FLinearColor ASlopeFlightPawn::BungeeColour() const { return EquipmentColour(BungeeColourIndex); }
FString ASlopeFlightPawn::BungeeColourName() const { return ColourNames[BungeeColourIndex]; }
void ASlopeFlightPawn::SetWinchPowerLimit(float Value)
{
    WinchMaximumPower=FMath::Clamp(Value,.25f,1.5f);
    GConfig->SetFloat(Settings,TEXT("WinchPower"),WinchMaximumPower,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}
double ASlopeFlightPawn::WinchCommand() const
{
    const bool Assigned=Radio.HasAssignment(EGliderControl::WinchPower);
    if(Bungee.IsWinch() && Bungee.State==State::Towing && bManualWinchPower && !Assigned) return 0.;
    return WinchMaximumPower*(Assigned?Radio.Command(EGliderControl::WinchPower):1.);
}
void ASlopeFlightPawn::CycleBungeeColour(int32 Step)
{
    const int32 Count = UE_ARRAY_COUNT(Colours);
    BungeeColourIndex = (BungeeColourIndex + Step%Count + Count)%Count;
    GConfig->SetInt(Settings,TEXT("Colour"),BungeeColourIndex,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    if(Bungee.Installed() && Bungee.State!=State::LayingOut) SaveBungee();
}

void ASlopeFlightPawn::SaveBungee()
{
    if(Bungee.Installed() && Bungee.State!=State::LayingOut)
    {
        if(auto* E=LaunchInstallations.Find(ActiveBungeeId))
        { E->Anchor=Bungee.Anchor; E->Departure=Bungee.Departure; E->Stiffness=Bungee.Stiffness; E->Colour=BungeeColourIndex; }
        else
        {
            FLaunchInstallation Entry;
            Entry.Type=Bungee.Type;
            Entry.Anchor=Bungee.Anchor; Entry.Departure=Bungee.Departure;
            Entry.Stiffness=Bungee.Stiffness; Entry.Colour=BungeeColourIndex;
            const auto Result=LaunchInstallations.Place(Entry);
            ActiveBungeeId=Result.Id;
            EquipmentConfirmationText=FString::Printf(TEXT("%s installé · %d / 10 dans le monde"),Bungee.IsWinch()?TEXT("Treuil"):TEXT("Élastique"),LaunchInstallations.Count(Bungee.Type));
            if(Result.NearbyRemoved>0) EquipmentConfirmationText+=FString::Printf(TEXT(" · %d installation(s) à moins de 500 m remplacée(s)"),Result.NearbyRemoved);
            if(Result.LeastUsedRemoved!=INDEX_NONE) EquipmentConfirmationText+=FString::Printf(TEXT(" · Installation #%d, la moins utilisée, retirée"),Result.LeastUsedRemoved);
            EquipmentConfirmationUntil=FPlatformTime::Seconds()+7.;
        }
    }
    // Mark the legacy single-installation save as migrated, even after packing
    // the last installation: an old anchor must never reappear on restart.
    GConfig->SetBool(Settings,TEXT("Installed"),false,GGameUserSettingsIni);
    LaunchInstallations.Save(GGameUserSettingsIni,ActiveBungeeId);
    SyncWinchStations();
}

void ASlopeFlightPawn::LoadBungee()
{
    GConfig->GetFloat(Settings,TEXT("WinchPower"),WinchMaximumPower,GGameUserSettingsIni);
    WinchMaximumPower=FMath::Clamp(WinchMaximumPower,.25f,1.5f);
    GConfig->GetInt(Settings,TEXT("Colour"),BungeeColourIndex,GGameUserSettingsIni);
    BungeeColourIndex = FMath::Clamp(BungeeColourIndex,0,int32(UE_ARRAY_COUNT(Colours))-1);
    Bungee=FBungeeLaunch();
    if(LaunchInstallations.Load(GGameUserSettingsIni,ActiveBungeeId))
    {
        ActivateBungee(ActiveBungeeId);
        SyncWinchStations();
        return;
    }
    bool Installed = false;
    GConfig->GetBool(Settings, TEXT("Installed"), Installed, GGameUserSettingsIni);
    FBungeeLaunch Saved;
    if (!Installed || !GConfig->GetVector(Settings, TEXT("Anchor"), Saved.Anchor, GGameUserSettingsIni)
        || !GConfig->GetVector(Settings, TEXT("Departure"), Saved.Departure, GGameUserSettingsIni)
        || !GConfig->GetDouble(Settings, TEXT("Stiffness"), Saved.Stiffness, GGameUserSettingsIni)) return;
    const double Length = FVector::Distance(Saved.Anchor, Saved.Departure)*.01;
    if (Saved.Anchor.ContainsNaN() || Saved.Departure.ContainsNaN() || !FMath::IsFinite(Saved.Stiffness)
        || Saved.Stiffness <= 0. || Length < FBungeeLaunch::MinLength || Length > FBungeeLaunch::MaxLength+2.) return;
    Saved.State = State::Ready;
    Bungee = Saved;
    SaveBungee();
    EquipmentConfirmationText.Reset();
}

void ASlopeFlightPawn::ActivateBungee(int32 Id)
{
    if(const auto* E=LaunchInstallations.Find(Id))
    {
        ActiveBungeeId=Id;
        Bungee=FBungeeLaunch(); Bungee.State=State::Ready;
        Bungee.Type=E->Type;
        Bungee.Anchor=E->Anchor; Bungee.Departure=E->Departure; Bungee.Stiffness=E->Stiffness;
        BungeeColourIndex=E->Colour;
    }
}

void ASlopeFlightPawn::SelectNearbyBungee()
{
    if(bBungeeSelected || Bungee.Connected() || Bungee.State==State::LayingOut) return;
    const FVector Here=PilotPosition();
    int32 Nearest=INDEX_NONE;
    double Distance=ActiveBungeeId==INDEX_NONE ? TNumericLimits<double>::Max() : ReachCm;
    for(const auto& E:LaunchInstallations.Entries())
    {
        const double D=FMath::Min(FVector::Distance(Here,E.Anchor),FVector::Distance(Here,E.Departure));
        if(D<Distance) { Distance=D; Nearest=E.Id; }
    }
    if(Nearest!=INDEX_NONE && Nearest!=ActiveBungeeId) ActivateBungee(Nearest);
}

FString ASlopeFlightPawn::BackpackAction() const
{
    if (bInBackpack) return TEXT("Sortir le planeur");
    if (bHeldForLaunch) return TEXT("Planeur au sac");
    if (Flight.IsStopped()) return TEXT("Récupérer le planeur");
    return TEXT("Sac indisponible en vol");
}

void ASlopeFlightPawn::ToggleBackpack()
{
    if (bMenuOpen || bNoclip || IsThrowing() || bGroundPhysics) return;
    SelectNearbyBungee();
    if (!bHeldForLaunch)
    {
        if (!Flight.IsStopped()) { SiteNotice = TEXT("Le planeur est en vol."); return; }
        if (FVector::Dist2D(PilotPosition(), GetActorLocation()) > ReachCm)
        { SiteNotice = TEXT("Approche-toi du planeur posé pour le récupérer."); return; }
        ResetFlight();
        bInBackpack = true;
    }
    else
    {
        Bungee.Release();
        bInBackpack = !bInBackpack;
    }
    PlaneVisual->SetHiddenInGame(bInBackpack, true);
    FlightView = EFlightView::Ground;
    SiteNotice = bInBackpack ? TEXT("Planeur rangé · Maj pour courir") : TEXT("Planeur en main · Espace pour lancer");
    if (!bInBackpack && Bungee.State == State::Ready && FVector::Distance(PilotPosition(),Bungee.Departure)<ReachCm)
        InteractEquipment(false);
}

void ASlopeFlightPawn::SelectBungee()
{
    SelectLaunchEquipment(ELaunchEquipment::Bungee);
}

void ASlopeFlightPawn::SelectLaunchEquipment(ELaunchEquipment Type)
{
    if (!IsWalking()) { SiteNotice = TEXT("Reviens au sol pour préparer le matériel."); return; }
    F3FRun.Invalidate(TEXT("Changement de matériel"));
    bF3FEnabled=false; F3FPlacement=0;
    bBungeeSelected = !bBungeeSelected || Bungee.Type!=Type;
    Bungee.Release();
    if(!Bungee.Connected())
    {
        // F1 / F4 start a fresh layout. Existing installations stay in the registry
        // until a valid new departure commits the placement/replacement.
        Bungee=FBungeeLaunch(); ActiveBungeeId=INDEX_NONE;
        Bungee.Type=Type;
        if(!bBungeeSelected) SelectNearbyBungee();
    }
    if (bBungeeSelected && bHeldForLaunch && !bInBackpack) ToggleBackpack();
    SiteNotice = bBungeeSelected ? (Bungee.IsWinch()?TEXT("Treuil sélectionné · E pour poser la machine puis le départ"):TEXT("Élastique sélectionné · E pour poser / utiliser · E maintenu pour ranger")) : TEXT("Matériel désélectionné");
}

bool ASlopeFlightPawn::NearEquipmentEnd() const
{
    if (!Bungee.Installed()) return false;
    const FVector Here = PilotPosition();
    return FVector::Distance(Here, Bungee.Anchor) < ReachCm
        || (Bungee.State != State::LayingOut && FVector::Distance(Here, Bungee.Departure) < ReachCm);
}

bool ASlopeFlightPawn::ClearBungeeRoute(const FVector& End) const
{
    // The line must clear the relief and solid scenery between the two points.
    FHitResult Hit;
    const FVector Start = Bungee.Anchor + FVector(0,0,30);
    const FVector Finish = End + FVector(0,0,145);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BungeePlacement), false, this);
    if(auto* Station=WinchStations.FindRef(ActiveBungeeId).Get()) Query.AddIgnoredActor(Station);
    return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Finish, SceneContact::Channel,Query);
}

void ASlopeFlightPawn::SyncWinchStations()
{
    for(auto It=WinchStations.CreateIterator();It;++It)
        if(!LaunchInstallations.Find(It.Key()))
        { if(IsValid(It.Value())) It.Value()->Destroy(); It.RemoveCurrent(); }
    for(const auto& Entry:LaunchInstallations.Entries())
    {
        if(Entry.Type!=ELaunchEquipment::Winch) continue;
        auto* Station=WinchStations.FindRef(Entry.Id).Get();
        const FRotator Facing(0,(Entry.Departure-Entry.Anchor).Rotation().Yaw,0);
        if(!IsValid(Station))
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Station=GetWorld()->SpawnActor<AWinchStation>(Entry.Anchor,Facing,Params);
            WinchStations.Add(Entry.Id,Station);
        }
        else Station->SetActorLocationAndRotation(Entry.Anchor,Facing);
    }
}

FString ASlopeFlightPawn::DeparturePlacementError() const
{
    const double Length = FVector::Distance(PilotPosition(), Bungee.Anchor)*.01;
    if (Length < Bungee.MinimumLength()) return FString::Printf(TEXT("Éloigne-toi à au moins %.0f m de l’ancrage"),Bungee.MinimumLength());
    if (Length > Bungee.MaximumLength()) return FString::Printf(TEXT("Ligne trop courte · rapproche-toi à moins de %.0f m"),Bungee.MaximumLength());
    return {};
}

FVector ASlopeFlightPawn::EquipmentLineEnd() const
{
    if (Bungee.State == State::Attached) return BungeeHandPose().GetLocation();
    if (Bungee.State == State::Towing) return GetActorLocation();
    if (Bungee.State == State::LayingOut) return PilotPosition();
    return Bungee.Departure;
}

FTransform ASlopeFlightPawn::BungeeHandPose() const
{
    const FVector Forward = FRotator(0,LaunchHeading,0).Vector();
    return FTransform(FRotator(12,LaunchHeading,0), PilotPosition()+Forward*75.+FVector(0,0,145));
}

double ASlopeFlightPawn::BungeeTension() const
{
    if (Bungee.State == State::Towing)
    {
        const FVector V = Flight.VelocityNED();
        return Bungee.ForceWorld(Flight.WorldPose().GetLocation(), FVector(V.X,V.Y,-V.Z)).Size();
    }
    if (Bungee.State == State::Attached) return Bungee.ForceWorld(BungeeHandPose().GetLocation(), FVector::ZeroVector).Size();
    return 0.;
}

FVector ASlopeFlightPawn::BungeeForceNED()
{
    if (Bungee.State != State::Towing) return FVector::ZeroVector;
    const FVector P = Flight.WorldPose().GetLocation();
    const FVector Velocity=Flight.VelocityNED();
    Bungee.AdvanceTow(FGliderDynamics::TimeStep,P,FVector(Velocity.X,Velocity.Y,-Velocity.Z),WinchCommand());
    const auto Reason = Bungee.ReleaseCondition(P);
    if (Reason != FBungeeLaunch::ERelease::None)
    {
        Bungee.Release();
        switch (Reason)
        {
        case FBungeeLaunch::ERelease::Angle:
            SiteNotice = TEXT("Crochet libéré automatiquement · angle de ligne atteint"); break;
        case FBungeeLaunch::ERelease::Overstretch:
            SiteNotice = TEXT("Sécurité · ligne larguée en surtension"); break;
        case FBungeeLaunch::ERelease::EndOfLine:
            SiteNotice = TEXT("Treuil arrêté · fin de treuillée · crochet libéré"); break;
        default:
            SiteNotice = TEXT("Ligne détendue · largage automatique"); break;
        }
        return FVector::ZeroVector;
    }
    const FVector V = Flight.VelocityNED();
    const FVector Force = Bungee.ForceWorld(P, FVector(V.X,V.Y,-V.Z));
    return FVector(Force.X, Force.Y, -Force.Z);
}

FString ASlopeFlightPawn::EquipmentPrompt() const
{
    if (Bungee.State == State::Towing) return Bungee.IsWinch()
        ? FString::Printf(TEXT("TREUIL · puissance %.0f %% · E pour larguer"),WinchCommand()*100.) : TEXT("ÉLASTIQUE · E pour larguer");
    if (!IsWalking()) return {};
    if (Bungee.State == State::Attached)
    {
        if(Bungee.IsWinch()) return TEXT("TREUIL ACCROCHÉ · Espace démarrer · WASD / Maj ajuster · E détacher");
        return BungeeTension() < Bungee.Stiffness*5.
            ? TEXT("PLANEUR ACCROCHÉ · recule pour tendre · Maj courir · E détacher")
            : TEXT("PLANEUR ACCROCHÉ · WASD / Maj pour ajuster · Espace lancer · E détacher");
    }
    if (NearEquipmentEnd())
    {
        if (Bungee.State == State::Ready && FVector::Distance(PilotPosition(),Bungee.Departure)<ReachCm)
            return bInBackpack ? TEXT("DÉPART POSÉ · E ou F3 pour sortir et accrocher   |   Maintenir E pour ranger")
                : TEXT("DÉPART POSÉ · E pour accrocher le planeur   |   Maintenir E pour ranger");
        return TEXT("Maintenir E · Ranger l’installation");
    }
    if (bBungeeSelected && Bungee.State == State::Packed)
        return bPlacementValid ? (Bungee.IsWinch()?TEXT("E · Poser le treuil sur le repère"):TEXT("E · Poser l’ancrage sur le repère")) : TEXT("Choisis un sol ferme et accessible pour l’ancrage");
    if (bBungeeSelected && Bungee.State == State::LayingOut)
    {
        const double Length = FVector::Distance(PilotPosition(), Bungee.Anchor)*.01;
        const FString Error = DeparturePlacementError();
        return Error.IsEmpty() ? FString::Printf(TEXT("DÉROULEMENT · E pour poser le départ ici · %.0f m · Maj courir"), Length)
            : FString::Printf(TEXT("DÉPART NON POSÉ · %s · %.0f m"), *Error, Length);
    }
    if (Bungee.State == State::Ready)
        return FString::Printf(TEXT("DÉPART POSÉ · rejoins le repère à %.0f m pour accrocher le planeur"), FVector::Distance(PilotPosition(),Bungee.Departure)*.01);
    return {};
}

void ASlopeFlightPawn::InteractEquipment(bool Pack)
{
    if (!IsWalking() || IsThrowing()) return;
    if (Pack)
    {
        if (!NearEquipmentEnd()) return;
        LaunchInstallations.Remove(ActiveBungeeId);
        const auto Type=Bungee.Type;
        ActiveBungeeId=INDEX_NONE;
        Bungee = FBungeeLaunch();
        SaveBungee();
        SiteNotice = TEXT("Matériel et ligne rangés dans le sac");
        EquipmentConfirmationText=FString::Printf(TEXT("Matériel rangé · %d / 10 %s restent installés"),LaunchInstallations.Count(Type),Type==ELaunchEquipment::Winch?TEXT("treuils"):TEXT("élastiques"));
        EquipmentConfirmationUntil=FPlatformTime::Seconds()+5.;
        return;
    }
    if (Bungee.State == State::Attached)
    {
        Bungee.Release();
        SiteNotice = TEXT("Planeur détaché · installation conservée");
        return;
    }
    if (Bungee.State == State::Ready && FVector::Distance(PilotPosition(),Bungee.Departure)<ReachCm)
    {
        if (!bHeldForLaunch) { SiteNotice = TEXT("Récupère d’abord ton planeur."); return; }
        bInBackpack = false;
        PlaneVisual->SetHiddenInGame(false,true);
        LaunchHeading = (Bungee.Anchor-PilotPosition()).Rotation().Yaw;
        WalkPitch = -8.f;
        Bungee.State = State::Attached;
        if(Bungee.IsWinch()) Bungee.Winch.Start(FVector::Distance(PilotPosition(),Bungee.Anchor)*.01,Flight.Loading().MassGrams*.001);
        SiteNotice = Bungee.IsWinch()?TEXT("Treuil prêt · Espace lance la traction progressive · cabre doucement pour monter"):TEXT("Planeur accroché · recule pour tendre la ligne et choisir l’angle");
        return;
    }
    if (!bBungeeSelected || !bHeldForLaunch) return;
    if (Bungee.State == State::Packed)
    {
        if (!bPlacementValid) { SiteNotice = TEXT("Pose impossible ici."); return; }
        Bungee.PlaceAnchor(PlacementPoint);
        SiteNotice = Bungee.IsWinch()?TEXT("Treuil posé · déroule entre 40 et 400 m · E pour poser le départ"):TEXT("Ancrage posé · déroule la ligne · E pour poser le départ · puis accroche et tends");
    }
    else if (Bungee.State == State::LayingOut)
    {
        const FString Error = DeparturePlacementError();
        if (!Error.IsEmpty()) { SiteNotice = Error; return; }
        if (!Bungee.SetDeparture(PilotPosition(), Flight.Loading().MassGrams*.001))
        { SiteNotice = TEXT("Pose du départ impossible."); return; }
        SaveBungee();
        bBungeeSelected = false;
        SiteNotice = TEXT("Départ posé et enregistré · la ligne reste au sol · E ou F3 pour accrocher");
    }
}

void ASlopeFlightPawn::HandleEquipmentInput(bool Down, bool Pressed, bool Released, float DeltaSeconds)
{
    if (!IsWalking()) { bInteractionStarted = false; return; }
    if (Pressed)
    {
        InteractionSeconds = 0.f;
        bInteractionConsumed = false;
        bInteractionStarted = true;
        bInteractionCanPack = NearEquipmentEnd();
    }
    if (!bInteractionStarted) return;
    if (Down) InteractionSeconds += DeltaSeconds;
    if (InteractionSeconds >= 1.f && bInteractionCanPack && !bInteractionConsumed && NearEquipmentEnd())
    {
        InteractEquipment(true);
        bInteractionConsumed = true;
    }
    // A press and release can both arrive within a single slow render frame.
    if (Released || !Down)
    {
        if (!bInteractionConsumed) InteractEquipment(false);
        bInteractionStarted = false;
        InteractionSeconds = 0.f;
    }
}

void ASlopeFlightPawn::UpdateEquipment(float DeltaSeconds)
{
    EquipmentLines->Flush();
    if (bMenuOpen || bNoclip)
    {
        InteractionSeconds = 0.f;
        bInteractionConsumed = true;
        bInteractionStarted = false;
        return;
    }
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    if(IsWalking()) SelectNearbyBungee();
    bPlacementValid = false;
    if (IsWalking() && bBungeeSelected && Bungee.State == State::Packed)
    {
        const double Reach = FMath::Clamp(170./FMath::Tan(FMath::DegreesToRadians(FMath::Max(20.f,-WalkPitch))), 100., 450.);
        FHitResult Ground;
        const FVector Ahead = PilotPosition()+FRotator(0,LaunchHeading,0).Vector()*Reach;
        if (FindPilotGround(Ahead,Ground))
        {
            PlacementPoint = Ground.ImpactPoint;
            FHitResult Obstacle;
            bPlacementValid = Ground.ImpactNormal.Z > .65 && !GetWorld()->LineTraceSingleByChannel(Obstacle,
                PilotPosition()+FVector(0,0,100), PlacementPoint+FVector(0,0,30), SceneContact::Channel,
                FCollisionQueryParams(SCENE_QUERY_STAT(BungeeMarker),false,this));
        }
        else PlacementPoint = Ahead;
    }
    if(!bF3FEnabled) HandleEquipmentInput(PC->IsInputKeyDown(EKeys::E), PC->WasInputKeyJustPressed(EKeys::E),
        PC->WasInputKeyJustReleased(EKeys::E), DeltaSeconds);
    const FLinearColor Gold(1.f,.68f,.2f), Coral(1.f,.25f,.17f), Green(.25f,.85f,.55f);
    auto Line = [&](const FVector& A,const FVector& B,FLinearColor Color,float Width)
    { EquipmentLines->DrawLine(A,B,Color,0,Width,0.f); };
    auto Mark = [&](const FVector& Point,FLinearColor Color)
    {
        const FVector Centre = Point+FVector(0,0,8);
        for (int I=0; I<32; ++I)
        {
            const double A = UE_TWO_PI*I/32., B = UE_TWO_PI*(I+1)/32.;
            Line(Centre+FVector(FMath::Cos(A)*65,FMath::Sin(A)*65,0),Centre+FVector(FMath::Cos(B)*65,FMath::Sin(B)*65,0),Color,3);
        }
        Line(Centre-FVector(30,0,0),Centre+FVector(30,0,0),Color,2);
        Line(Centre-FVector(0,30,0),Centre+FVector(0,30,0),Color,2);
        Line(Centre,Centre+FVector(0,0,85),Color,5);
        Line(Centre+FVector(0,0,85),Centre+FVector(35,0,65),Color,5);
    };
    if (bBungeeSelected && Bungee.State == State::Packed && IsWalking()) Mark(PlacementPoint,bPlacementValid?Green:Coral);
    FVector Eye; FRotator View;
    PC->GetPlayerViewPoint(Eye,View);
    auto DrawInstallation=[&](const FVector& Anchor,const FVector& End,bool Connected,FLinearColor Colour,bool Winch)
    {
        FVector Previous=Anchor+FVector(0,0,8);
        for(int I=1;I<=64;++I)
        {
            FVector Point=FMath::Lerp(Anchor,End,I/64.);
            if(!Connected) Point.Z=ASlopeWorld::TerrainHeightCm(Point.X,Point.Y)+8.;
            Line(Previous,Point,Colour,!Winch && I<=21?4.f:2.f); Previous=Point;
        }
    };
    for(const auto& E:LaunchInstallations.Entries())
    {
        if(E.Id==ActiveBungeeId
            || FVector::DistSquared2D(E.Anchor,Eye)>FMath::Square(60000.)) continue;
        Mark(E.Anchor,Gold); Mark(E.Departure,Green);
        DrawInstallation(E.Anchor,E.Departure,false,EquipmentColour(E.Colour),E.Type==ELaunchEquipment::Winch);
    }
    if(!Bungee.Installed()) return;
    Mark(Bungee.Anchor,Gold);
    if(Bungee.State!=State::LayingOut) Mark(Bungee.Departure,Green);
    else Mark(PilotPosition(),DeparturePlacementError().IsEmpty()?Green:Coral);
    DrawInstallation(Bungee.Anchor,EquipmentLineEnd(),Bungee.Connected(),BungeeColour(),Bungee.IsWinch());
    if(auto* Station=WinchStations.FindRef(ActiveBungeeId).Get(); Station && Bungee.IsWinch())
        Station->TurnDrum(Bungee.State==State::Towing ? Bungee.Winch.ReelSpeed*DeltaSeconds : 0.);
}
