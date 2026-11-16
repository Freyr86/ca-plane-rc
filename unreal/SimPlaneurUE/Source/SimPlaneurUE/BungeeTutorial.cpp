#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "Components/SceneComponent.h"
#include "SceneContact.h"

void ASlopeFlightPawn::StartBungeeTutorial()
{
    if(!bModelReady || !bAppearanceReady || !bMenuOpen) return;
    FHitResult Spawn, Anchor;
    if(!FindPilotGround(FVector(Airfield::Point(3250,0),0),Spawn)
        || !FindPilotGround(FVector(Airfield::Point(3550,0),0),Anchor)) return;
    StartFirstFlightTutorial(); // Shared snapshot and deferred texture/menu setup.
    if(!IsTutorialActive()) return;
    Tutorial=FFirstFlightTutorial(); Tutorial.Go(EBungeeTutorialStep::Select);
    Tutorial.BungeeAnchorTarget=Anchor.ImpactPoint;
    Tutorial.WalkTarget=Spawn.ImpactPoint;
    TutorialHeading=135.f;
    TutorialWeather.WindSpeedMps=2.; TutorialWeather.WindFromDegrees=TutorialHeading;
    TutorialWeather.ThermalStrength=0.; TutorialWeather.Turbulence=.01;
    ValleyAir::SetSettings(TutorialWeather);
    SelectedModel=PreviewModel=7; Flight=HangarModels[7];
    Flight.SetMotor({false,0});
    SelectedSite=3; PilotSite=Spawn.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
    LaunchHeading=TutorialHeading; WalkPitch=-34.f;
    Flight.SetLoading(Flight.StandardLoading()); Flight.SetControlGains({});
    Bungee=FBungeeLaunch(); ActiveBungeeId=INDEX_NONE;
    LaunchInstallations=FLaunchInstallations(); SyncWinchStations();
    BuildFallbackTopy(); ResetFlight(); bInBackpack=true;
    bTutorialStarting=true;
    PlaneVisual->SetHiddenInGame(true,true);
    SiteNotice.Reset(); EquipmentConfirmationText.Reset();
}

bool ASlopeFlightPawn::BungeeTutorialReady() const
{
    if(Bungee.State!=FBungeeLaunch::EState::Attached || bInBackpack) return false;
    const double Weight=Flight.Loading().MassGrams*.001*9.80665;
    const double Ratio=BungeeTension()/FMath::Max(.01,Weight);
    const double Heading=(Bungee.Anchor-PilotPosition()).Rotation().Yaw;
    return Ratio>=2. && Ratio<=4. && ClearBungeeRoute(PilotPosition())
        && FMath::Abs(FMath::FindDeltaAngleDegrees(LaunchHeading,Heading))<=20.;
}

void ASlopeFlightPawn::RecoverBungeeTutorial()
{
    using S=EBungeeTutorialStep;
    if(Tutorial.BungeeStep==S::Complete) return;
    const bool Installed=Bungee.Installed() && Bungee.State!=FBungeeLaunch::EState::LayingOut;
    PilotSite=Installed?Bungee.Departure:Tutorial.WalkTarget;
    PilotOffset=FVector2D::ZeroVector; LaunchHeading=TutorialHeading; WalkPitch=-20.f;
    ResetFlight(); bInBackpack=true; PlaneVisual->SetHiddenInGame(true,true);
    if(!Installed) { Bungee=FBungeeLaunch(); ActiveBungeeId=INDEX_NONE; }
    Tutorial.bRecoveryNeeded=Tutorial.bCrashed=Tutorial.bRoughLanding=false;
    Tutorial.FlightSeconds=0;
    Tutorial.Go(Installed?S::Attach:S::Select);
    SiteNotice.Reset();
}

void ASlopeFlightPawn::UpdateBungeeTutorial(float Dt)
{
    using S=EBungeeTutorialStep; using B=FBungeeLaunch::EState;
    if(!IsBungeeTutorialActive() || bMenuOpen || !bAppearanceReady) return;
    Dt=FMath::Clamp(Dt,0.f,.1f); Tutorial.StageSeconds+=Dt;
    auto GroundBehind=[&](double Distance)
    {
        const FVector Point=Bungee.Anchor-FRotator(0,TutorialHeading,0).Vector()*Distance;
        FHitResult Ground;
        return FindPilotGround(Point,Ground)?Ground.ImpactPoint:Point;
    };
    switch(Tutorial.BungeeStep)
    {
    case S::Select:
        if(bBungeeSelected && !Bungee.IsWinch()) Tutorial.Go(S::Anchor);
        break;
    case S::Anchor:
        if(Bungee.State==B::LayingOut)
        {
            Tutorial.BungeeDepartureTarget=GroundBehind(3000.);
            Tutorial.LaunchPoint=GroundBehind(6800.);
            Tutorial.Go(S::Layout);
        }
        break;
    case S::Layout:
        if(Bungee.State==B::Ready) Tutorial.Go(S::Attach);
        break;
    case S::Attach:
        if(Bungee.State==B::Attached) Tutorial.Go(S::Tension);
        break;
    case S::Tension:
        if(Bungee.State==B::Ready) Tutorial.Go(S::Attach);
        else if(BungeeTutorialReady()) Tutorial.Go(S::Launch);
        break;
    case S::Launch:
        if(Bungee.State==B::Towing && !bHeldForLaunch) Tutorial.Go(S::Tow);
        else if(!BungeeTutorialReady()) Tutorial.Go(S::Tension);
        break;
    case S::Tow:
    case S::Glide:
    {
        const bool BadContact=ImpactDamage>=2 || ContactKind==TEXT("tree") || ContactKind==TEXT("water")
            || (ImpactDamage>0 && ContactKind!=TEXT("ground"));
        if(BadContact) { Tutorial.bRecoveryNeeded=Tutorial.bCrashed=true; break; }
        if(Tutorial.bRecoveryNeeded) break;
        if(!Flight.IsStopped() && !bGroundPhysics)
        {
            const FVector Eye=PilotPosition()+FVector(0,0,170), Plane=GetActorLocation();
            FHitResult Hit;
            const bool Hidden=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Plane,SceneContact::Channel,
                FCollisionQueryParams(SCENE_QUERY_STAT(BungeeTutorialSight),false,this))
                && FVector::Distance(Hit.ImpactPoint,Plane)>200.;
            const bool Lost=Hidden || FVector::Distance(Eye,Plane)>35000.;
            Tutorial.LostSeconds=Lost?Tutorial.LostSeconds+Dt:0;
            if(Tutorial.LostSeconds>5.f) Tutorial.bRecoveryNeeded=true;
            Tutorial.FlightSeconds+=Dt;
        }
        if(Tutorial.BungeeStep==S::Tow && !Bungee.Connected()) Tutorial.Go(S::Glide);
        if(Flight.IsStopped() && !bGroundPhysics && !bHeldForLaunch)
        {
            Tutorial.LandingTarget=GetActorLocation();
            Tutorial.bRoughLanding=ImpactDamage==1; Tutorial.Go(S::Retrieve);
        }
        break;
    }
    case S::Retrieve:
        if(bInBackpack) Tutorial.Go(S::Pack);
        break;
    case S::Pack:
        if(!Bungee.Installed()) { bBungeeSelected=false; Tutorial.Go(S::Complete); }
        break;
    default: break;
    }
}
