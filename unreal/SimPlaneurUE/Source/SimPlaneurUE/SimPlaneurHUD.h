#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimPlaneurHUD.generated.h"

UCLASS()
class SIMPLANEURUE_API ASimPlaneurHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
private:
    void DrawWindCompass(const class ASlopeFlightPawn& Plane);
};
