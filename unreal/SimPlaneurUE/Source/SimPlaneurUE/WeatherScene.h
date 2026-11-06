#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherScene.generated.h"

class ULocalFogVolumeComponent;
class UProceduralMeshComponent;
class UDirectionalLightComponent;
class UMaterialInstanceDynamic;

// Bounded visual pools. The weather field stays world anchored; only the small
// rendering volume of individual raindrops follows the observer.
UCLASS()
class SIMPLANEURUE_API AWeatherScene : public AActor
{
    GENERATED_BODY()
public:
    AWeatherScene();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    int32 GetLightningSerial() const { return LightningSerial; }
    FVector GetLightningPosition() const { return LightningPosition; }
    double GetLightningAge() const;
    bool IsLightningVisible() const;
    float GetLocalRain() const { return LocalRain; }
    int32 GetFogPoolSize() const { return LowFog.Num()+RainFog.Num(); }
private:
    UPROPERTY() TArray<TObjectPtr<ULocalFogVolumeComponent>> LowFog;
    UPROPERTY() TArray<TObjectPtr<ULocalFogVolumeComponent>> RainFog;
    UPROPERTY() UProceduralMeshComponent* Drops;
    UPROPERTY() UProceduralMeshComponent* Bolt;
    UPROPERTY() UDirectionalLightComponent* Flash;
    UPROPERTY(Transient) UMaterialInstanceDynamic* RainMaterial;
    struct FDrop { FVector P; double Floor=0; float Speed=0,Length=0,Width=.8f; };
    TArray<FDrop> Particles;
    TArray<FVector> Vertices,Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    FRandomStream Random{385477};
    FRandomStream StormRandom{885931};
    float LowUpdate=0,RainUpdate=0,LocalRain=0;
    double NextLightning=0,LightningTime=-100;
    int32 LastSky=-1,LightningSerial=0;
    FVector LightningPosition=FVector::ZeroVector;
    void UpdateFog(double Seconds);
    void UpdateRain(float DeltaSeconds,const FVector& Eye,const FVector& Right,double Seconds);
    void Strike(double Seconds);
};
