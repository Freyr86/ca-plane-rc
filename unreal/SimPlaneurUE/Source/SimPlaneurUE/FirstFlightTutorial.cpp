#include "FirstFlightTutorial.h"
#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "FlightMenu.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraTypes.h"
#include "Misc/ConfigCacheIni.h"

void ASlopeFlightPawn::StartFirstFlightTutorial()
{
    if(!bModelReady || !bAppearanceReady || !bMenuOpen) return;
    if(IsTutorialActive()) EndFirstFlightTutorial();
    FHitResult Spawn, Target, Sock;
    const FVector Start(ASlopeWorld::ChaletX+430,ASlopeWorld::ChaletY,0);
    // Provisional point, easily replaced after the pilot's site reconnaissance.
    double X=(ASlopeWorld::ChaletX+2000)*.01, Y=(ASlopeWorld::ChaletY+450)*.01;
    GConfig->GetDouble(TEXT("SimPlaneur.Tutorial"),TEXT("WalkX"),X,GGameUserSettingsIni);
    GConfig->GetDouble(TEXT("SimPlaneur.Tutorial"),TEXT("WalkY"),Y,GGameUserSettingsIni);
    if(!FindPilotGround(Start,Spawn) || !FindPilotGround(FVector(X*100,Y*100,0),Target)
        || !FindPilotGround(FVector(ASlopeWorld::ChaletX+950,ASlopeWorld::ChaletY+450,0),Sock)) return;
    TutorialPreviousWeather=ValleyAir::Settings();
    TutorialPreviousFlight=Flight;
    TutorialPreviousModel=SelectedModel; TutorialPreviousPreview=PreviewModel;
    TutorialPreviousSite=SelectedSite; TutorialPreviousPilot=PilotSite; TutorialPreviousOffset=PilotOffset;
    TutorialPreviousHeading=LaunchHeading; TutorialPreviousPitch=WalkPitch; TutorialPreviousView=int32(FlightView);
    TutorialPreviousTrails=bShowWindTrails; TutorialPreviousF3F=bF3FEnabled;
    TutorialPreviousBungee=Bungee; TutorialPreviousBungeeId=ActiveBungeeId;
    Tutorial=FFirstFlightTutorial(); Tutorial.Go(EFirstFlightStep::Walk);
    Tutorial.WalkTarget=Tutorial.LaunchPoint=Target.ImpactPoint;
    Tutorial.WindsockTarget=Sock.ImpactPoint+FVector(0,0,370);
    Tutorial.LandingTarget=Target.ImpactPoint;
    // Aim the reference wind uphill, making the launch direction face the breeze.
    const FVector P=Target.ImpactPoint;
    const FVector Down(ASlopeWorld::TerrainHeightCm(P.X-2500,P.Y)-ASlopeWorld::TerrainHeightCm(P.X+2500,P.Y),
        ASlopeWorld::TerrainHeightCm(P.X,P.Y-2500)-ASlopeWorld::TerrainHeightCm(P.X,P.Y+2500),0);
    TutorialHeading=Down.IsNearlyZero()?0.f:Down.Rotation().Yaw;
    TutorialWeather=ValleyAir::FWeatherSettings();
    // A light headwind suits the Topy's low flying speed for a first lesson.
    TutorialWeather.WindSpeedMps=3.; TutorialWeather.WindFromDegrees=FRotator::ClampAxis(TutorialHeading);
    TutorialWeather.ThermalStrength=.25; TutorialWeather.Turbulence=.015;
    TutorialWeather.Sky=0; TutorialWeather.CloudCoverage=.2; TutorialWeather.LowClouds=false;
    TutorialWeather.RainStrength=0; TutorialWeather.SolarMinutes=14*60;
    ValleyAir::SetSettings(TutorialWeather); // Session only: never save the teaching preset.
    SelectedModel=PreviewModel=0; Flight=HangarModels[0];
    Flight.SetMotor({false,0});
    SelectedSite=2; PilotSite=Spawn.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
    LaunchHeading=(Target.ImpactPoint-Spawn.ImpactPoint).Rotation().Yaw; WalkPitch=0;
    Bungee=FBungeeLaunch(); ActiveBungeeId=INDEX_NONE; bF3FEnabled=false; F3FPlacement=0;
    BuildFallbackTopy(); ResetFlight();
    bHeldForLaunch=true; bInBackpack=true; bShowWindTrails=false;
    SiteNotice.Reset();
    // New model textures may need a frame before Fly can close the menu.
    bTutorialStarting=true;
    if(bAppearanceReady) { Fly(); bTutorialStarting=bMenuOpen; PlaneVisual->SetHiddenInGame(true,true); }
}

