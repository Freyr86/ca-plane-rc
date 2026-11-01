#include "SlopeWorld.h"
#include "ValleyLandscape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    constexpr int32 Species = 3;
    constexpr int32 DetailWidth=7;
    constexpr float DetailSize=800.f;
    float CellSize(int32 Meters) { return FMath::Max(1400.f,Meters*100.f/6.f); }
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

float ASlopeWorld::CrownCover(float X,float Y) const
{
    const auto* Trees=Crowns.Find(FIntPoint(FMath::FloorToInt(X/1000),FMath::FloorToInt(Y/1000)));
    float Cover=0;
    if (Trees)
        for (const FVector& Tree : *Trees)
        {
            const float Distance=FVector2D(X-Tree.X,Y-Tree.Y).Size();
            Cover=FMath::Max(Cover,1.f-FMath::SmoothStep(float(Tree.Z)*.55f,float(Tree.Z),Distance));
        }
    return Cover;
}

int32 ASlopeWorld::SavedGrassDistanceMeters()
{
    int32 Meters = 50;
    GConfig->GetInt(TEXT("SimPlaneur.Graphics"),TEXT("GrassDistanceMeters"),Meters,GGameUserSettingsIni);
    return FMath::Clamp(Meters,MinGrassDistance,MaxGrassDistance);
}

void ASlopeWorld::BeginPlay()
{
    Super::BeginPlay();
    BuildLaunchSite();
    bool Visible=true;
    GConfig->GetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowGrass"),Visible,GGameUserSettingsIni);
    SetGrassDistanceMeters(SavedGrassDistanceMeters(),Visible);
}

void ASlopeWorld::SetGrassDistanceMeters(int32 Meters,bool Visible)
{
    const float OldSize=CellSize(GrassDistance);
    GrassDistance = FMath::Clamp(Meters,MinGrassDistance,MaxGrassDistance);
    const int32 End = GrassDistance*100;
    // Complete cells beyond the camera cell cover the radius at cell boundaries.
    const int32 Width = Visible ? 2*FMath::CeilToInt(float(End)/CellSize(GrassDistance))+1 : 0;
    if (Width != FieldWidth || OldSize!=CellSize(GrassDistance))
    {
        for (auto& Patch : FieldPatches)
            if (Patch) Patch->DestroyComponent();
        FieldPatches.Empty();
        FieldTiles.Empty();
        LastGrassCentre = FIntPoint(MAX_int32,MAX_int32);
        LastDetailCentre=LastGrassCentre;
        FieldWidth = Width;
    }
    for (int32 I=0; I<FieldPatches.Num(); ++I)
    {
        const int32 Limit=I<Width*Width*Species ? End : FMath::Min(2400,End);
        if (FieldPatches[I]) FieldPatches[I]->SetCullDistances(Limit*76/100,Limit);
    }
    for (int32 Index=0; Index<PilotGrass.Num(); ++Index)
    {
        const int32 PatchEnd = Index==2 ? FMath::RoundToInt(End*.68f) : End;
        if (PilotGrass[Index]) PilotGrass[Index]->SetVisibility(Visible);
        if (PilotGrass[Index]) PilotGrass[Index]->SetCullDistances(FMath::RoundToInt(PatchEnd*.76f),PatchEnd);
    }
    UE_LOG(LogTemp,Display,TEXT("GRASS_DISTANCE meters=%d grid=%d"),GrassDistance,FieldWidth);
}

