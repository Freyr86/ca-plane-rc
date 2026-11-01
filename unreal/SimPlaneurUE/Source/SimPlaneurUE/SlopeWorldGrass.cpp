#include "SlopeWorld.h"
#include "ValleyLandscape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    constexpr int32 Width = 5;
    constexpr int32 Species = 3;
    constexpr float TileSize = 1400.f;
    const TCHAR* const MeshPaths[Species] = {
        TEXT("/Game/Meadow/BentGrass/StaticMeshes/BentGrass"),
        TEXT("/Game/Meadow/FloweringFescue/StaticMeshes/FloweringFescue"),
        TEXT("/Game/Meadow/CreepingBent/StaticMeshes/CreepingBent")
    };

    // Match the actual terrain triangles on the wider valley grid. Analytic heights
    // can put roots above or below the interpolated surface on a steep mountainside.
    FVector RootAt(float X,float Y,FVector& Normal)
    {
        const float Step=ASlopeWorld::TerrainGridSpacing(X,Y);
        const float X0=FMath::FloorToFloat(X/Step)*Step, Y0=FMath::FloorToFloat(Y/Step)*Step;
        const float U=(X-X0)/Step, V=(Y-Y0)/Step;
        const float B=ASlopeWorld::TerrainHeightCm(X0,Y0+Step);
        const float C=ASlopeWorld::TerrainHeightCm(X0+Step,Y0);
        const bool Lower = U+V<=1.f;
        const float Corner = Lower ? ASlopeWorld::TerrainHeightCm(X0,Y0) : ASlopeWorld::TerrainHeightCm(X0+Step,Y0+Step);
        const float Dx=Lower ? C-Corner : Corner-B, Dy=Lower ? B-Corner : Corner-C;
        Normal=FVector(-Dx,-Dy,Step).GetSafeNormal();
        const float Z=Lower ? Corner+Dx*U+Dy*V : Corner+Dx*(U-1)+Dy*(V-1);
        return FVector(X,Y,Z-1);
    }
}

void ASlopeWorld::UpdateFieldGrass(const FVector& Eye)
{
    const FIntPoint Centre(FMath::FloorToInt(Eye.X/TileSize),FMath::FloorToInt(Eye.Y/TileSize));
    if (Centre==LastGrassCentre || FMath::Max(FMath::Abs(Eye.X),FMath::Abs(Eye.Y))>550000.f) return;
    if (Eye.Z-TerrainHeightCm(Eye.X,Eye.Y)>4500.f) return;
    if (FieldTiles.IsEmpty())
    {
        FieldTiles.Init(FIntPoint(MAX_int32,MAX_int32),Width*Width);
        FieldPatches.SetNum(Width*Width*Species);
    }
    int32 Built=0;
    // Reuse world-aligned cells. Crossing a boundary only replaces the trailing row,
    // and a camera cut amortizes planting across frames, nearest cells first.
    for (int32 Ring=0;Ring<=Width/2;++Ring)
        for (int32 I=-Ring;I<=Ring;++I)
            for (int32 J=-Ring;J<=Ring;++J)
            {
                if (FMath::Max(FMath::Abs(I),FMath::Abs(J))!=Ring) continue;
                const FIntPoint Tile=Centre+FIntPoint(I,J);
                const int32 Slot=((Tile.X%Width+Width)%Width)*Width+(Tile.Y%Width+Width)%Width;
                if (FieldTiles[Slot]==Tile) continue;
                if (Built==2) return;
                PlantFieldTile(Tile,Slot);
                FieldTiles[Slot]=Tile;
                ++Built;
            }
    LastGrassCentre=Centre;
}

void ASlopeWorld::PlantFieldTile(FIntPoint Tile,int32 Slot)
{
    FRandomStream Random(HashCombineFast(GetTypeHash(Tile),4522u));
    TArray<FTransform> Tufts[Species];
    const FVector2D Base(Tile.X*TileSize,Tile.Y*TileSize);
    const float CentreHeight=TerrainHeightCm(Base.X+TileSize*.5f,Base.Y+TileSize*.5f);
    if (CentreHeight>Valley::WaterLevel-400 && CentreHeight<39000)
    {
        constexpr int32 Count=30;
        for (int32 I=0;I<Count;++I)
            for (int32 J=0;J<Count;++J)
            {
                const float X=Base.X+(I+Random.FRand())*TileSize/Count;
                const float Y=Base.Y+(J+Random.FRand())*TileSize/Count;
                const float FromPilot=FVector2D(X-PilotX,Y-PilotY).Size();
                if (FromPilot<2700 || Random.FRand()>FMath::SmoothStep(2700.f,4100.f,FromPilot)) continue;
                FVector Normal;
                FVector Root=RootAt(X,Y,Normal);
                if (Root.Z<Valley::WaterLevel+60 || Normal.Z<.75f) continue;
                const float Wood=Valley::Woodland(X,Y);
                if (Wood>.61f && Random.FRand()<.70f) continue;
                const float A=Random.FRand()*2*PI;
                const FQuat Rot=FRotationMatrix::MakeFromZX(Normal,FVector(FMath::Cos(A),FMath::Sin(A),0)).ToQuat();
                const float Variety=Random.FRand();
                const int32 Kind=Variety<.18f ? 1 : (Variety<.48f ? 2 : 0);
                const float Size=Random.FRandRange(.78f,1.23f);
                Tufts[Kind].Emplace(Rot,Root,FVector(Size,Size,Random.FRandRange(.66f,1.04f)));
            }
    }
    for (int32 K=0;K<Species;++K)
    {
        auto& Patch=FieldPatches[Slot*Species+K];
        if (!Patch && !Tufts[K].IsEmpty())
        {
            Patch=NewObject<UInstancedStaticMeshComponent>(this);
            Patch->SetupAttachment(GetRootComponent());
            Patch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPaths[K]));
            Patch->SetMaterial(0,GrassMaterial);
            Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Patch->SetCanEverAffectNavigation(false);
            Patch->SetCastShadow(K!=2);
            Patch->SetCullDistances(2200,2850);
            Patch->RegisterComponent();
        }
        if (Patch)
        {
            Patch->ClearInstances();
            if (!Tufts[K].IsEmpty()) Patch->AddInstances(Tufts[K],false,false,false);
        }
    }
}
