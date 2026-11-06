#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInstanceDynamic;

// GPU vertex deformation only. Tree instances and their simple colliders stay fixed.
struct FVegetationWind
{
    void Prepare(AActor* Owner,UMaterialInstanceDynamic* LivingGrass);
    void Update(double WeatherSeconds);
private:
    TWeakObjectPtr<AActor> WorldOwner;
    TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Materials;
    double LastUpdate=-1.;
};