void ASlopeWorld::UpdateFieldGrass(const FVector& Eye)
{
    const int32 Width = FieldWidth;
    if (Width<=0) return;
    const float TileSize=CellSize(GrassDistance);
    const FIntPoint Far(FMath::FloorToInt(Eye.X/TileSize),FMath::FloorToInt(Eye.Y/TileSize));
    const FIntPoint Near(FMath::FloorToInt(Eye.X/DetailSize),FMath::FloorToInt(Eye.Y/DetailSize));
    if ((Far==LastGrassCentre && Near==LastDetailCentre) || FMath::Max(FMath::Abs(Eye.X),FMath::Abs(Eye.Y))>550000.f) return;
    if (Eye.Z-TerrainHeightCm(Eye.X,Eye.Y)>GrassDistance*100.f) return;
    if (FieldTiles.IsEmpty())
    {
        FieldTiles.Init(FIntPoint(MAX_int32,MAX_int32),Width*Width+DetailWidth*DetailWidth);
        FieldPatches.SetNum(FieldTiles.Num()*Species);
    }
    auto UpdateLayer=[&](FIntPoint Centre,int32 Side,int32 Offset,bool Detail,FIntPoint& Last)
    {
        if (Centre==Last) return;
        int32 Built=0;
        // Reuse world-aligned cells. Crossing a boundary only replaces the trailing row,
        // and a camera cut amortizes planting across frames, nearest cells first.
        for (int32 Ring=0;Ring<=Side/2;++Ring)
            for (int32 I=-Ring;I<=Ring;++I)
                for (int32 J=-Ring;J<=Ring;++J)
                {
                    if (FMath::Max(FMath::Abs(I),FMath::Abs(J))!=Ring) continue;
                    const FIntPoint Tile=Centre+FIntPoint(I,J);
                    const int32 Slot=Offset+((Tile.X%Side+Side)%Side)*Side+(Tile.Y%Side+Side)%Side;
                    if (FieldTiles[Slot]==Tile) continue;
                    if (Built==2) return;
                    PlantFieldTile(Tile,Slot,Detail);
                    FieldTiles[Slot]=Tile;
                    ++Built;
                }
        Last=Centre;
    };
    UpdateLayer(Near,DetailWidth,Width*Width,true,LastDetailCentre);
    UpdateLayer(Far,Width,0,false,LastGrassCentre);
}

void ASlopeWorld::PlantFieldTile(FIntPoint Tile,int32 Slot,bool Detail)
{
    FRandomStream Random(HashCombineFast(GetTypeHash(Tile),4522u));
    TArray<FTransform> Tufts[Species];
    const float TileSize=Detail ? DetailSize : CellSize(GrassDistance);
    const FVector2D Base(Tile.X*TileSize,Tile.Y*TileSize);
    const float CentreHeight=TerrainHeightCm(Base.X+TileSize*.5f,Base.Y+TileSize*.5f);
    if (CentreHeight>Valley::WaterLevel-400 && CentreHeight<39000)
    {
        const int32 Count=Detail ? 28 : 30;
        for (int32 I=0;I<Count;++I)
            for (int32 J=0;J<Count;++J)
            {
                const float X=Base.X+(I+Random.FRand())*TileSize/Count;
                const float Y=Base.Y+(J+Random.FRand())*TileSize/Count;
                const float FromPilot=FVector2D(X-PilotX,Y-PilotY).Size();
                if (Detail)
                {
                    if (X< -135000 || X>65000 || Y< -75000 || Y>125000) continue;
                    const float Existing=FromPilot<4100 ? FMath::Square(1350.f/FMath::Max(1350.f,FromPilot)) : 0.f;
                    if (Random.FRand()<Existing) continue;
                }
                else if (FromPilot<2700 || Random.FRand()>FMath::SmoothStep(2700.f,4100.f,FromPilot)) continue;
                FVector Normal;
                FVector Root=RootAt(X,Y,Normal);
                if (Root.Z<Valley::WaterLevel+60 || Normal.Z<.75f) continue;
                const bool InLake=X>=-135000 && X<=65000 && Y>=-75000 && Y<=125000;
                const float Shade=InLake ? CrownCover(X,Y) : (Valley::Woodland(X,Y)>.61f ? .82f : 0.f);
                if (Random.FRand()<Shade*.85f) continue;
                const float A=Random.FRand()*2*PI;
                const FQuat Rot=FRotationMatrix::MakeFromZX(Normal,FVector(FMath::Cos(A),FMath::Sin(A),0)).ToQuat();
                const float Variety=Random.FRand();
                const int32 Kind=Shade>.45f ? 2 : Variety<.18f ? 1 : (Variety<.48f ? 2 : 0);
                const float Size=Random.FRandRange(.78f,1.23f);
                Tufts[Kind].Emplace(Rot,Root,FVector(Size,Size,Random.FRandRange(.66f,1.04f)*(1.f-.48f*Shade)));
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
            const int32 End=Detail ? FMath::Min(2400,GrassDistance*100) : GrassDistance*100;
            Patch->SetCullDistances(End*76/100,End);
            Patch->SetEvaluateWorldPositionOffset(true);
            Patch->RegisterComponent();
        }
        if (Patch)
        {
            Patch->ClearInstances();
            if (!Tufts[K].IsEmpty()) Patch->AddInstances(Tufts[K],false,false,false);
        }
    }
}
