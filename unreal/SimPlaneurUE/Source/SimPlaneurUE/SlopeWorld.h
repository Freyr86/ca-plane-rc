#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlopeWorld.generated.h"

class UProceduralMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;

UCLASS()
class SIMPLANEURUE_API ASlopeWorld : public AActor
{
    GENERATED_BODY()

public:
    ASlopeWorld();
    static constexpr float PilotX = 6400.f;
    static constexpr float PilotY = -1800.f;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure)
    static float TerrainHeightCm(float X, float Y);
    static float TerrainGridSpacing(float X,float Y);

private:
    UPROPERTY()
    UProceduralMeshComponent* Terrain;
    UPROPERTY()
    UProceduralMeshComponent* Lake;

    UPROPERTY()
    UDirectionalLightComponent* Sun;

    UPROPERTY()
    USkyLightComponent* SkyLight;

    UPROPERTY()
    USkyAtmosphereComponent* SkyAtmosphere;

    UPROPERTY()
    UExponentialHeightFogComponent* HeightFog;

    UPROPERTY()
    UVolumetricCloudComponent* Clouds;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* GrassMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> FieldPatches;
    TArray<FIntPoint> FieldTiles;
    FIntPoint LastGrassCentre = FIntPoint(MAX_int32,MAX_int32);
    void UpdateFieldGrass(const FVector& Eye);
    void PlantFieldTile(FIntPoint Tile,int32 Slot);

    void BuildTerrain();
    void BuildLake();
    void BuildTerrainSection(int32 Section, int32 Cells, float Spacing, FVector2D Origin, FBox2D Hole, float BorderStep);
    void AddScenery();
};