void ASlopeFlightPawn::EndFirstFlightTutorial()
{
    if(!IsTutorialActive()) return;
    Tutorial=FFirstFlightTutorial();
    bTutorialStarting=false;
    ValleyAir::SetSettings(TutorialPreviousWeather);
    // Restore any weather/trail preferences saved from the paused lesson menu.
    ValleyAir::SaveSettings();
    bShowWindTrails=TutorialPreviousTrails;
    GConfig->SetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowTrails"),bShowWindTrails,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    SelectedModel=TutorialPreviousModel; PreviewModel=TutorialPreviousPreview;
    Flight=TutorialPreviousFlight; SelectedSite=TutorialPreviousSite;
    PilotSite=TutorialPreviousPilot; PilotOffset=TutorialPreviousOffset;
    LaunchHeading=TutorialPreviousHeading; WalkPitch=TutorialPreviousPitch;
    Bungee=TutorialPreviousBungee; ActiveBungeeId=TutorialPreviousBungeeId;
    bF3FEnabled=TutorialPreviousF3F;
    BuildFallbackTopy(); ResetFlight(); FlightView=EFlightView(TutorialPreviousView);
    bStartedFlight=false; SiteNotice.Reset();
}

void ASlopeFlightPawn::RecoverFirstFlightTutorial()
{
    if(!IsTutorialActive()) return;
    // No shortcut may bypass the ground introduction or the final retrieval.
    if(Tutorial.Step<EFirstFlightStep::Launch || Tutorial.Step==EFirstFlightStep::Complete) return;
    PilotSite=Tutorial.LaunchPoint; PilotOffset=FVector2D::ZeroVector;
    LaunchHeading=TutorialHeading; WalkPitch=0;
    ResetFlight(); bHeldForLaunch=true; bInBackpack=false;
    ValleyAir::SetSettings(TutorialWeather);
    Tutorial.LandingTarget=Tutorial.WalkTarget;
    Tutorial.bRecoveryNeeded=Tutorial.bCrashed=false;
    Tutorial.bRoughLanding=false;
    Tutorial.Go(EFirstFlightStep::Launch);
    PlaneVisual->SetHiddenInGame(false,true);
    SiteNotice.Reset();
}

