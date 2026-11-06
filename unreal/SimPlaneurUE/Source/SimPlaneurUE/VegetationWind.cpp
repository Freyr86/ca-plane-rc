#include "VegetationWind.h"
#include "ValleyAir.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

void FVegetationWind::Prepare(AActor* Owner,UMaterialInstanceDynamic* LivingGrass)
{
    Materials.Reset(); LastUpdate=-1.; WorldOwner=Owner;
    if(LivingGrass) Materials.Add(LivingGrass);
    int32 Trees=0;
    TInlineComponentArray<UStaticMeshComponent*> Meshes(Owner);
    for(auto* Component:Meshes)
    {
        UStaticMesh* Mesh=Component->GetStaticMesh();
        if(!Mesh || !Component->IsVisible()) continue;
        const FString Name=Mesh->GetName();
        const bool Tree=Name==TEXT("HillTree_02") || Name==TEXT("SilverFir") || Name.StartsWith(TEXT("pine_sapling_small_"));
        for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)
        {
            UMaterialInterface* Material=Component->GetMaterial(Slot);
            if(!Material) continue;
            if(Tree)
            {
                const FString Path=FString::Printf(TEXT("/Game/Meadow/Wind/MI_Wind_%s_%d"),*Name,Slot);
                Material=LoadObject<UMaterialInterface>(nullptr,*Path);
                if(!Material) continue; // Keep original appearance when optional generated assets are absent.
            }
            else if(Material->GetName()!=TEXT("M_WildGrass") && Material->GetName()!=TEXT("M_Flowers")) continue;
            auto* Dynamic=UMaterialInstanceDynamic::Create(Material,Owner);
            if(Tree)
            {
                const FBox Bounds=Mesh->GetBoundingBox();
                Dynamic->SetScalarParameterValue(TEXT("VegetationMinZ"),Bounds.Min.Z);
                Dynamic->SetScalarParameterValue(TEXT("VegetationHeight"),FMath::Max(1.,Bounds.GetSize().Z));
                Dynamic->SetScalarParameterValue(TEXT("VegetationCompliance"),Name==TEXT("HillTree_02")?3.2f:Name==TEXT("SilverFir")?2.3f:1.4f);
                ++Trees;
            }
            Component->SetMaterial(Slot,Dynamic);
            Component->SetEvaluateWorldPositionOffset(true);
            if(Tree) Component->SetWorldPositionOffsetDisableDistance(30000);
            Component->SetBoundsScale(1.08f);
            Materials.Add(Dynamic);
        }
    }
    Update(0.);
    UE_LOG(LogTemp,Display,TEXT("VEGETATION_WIND tree_materials=%d total_materials=%d gpu_deformation=1"),Trees,Materials.Num());
}

void FVegetationWind::Update(double WeatherSeconds)
{
    if(LastUpdate>=0. && WeatherSeconds>=LastUpdate && WeatherSeconds-LastUpdate<.08) return;
    LastUpdate=WeatherSeconds;
    const auto Weather=ValleyAir::Settings();
    const double Bearing=FMath::DegreesToRadians(Weather.WindFromDegrees);
    const FLinearColor Flow(-FMath::Cos(Bearing)*Weather.WindSpeedMps,-FMath::Sin(Bearing)*Weather.WindSpeedMps,Weather.Turbulence,1.f);
    const auto* Owner=WorldOwner.Get();
    const double WorldSeconds=Owner && Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : WeatherSeconds;
    for(auto& Ref:Materials)
        if(auto* Material=Ref.Get())
        {
            Material->SetVectorParameterValue(TEXT("WeatherWind"),Flow);
            Material->SetScalarParameterValue(TEXT("WeatherTimeOffset"),WeatherSeconds-WorldSeconds);
        }
}
