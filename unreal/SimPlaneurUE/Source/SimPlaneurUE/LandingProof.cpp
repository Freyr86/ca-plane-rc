#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "ProceduralMeshComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ASlopeFlightPawn::TestLanding(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if(!bAppearanceReady || Elapsed<5.+MenuProofStep*3.) return;
    bool OK=true;
    switch(MenuProofStep)
    {
    case 0:
    {
        SelectedModel=PreviewModel=0; Flight=HangarModels[0]; BuildFallbackTopy();
        FHitResult Ground;
        OK=FindPilotGround(FVector(Airfield::Point(0,0),0),Ground);
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
        ResetFlight(); bHeldForLaunch=false;
        MenuProofLocation=Ground.ImpactPoint;
        Flight.LaunchFrom(FTransform(FRotator(-35,135,15),Ground.ImpactPoint+FVector(0,0,120)),FVector(-10,10,-25));
        PreviousPose=Flight.WorldPose(); SetActorTransform(PreviousPose);
        break;
    }
    case 1:
        OK=ImpactDamage==2 && FVector::Dist2D(Flight.WorldPose().GetLocation(),MenuProofLocation)>100.;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/crash-impact.png"),true,false,false);
        break;
    case 2: case 3:
        OK=Flight.WorldPose().GetLocation().Z>MenuProofLocation.Z-100.;
        break;
    case 4:
        OK=ImpactDamage==2 && !bGroundPhysics && IsWalking();
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/crash-rest.png"),true,false,false);
        break;
    case 5:
    {
        ResetFlight(); bHeldForLaunch=false;
        const FVector P=MenuProofLocation+FVector(0,0,5);
        Flight.LaunchFrom(FTransform(FRotator(0,135,0),P),FVector(-6,6,-.5));
        FHitResult Hit; Hit.ImpactNormal=FVector::UpVector; Hit.ImpactPoint=MenuProofLocation;
        ContactKind=TEXT("ground");
        BeginGroundPhysics(Flight.WorldPose(),Hit,86,119,7.5/59.5,0,FVector4(0,0,0,0));
        OK=ImpactDamage==0 && bGroundPhysics;
        break;
    }
    case 6:
        OK=ImpactDamage==0 && FVector::Dist2D(Flight.WorldPose().GetLocation(),MenuProofLocation)>100.;
        break;
    case 7: case 8:
        OK=ImpactDamage==0;
        break;
    case 9:
        OK=ImpactDamage==0 && !bGroundPhysics;
        ResetFlight();
        OK &= ImpactDamage==0 && !LandingBody->IsSimulatingPhysics();
        break;
    case 10:
    {
        bHeldForLaunch=false;
        const FVector P=MenuProofLocation+FVector(0,0,2000);
        Flight.LaunchFrom(FTransform(FRotator(0,135,30),P),FVector(3,0,-1));
        FHitResult Hit; Hit.ImpactNormal=FVector(-1,0,0); Hit.ImpactPoint=P;
        ContactKind=TEXT("tree");
        BeginGroundPhysics(Flight.WorldPose(),Hit,86,119,7.5/59.5,0,FVector4(0,0,0,0));
        OK=ImpactDamage==1 && bGroundPhysics;
        bMenuOpen=true; UpdateGroundPhysics(0.f);
        OK &= !LandingBody->IsSimulatingPhysics();
        bMenuOpen=false; UpdateGroundPhysics(0.f);
        OK &= LandingBody->IsSimulatingPhysics() && LandingBody->GetPhysicsLinearVelocity().Equals(FVector(300,0,-100),.1);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/damage-falling.png"),true,false,false);
        break;
    }
    case 11:
        OK=ImpactDamage==2 && Flight.WorldPose().GetLocation().Z<MenuProofLocation.Z+500.;
        break;
    case 12:
        OK=ImpactDamage==2 && !bGroundPhysics && IsWalking();
        break;
    default:
        UE_LOG(LogTemp,Display,TEXT("LANDING_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    UE_LOG(LogTemp,Display,TEXT("LANDING_PROOF step=%d success=%d damage=%d settling=%d pos=%s speed=%.2f"),MenuProofStep,OK,ImpactDamage,bGroundPhysics,*Flight.WorldPose().GetLocation().ToString(),LandingBody->GetPhysicsLinearVelocity().Size()*.01);
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
