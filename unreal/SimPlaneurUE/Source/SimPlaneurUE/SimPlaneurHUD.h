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
    static constexpr float InstrumentScale=.7f;
    UPROPERTY(Transient)
    TObjectPtr<class UFont> CompassFont;
    float EquipmentOpacity = .9f;
    double LastEquipmentActivity = -1.;
    double LastEquipmentUpdate = -1.;
    void UpdateEquipmentOpacity(const class ASlopeFlightPawn& Plane);
    void DrawWindCompass(const class ASlopeFlightPawn& Plane);
    void DrawEquipmentHUD(const class ASlopeFlightPawn& Plane);
};