void ASlopeFlightPawn::UpdateFirstFlightTutorial(float Dt)
{
    using S=EFirstFlightStep;
    if(!IsTutorialActive() || bMenuOpen || !bAppearanceReady) return;
    Dt=FMath::Clamp(Dt,0.f,.1f);
    Tutorial.StageSeconds+=Dt;
    if(Tutorial.Step==S::Walk)
    {
        if(FVector::Dist2D(PilotPosition(),Tutorial.WalkTarget)<200.) Tutorial.Go(S::Windsock);
    }
    else if(Tutorial.Step==S::Windsock)
    {
        FMinimalViewInfo View; CalcCamera(0,View);
        const FVector To=Tutorial.WindsockTarget-View.Location;
        FHitResult Hit;
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,View.Location,Tutorial.WindsockTarget,
            SceneContact::Channel,FCollisionQueryParams(SCENE_QUERY_STAT(TutorialSock),false,this))
            && FVector::Distance(Hit.ImpactPoint,Tutorial.WindsockTarget)>100.;
        const bool Looking=To.Size()<8000. && FVector::DotProduct(View.Rotation.Vector(),To.GetSafeNormal())>.965 && !Blocked;
        Tutorial.LostSeconds=Looking?Tutorial.LostSeconds+Dt:0;
        if(Tutorial.LostSeconds>=1.5f) Tutorial.Go(S::Airflow);
    }
    else if(Tutorial.Step==S::Airflow)
    {
        Tutorial.LostSeconds=bShowWindTrails?Tutorial.LostSeconds+Dt:0;
        if(Tutorial.LostSeconds>=2.f) Tutorial.Go(S::Controls);
    }
    else if(Tutorial.Step==S::Controls)
    {
        if(Tutorial.ControlsReady()) Tutorial.Go(S::Launch);
    }
    else if(Tutorial.Step==S::Launch)
    {
        if(!bHeldForLaunch && !Flight.IsStopped())
        {
            Tutorial.LaunchPoint=PilotPosition();
            Tutorial.Go(Tutorial.FlightSeconds>=150.f?S::Landing:S::Fly);
        }
    }
    else if(Tutorial.Step==S::Fly || Tutorial.Step==S::Landing)
    {
        // A firm ground landing is a learning moment, not a failed lesson.
        // Crashes and obstacle/water contacts still require recovery.
        const bool BadContact=ImpactDamage>=2 || ContactKind==TEXT("tree") || ContactKind==TEXT("water")
            || (ImpactDamage>0 && ContactKind!=TEXT("ground"));
        if(BadContact) { Tutorial.bRecoveryNeeded=true; Tutorial.bCrashed=true; }
        if(!Tutorial.bRecoveryNeeded && !Flight.IsStopped() && !bGroundPhysics)
        {
            // Test visibility from the pilot, not the current camera direction:
            // looking away momentarily is not losing the aircraft.
            const FVector Eye=PilotPosition()+FVector(0,0,170), Plane=GetActorLocation();
            FHitResult Hit;
            const bool Occluded=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Plane,
                SceneContact::Channel,FCollisionQueryParams(SCENE_QUERY_STAT(TutorialSight),false,this))
                && FVector::Distance(Hit.ImpactPoint,Plane)>200.;
            const bool Lost=FVector::Distance(Eye,Plane)>35000. || Occluded;
            Tutorial.LostSeconds=Lost?Tutorial.LostSeconds+Dt:0;
            if(Tutorial.LostSeconds>5.f) Tutorial.bRecoveryNeeded=true;
            else Tutorial.FlightSeconds+=Dt;
        }
        if(Tutorial.bRecoveryNeeded) return;
        if(Tutorial.Step==S::Fly && Tutorial.FlightSeconds>=150.f) Tutorial.Go(S::Landing);
        if(Tutorial.Step==S::Landing)
        {
            // Announce first, then fade the real world wind over twenty seconds.
            const float T=FMath::SmoothStep(0.f,1.f,FMath::Clamp((Tutorial.StageSeconds-5.f)/20.f,0.f,1.f));
            auto Weather=TutorialWeather;
            Weather.WindSpeedMps=FMath::Lerp(TutorialWeather.WindSpeedMps,1.2,double(T));
            Weather.ThermalStrength=FMath::Lerp(TutorialWeather.ThermalStrength,.10,double(T));
            ValleyAir::SetSettings(Weather);
        }
        if(Flight.IsStopped() && !bGroundPhysics && !bHeldForLaunch)
        {
            Tutorial.bRoughLanding=ImpactDamage==1;
            Tutorial.LandingTarget=GetActorLocation();
            Tutorial.Go(S::Retrieve);
        }
    }
    else if(Tutorial.Step==S::Retrieve && bInBackpack)
    {
        if(Tutorial.FlightSeconds>=150.f) Tutorial.Go(S::Complete);
        else { RecoverFirstFlightTutorial(); } // Keep accumulated airtime on an early landing.
    }
}
