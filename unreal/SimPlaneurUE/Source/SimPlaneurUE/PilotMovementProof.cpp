#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraTypes.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ASlopeFlightPawn::TestPilotMovement(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if(!bAppearanceReady || Elapsed<5.+MenuProofStep*2.) return;
    bool OK=true;
    const FVector Direction=FRotator(0,135,0).Vector();
    auto Move=[&](bool Sprint){for(int I=0;I<20;++I) MovePilot(Direction,.05f,Sprint);};
    switch(MenuProofStep)
    {
    case 0:
    {
        FHitResult Ground;
        OK=FindPilotGround(FVector(Airfield::Point(-1500,0),0),Ground);
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
        SelectedModel=PreviewModel=0; Flight=HangarModels[0]; BuildFallbackTopy(); ResetFlight();
        bHeldForLaunch=true; LaunchHeading=135;
        const FVector Start=PilotPosition();
        Move(false);
        const double Walk=FVector::Dist2D(Start,PilotPosition());
        OK &= FMath::IsNearlyEqual(Walk,550.,1.);
        bHeldForLaunch=false;
        Flight.LaunchFrom(FTransform(FRotator(0,135,0),PilotPosition()+Direction*5000+FVector(0,0,4000)),Direction*10.);
        SetActorTransform(Flight.WorldPose());
        const FTransform PlanePose=Flight.WorldPose();
        FMinimalViewInfo Before,After; CalcCamera(0,Before);
        const FVector From=PilotPosition(); Move(false);
        CalcCamera(0,After);
        OK &= CanWalkInFlight() && !IsWalking();
        OK &= FMath::IsNearlyEqual(FVector::Dist2D(From,PilotPosition()),Walk*.8,1.);
        OK &= Flight.WorldPose().Equals(PlanePose,.001) && !Flight.IsStopped();
        OK &= FVector::Dist2D(Before.Location,After.Location)>430.;
        MenuProofLocation=PilotPosition(); Move(true);
        OK &= FMath::IsNearlyEqual(FVector::Dist2D(MenuProofLocation,PilotPosition()),440.,1.);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/instruments-70-flight.png"),true,false,false);
        break;
    }
    case 1:
    {
        // Place a solid obstacle on the actual terrain: flight-time walking
        // must stop in front of it instead of bypassing the standard sweep.
        const FVector Start=PilotPosition();
        auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>();
        auto* Mesh=Wall->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        Wall->SetActorLocation(Start+Direction*160.+FVector(0,0,95));
        Wall->SetActorRotation(FRotator(0,135,0));
        Wall->SetActorScale3D({.5,4.,2.});
        SceneContact::Solid(Mesh,TEXT("rock"));
        Move(false);
        OK=FVector::Dist2D(Start,PilotPosition())<130. && CanWalkInFlight();
        Wall->Destroy();
        const FVector BeforeMenu=PilotPosition();
        bMenuOpen=true; Move(false);
        OK &= PilotPosition().Equals(BeforeMenu,.001);
        bMenuOpen=false;
        bNoclip=true; Move(false);
        OK &= PilotPosition().Equals(BeforeMenu,.001);
        bNoclip=false;
        break;
    }
    case 2:
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/instruments-70-after-walk.png"),true,false,false);
        break;
    case 3:
    {
        // Walk down the real chalet staircase, including its last step.
        ResetFlight(); bHeldForLaunch=true; bInBackpack=true;
        PlaneVisual->SetHiddenInGame(true,true);
        FHitResult Ground;
        const double X=ASlopeWorld::ChaletX,Y=ASlopeWorld::ChaletY;
        OK=FindPilotGround(FVector(X+550*.82,Y,0),Ground);
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
        MenuProofLocation=PilotPosition();
        LaunchHeading=0; WalkPitch=-35;
        double LastZ=PilotSite.Z;
        for(double Offset=550*.82;Offset<=1550*.82;Offset+=10.)
            if(FindPilotGround(FVector(X+Offset,Y,0),Ground))
            {
                if(FMath::Abs(Ground.ImpactPoint.Z-LastZ)>30.)
                    UE_LOG(LogTemp,Display,TEXT("STAIR_PROFILE offset=%.1f drop=%.1f"),Offset,LastZ-Ground.ImpactPoint.Z);
                LastZ=Ground.ImpactPoint.Z;
            }
        for(int I=0;I<75;++I) MovePilot(FVector::ForwardVector,.02f,false);
        OK &= PilotPosition().X>MenuProofLocation.X+780.;
        UE_LOG(LogTemp,Display,TEXT("STAIR_DOWN travel=%.1f offset=%.1f"),PilotPosition().X-MenuProofLocation.X,PilotPosition().X-X);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/chalet-stairs-down.png"),true,false,false);
        break;
    }
    case 4:
    {
        for(int I=0;I<75;++I) MovePilot(-FVector::ForwardVector,.02f,false);
        OK=FVector::Dist2D(PilotPosition(),MenuProofLocation)<35.;
        LaunchHeading=180; WalkPitch=-25;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/chalet-stairs-up.png"),true,false,false);
        break;
    }
    case 5:
    {
        FHitResult Ground;
        OK=FindPilotGround(FVector(Airfield::Point(0,0),0),Ground);
        const FVector Start=Ground.ImpactPoint;
        auto Box=[&](FVector Offset,FVector Size,FName Kind)
        {
            auto* Actor=GetWorld()->SpawnActor<AStaticMeshActor>();
            auto* Mesh=Actor->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
            Actor->SetActorLocation(Start+Offset); Actor->SetActorScale3D(Size/100.);
            SceneContact::Solid(Mesh,Kind);
            return Actor;
        };
        auto Across=[&]
        {
            PilotSite=Start; PilotOffset=FVector2D::ZeroVector;
            for(int I=0;I<20;++I) MovePilot(FVector::ForwardVector,.05f,false);
            return PilotPosition().X-Start.X;
        };
        auto* Step=Box({200,0,24},{200,200,48},TEXT("rock"));
        const double Small=Across();
        OK &= Small>540.;
        auto* Ceiling=Box({200,0,185},{400,300,10},TEXT("house"));
        const double UnderCeiling=Across();
        OK &= UnderCeiling<100.;
        Ceiling->Destroy();
        Step->SetActorLocation(Start+FVector(200,0,60));
        Step->SetActorScale3D({2,2,1.2});
        const double Tall=Across();
        OK &= Tall<100.;
        Step->Destroy();
        UE_LOG(LogTemp,Display,TEXT("STEP_OBSTACLES small=%.1f low_ceiling=%.1f tall=%.1f"),Small,UnderCeiling,Tall);
        break;
    }
    case 6:
    {
        // Both directions at sprint speed use the same bounded collision steps.
        PilotSite=MenuProofLocation; PilotOffset=FVector2D::ZeroVector;
        for(int I=0;I<10;++I) MovePilot(FVector::ForwardVector,.05f,true);
        const double Down=PilotPosition().X-MenuProofLocation.X;
        OK=Down>800.;
        for(int I=0;I<10;++I) MovePilot(-FVector::ForwardVector,.05f,true);
        OK &= FVector::Dist2D(PilotPosition(),MenuProofLocation)<35.;
        UE_LOG(LogTemp,Display,TEXT("STAIR_SPRINT down=%.1f return_error=%.1f"),Down,FVector::Dist2D(PilotPosition(),MenuProofLocation));
        break;
    }
    default:
        UE_LOG(LogTemp,Display,TEXT("PILOT_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    UE_LOG(LogTemp,Display,TEXT("PILOT_PROOF step=%d success=%d pilot=%s plane=%s"),MenuProofStep,OK,*PilotPosition().ToString(),*Flight.WorldPose().GetLocation().ToString());
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
