#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "Airfield.h"
#include "FlightMenu.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Components/SceneComponent.h"

void ASlopeFlightPawn::TestF3F(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    if(!bAppearanceReady || Elapsed<5.f+MenuProofStep*2.5f) return;
    bool OK=true;
    auto Place=[&](FVector P)
    {
        FHitResult Ground;
        if(!FindPilotGround(P,Ground)) { OK=false; return; }
        PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
    };
    auto Shot=[](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false,false); };
    const FVector Axis(-Airfield::Diagonal,Airfield::Diagonal,0);
    switch(MenuProofStep)
    {
    case 0:
        Fly(); bShowWindTrails=false; SelectedModel=PreviewModel=2; Flight=HangarModels[2]; BuildFallbackTopy();
        F3FCircuit=FF3FCourse(); bF3FEnabled=false; Bungee=FBungeeLaunch();
        Place(FVector(Airfield::Point(-5000,0),0)); LaunchHeading=Axis.Rotation().Yaw; WalkPitch=-20.; ResetFlight();
        SelectF3F(); UpdateF3F(0); InteractF3F();
        OK &= F3FPlacement==2;
        break;
    case 1:
        Place(F3FDraft.A+Axis*10000.); UpdateF3F(0); InteractF3F();
        OK &= F3FCircuit.Valid() && F3FPlacement==0;
        F3FCircuit.Load(GGameUserSettingsIni); OK &= F3FCircuit.Valid();
        break;
    case 2:
        Place((F3FCircuit.A+F3FCircuit.B)*.5-F3FCircuit.FlyingSide*800.);
        LaunchHeading=F3FCircuit.FlyingSide.Rotation().Yaw; WalkPitch=-12.;
        InteractF3F(); UpdateF3F(0); AdvanceFlight(0);
        OK &= F3FRun.State==FF3FRun::EState::Ready && !bInBackpack;
        Shot(TEXT("f3f-01-ready.png")); break;
    case 3:
        BeginHandLaunch(); for(int I=0;I<8;++I) AdvanceFlight(.05f);
        OK &= F3FRun.State==FF3FRun::EState::Entry && !bHeldForLaunch;
        break;
    case 4:
    {
        F3FRun.Arm(); F3FRun.Launch(PilotPosition().Z);
        const FVector Mid=(F3FCircuit.A+F3FCircuit.B)*.5;
        const FVector Offset=F3FCircuit.FlyingSide*5000.+FVector(0,0,6000.);
        const FTransform Before(Axis.Rotation(),F3FCircuit.A-Axis*500.+Offset), After(Axis.Rotation(),F3FCircuit.A+Axis*500.+Offset);
        F3FPhysics(Before,After,.5);
        OK &= F3FRun.State==FF3FRun::EState::Racing;
        Flight.LaunchFrom(FTransform(Axis.Rotation(),Mid+Offset),Axis*20.);
        PreviousPose=Flight.WorldPose(); TargetPosition=PreviousPose.GetLocation();
        SetActorTransform(PreviousPose);
        for(int I=0;I<4;++I)
        {
            const FVector Base=I%2?F3FCircuit.A:F3FCircuit.B;
            const FVector Direction=I%2?-Axis:Axis;
            F3FRun.Step(F3FCircuit,{Base-Direction*100.+Offset},{Base+Direction*100.+Offset},4.);
        }
        OK &= F3FRun.Legs==4;
        Shot(TEXT("f3f-02-timing.png")); break;
    }
    case 5:
    {
        for(int I=4;I<10;++I)
        {
            const FVector Base=I%2?F3FCircuit.A:F3FCircuit.B;
            const FVector Direction=I%2?-Axis:Axis;
            const FVector Offset=F3FCircuit.FlyingSide*5000.+FVector(0,0,6000.);
            F3FRun.Step(F3FCircuit,{Base-Direction*100.+Offset},{Base+Direction*100.+Offset},4.);
        }
        OK &= F3FRun.Legs==10 && F3FRun.State==FF3FRun::EState::Finished;
        Shot(TEXT("f3f-03-finish.png")); break;
    }
    case 6:
        OpenMenu(); if(Menu.IsValid()) Menu->OpenMapFromFlight(); else OK=false;
        break;
    case 7: Shot(TEXT("f3f-04-map.png")); break;
    default:
        UE_LOG(LogTemp,Display,TEXT("F3F_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
    UE_LOG(LogTemp,Display,TEXT("F3F_PROOF step=%d ok=%d legs=%d time=%.2f reason=%s"),MenuProofStep,OK,F3FRun.Legs,F3FRun.Time(),*F3FRun.Reason);
    if(!OK) { FPlatformMisc::RequestExitWithStatus(false,3); return; }
    ++MenuProofStep;
#endif
}
