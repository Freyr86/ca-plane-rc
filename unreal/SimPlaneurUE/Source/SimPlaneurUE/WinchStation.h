#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WinchStation.generated.h"

UCLASS()
class AWinchStation : public AActor
{
    GENERATED_BODY()
public:
    AWinchStation();
    void TurnDrum(double Metres);
    virtual void OnConstruction(const FTransform& Transform) override;
private:
    UPROPERTY() TObjectPtr<class USceneComponent> Drum;
    UPROPERTY() TMap<FName,FLinearColor> Colours;
};
