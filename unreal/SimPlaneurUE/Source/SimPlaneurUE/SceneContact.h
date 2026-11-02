#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace SceneContact
{
    constexpr ECollisionChannel Channel = ECC_GameTraceChannel1;
    void Solid(UPrimitiveComponent* Component,FName Kind);
    bool CheckSite(UWorld* World);
    bool SweepTopy(UWorld* World,const AActor* Plane,const FTransform& From,const FTransform& To,
                   FTransform& Rest,FName& Kind,FVector FuselageCentre=FVector::ZeroVector, FVector2D SizeScale=FVector2D(1,1),
                   double WingRisePerSpan=7.5/59.5, FVector4 Wheel=FVector4(0,0,0,0),double TipSweepCm=0.);
}
