#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VegetationWind.h"
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
    static constexpr float ChaletX=-132400.f;
    static constexpr float ChaletY=-57900.f;
    static constexpr float PilotX = 6400.f;
    static constexpr float PilotY = -1800.f;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    static constexpr int32 MinGrassDistance = 20;
    static constexpr int32 MaxGrassDistance = 500;
    static int32 SavedGrassDistanceMeters();
    void SetGrassDistanceMeters(int32 Meters,bool Visible=true);
    int32 GrassDistanceMeters() const { return GrassDistance; }

    UFUNCTION(BlueprintPure)
    static float TerrainHeightCm(float X, float Y);
    static float TerrainGridSpacing(float X,float Y);
    // Mesh vertex height, including the straight edges joining terrain grids.
    // Keep TerrainHeightCm as the continuous heightfield for the flight model.
    static float TerrainVertexHeightCm(float X,float Y);

private:
    FVegetationWind Vegetation;
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
    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> PilotGrass;
    int32 GrassDistance = 50;
    int32 FieldWidth = 0;
    TArray<FIntPoint> FieldTiles;
    FIntPoint LastGrassCentre = FIntPoint(MAX_int32,MAX_int32);
    void UpdateFieldGrass(const FVector& Eye);
    FIntPoint LastDetailCentre = FIntPoint(MAX_int32,MAX_int32);
    TMap<FIntPoint,TArray<FVector>> Crowns;
    float CrownCover(float X,float Y) const;
    void PlantFieldTile(FIntPoint Tile,int32 Slot,bool Detail);

    int32 LastSolarMinutes=-1, LastSky=-1;
    double LastCloudBase=-1,LastCloudCoverage=-1,LastWeatherWind=-1,LastWeatherBearing=-1;
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* WeatherClouds=nullptr;
    void BuildLaunchSite();
    void BuildCirqueFurniture();
    void BuildChalet(double X,double Y,bool Terrace);
    void UpdateWindsock(float DeltaSeconds, double WeatherTime);
    struct FSiteWindsock
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        FVector Pivot;
        FVector Flow=FVector::ZeroVector;
        bool Initialized=false;
    };
    TArray<FSiteWindsock> SiteWindsocks;
    void UpdateSky();
    void BuildTerrain();
    void BuildLake();
    void BuildTerrainSection(int32 Section, int32 Cells, float Spacing, FVector2D Origin, FBox2D Hole, float BorderStep);
    void AddScenery();
    void AddCirqueScenery();
};
