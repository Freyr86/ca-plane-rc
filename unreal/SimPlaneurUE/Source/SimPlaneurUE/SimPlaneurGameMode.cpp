#include "SimPlaneurGameMode.h"
#include "SlopeFlightPawn.h"
#include "SimPlaneurHUD.h"
#include "SlopeWorld.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Scalability.h"
#include "HAL/IConsoleManager.h"

ASimPlaneurGameMode::ASimPlaneurGameMode()
{
    DefaultPawnClass = ASlopeFlightPawn::StaticClass();
    HUDClass = ASimPlaneurHUD::StaticClass();
}

void ASimPlaneurGameMode::StartPlay()
{
    if (FParse::Param(FCommandLine::Get(),TEXT("CaPlaneLowSpec")))
    {
        // Session-only defaults, before the scene is created. Do not overwrite
        // the player's saved graphics/radio/model settings during recovery.
        Scalability::FQualityLevels Low; Low.SetFromSingleQualityLevel(0);
        Scalability::SetQualityLevels(Low);
        if (auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud"))) C->Set(0,ECVF_SetByCode);
        UE_LOG(LogTemp,Display,TEXT("COMPATIBILITY_MODE low_graphics=1 settings_preserved=1"));
    }
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
