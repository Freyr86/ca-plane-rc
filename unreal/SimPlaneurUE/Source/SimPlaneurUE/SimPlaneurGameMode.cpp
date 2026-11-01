#include "SimPlaneurGameMode.h"
#include "SlopeFlightPawn.h"
#include "SimPlaneurHUD.h"
#include "SlopeWorld.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ASimPlaneurGameMode::ASimPlaneurGameMode()
{
    DefaultPawnClass = ASlopeFlightPawn::StaticClass();
    HUDClass = ASimPlaneurHUD::StaticClass();
}

void ASimPlaneurGameMode::StartPlay()
{
    Super::StartPlay();
    if (UWorld* World = GetWorld())
    {
        World->SpawnActor<ASlopeWorld>(ASlopeWorld::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
        if (Controller && !Controller->GetPawn())
        {
            ASlopeFlightPawn* Plane = World->SpawnActor<ASlopeFlightPawn>(ASlopeFlightPawn::StaticClass(), FVector(0.0f, 0.0f, 8000.0f), FRotator::ZeroRotator);
            Controller->Possess(Plane);
        }
    }
}
