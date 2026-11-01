#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimPlaneurGameMode.generated.h"

UCLASS()
class SIMPLANEURUE_API ASimPlaneurGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASimPlaneurGameMode();
    virtual void StartPlay() override;
};
