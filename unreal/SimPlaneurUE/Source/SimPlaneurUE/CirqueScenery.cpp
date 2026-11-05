#include "SlopeWorld.h"
#include "CirqueLandscape.h"
#include "CirqueSite.h"
#include "CirqueWall.h"
#include "ValleyLandscape.h"
#include "SceneContact.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    FVector Surface(float X,float Y,FVector& Normal)
    {
        // Put stones and roots on the rendered triangles, including steep talus.
        const float S=ASlopeWorld::TerrainGridSpacing(X,Y);
        const float X0=FMath::FloorToFloat(X/S)*S,Y0=FMath::FloorToFloat(Y/S)*S;
        const float U=(X-X0)/S,V=(Y-Y0)/S;
        const float B=ASlopeWorld::TerrainVertexHeightCm(X0,Y0+S),C=ASlopeWorld::TerrainVertexHeightCm(X0+S,Y0);
        const bool Lower=U+V<=1.f;
        const float A=ASlopeWorld::TerrainVertexHeightCm(X0+(Lower?0:S),Y0+(Lower?0:S));
        const float Dx=Lower?C-A:A-B,Dy=Lower?B-A:A-C;
        Normal=FVector(-Dx,-Dy,S).GetSafeNormal();
        return FVector(X,Y,Lower?A+Dx*U+Dy*V:A+Dx*(U-1)+Dy*(V-1));
    }
}

void ASlopeWorld::AddCirqueScenery()
{
    FRandomStream Random(26801600);
    auto Group=[this](const TCHAR* Name,const TCHAR* Path,int32 Near,int32 Far,bool Solid)
    {
        auto* Mesh=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,FName(Name));
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetMobility(EComponentMobility::Static);
        Mesh->ComponentTags.Add(TEXT("GeneratedScenery"));
        Mesh->ComponentTags.Add(TEXT("CirqueScenery"));
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path));
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCullDistances(Near,Far);
        if(Solid) SceneContact::Solid(Mesh,TEXT("rock"));
        else Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->RegisterComponent();
        return Mesh;
    };
    auto* Blocks=Group(TEXT("CirqueBlocks"),TEXT("/PCGBiomeSample/Meshes/PCG_Boulder_01"),160000,220000,true);
    auto* Gravel=Group(TEXT("CirqueLooseStone"),TEXT("/PCGBiomeSample/Meshes/PCG_Boulder_01"),11000,18000,false);
    Gravel->SetCastShadow(false);
    if(auto* Limestone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_CirqueLimestone")))
        for(auto* Mesh:{Blocks,Gravel})
            if(Mesh->GetStaticMesh())
                for(int32 Slot=0;Slot<Mesh->GetStaticMesh()->GetStaticMaterials().Num();++Slot) Mesh->SetMaterial(Slot,Limestone);
    TArray<FTransform> RockTransforms,GravelTransforms;
    if(Blocks->GetStaticMesh())
    {
        const FBox Bounds=Blocks->GetStaticMesh()->GetBoundingBox();
        for(int32 I=0;I<16000;++I)
        {
            const float A=Random.FRandRange(-PI,PI),R=Random.FRandRange(30000,75000);
            const float X=Cirque::CenterX+R*FMath::Cos(A),Y=Cirque::CenterY+R*FMath::Sin(A);
            const auto Land=Cirque::Sample(X,Y);
            if(Land.Scree<.10f || Random.FRand()>Land.Scree*.85f) continue;
            FVector Normal; const FVector Ground=Surface(X,Y,Normal);
            if(Normal.Z<.55f) continue;
            const bool Small=I>=5000;
            const float Cluster=.55f+.45f*FMath::PerlinNoise2D(FVector2D(X,Y)/7500.f);
            if(Small && Random.FRand()>Cluster) continue;
            const float Size=Small ? Random.FRandRange(24,95) : Random.FRandRange(115,470)*(Random.FRand()<.06f?2.1f:1.f);
            const float Scale=Size/FMath::Max(1.,Bounds.GetSize().Z);
            const FVector S(Scale*Random.FRandRange(.8f,1.6f),Scale*Random.FRandRange(.65f,1.45f),Scale);
            const FQuat Rotation=FRotator(Random.FRandRange(-10,10),Random.FRandRange(0,360),Random.FRandRange(-8,8)).Quaternion();
            // The submerged lower fraction anchors the rock to the talus instead
            // of balancing identical meshes on their bounding-box corner.
            const FVector Centre=Ground+FVector(0,0,Size*.24f);
            const FTransform T(Rotation,Centre-Rotation.RotateVector(Bounds.GetCenter()*S),S);
            (Small?GravelTransforms:RockTransforms).Add(T);
        }
        Blocks->AddInstances(RockTransforms,false,false,false);
        Gravel->AddInstances(GravelTransforms,false,false,false);
    }

    auto* Firs=Group(TEXT("CirqueFirs"),TEXT("/Game/Meadow/SilverFir/StaticMeshes/SilverFir"),380000,460000,false);
    auto* Saplings=Group(TEXT("CirqueSaplings"),TEXT("/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_a"),65000,90000,false);
    UHierarchicalInstancedStaticMeshComponent* Trees[]={Firs,Saplings};
    TArray<FTransform> Timber[2],Cylinders;
    for(int32 I=0;I<7800;++I)
    {
        const float A=Random.FRandRange(-PI,PI),R=100000.f*FMath::Sqrt(Random.FRand());
        const float X=Cirque::CenterX+R*FMath::Cos(A),Y=Cirque::CenterY+R*FMath::Sin(A);
        const auto Land=Cirque::Sample(X,Y);
        if(Land.Influence<.15f || Land.Cliff>.06f || Land.Scree>.10f) continue;
        const float Cover=Valley::Woodland(X,Y);
        // Open rim meadows, irregular forest islands below, and a sparse fringe.
        const float OpenRim=Land.Meadow*FMath::SmoothStep(59000.f,72000.f,R);
        const float Woodland=FMath::SmoothStep(.40f,.68f,Cover)*(1.f-.88f*OpenRim);
        if(Random.FRand()>Woodland*.72f) continue;
        FVector Normal; const FVector Ground=Surface(X,Y,Normal);
        if(Normal.Z<.86f || Ground.Z<0 || Ground.Z>38000) continue;
        const int32 Kind=Random.FRand()<.22f?1:0;
        if(!Trees[Kind]->GetStaticMesh()) continue;
        const FBox Bounds=Trees[Kind]->GetStaticMesh()->GetBoundingBox();
        const float Height=Kind==0?Random.FRandRange(850,1750):Random.FRandRange(180,470);
        const float Scale=Height/FMath::Max(1.,Bounds.GetSize().Z),Width=Scale*Random.FRandRange(.86f,1.25f);
        const FVector S(Width,Width,Scale);
        const FTransform T(FRotator(0,Random.FRandRange(0,360),0),Ground-FVector(0,0,Bounds.Min.Z*Scale+8),S);
        // Preserve the existing random stream while making room for the site
        // and the walking corridor along the dry-stone wall.
        if(CirqueSite::Clearing(X,Y) || CirqueWall::Footprint(X,Y,400.f)
            || CirqueWall::Passages().ContainsByPredicate([&](const auto& Passage)
                {return FVector2D::DistSquared({X,Y},Passage.Centre)<FMath::Square(600.);})) continue;
        Timber[Kind].Add(T);
        const FVector Centre=T.TransformPosition(Bounds.GetCenter());
        const float Radius=.30f*FMath::Max(Bounds.GetSize().X*Width,Bounds.GetSize().Y*Width);
        Cylinders.Emplace(FQuat::Identity,Centre,FVector(Radius/50,Radius/50,Height/100));
        const float CrownRadius=.25f*(Bounds.GetSize().X+Bounds.GetSize().Y)*Width;
        for(int32 U=FMath::FloorToInt((X-CrownRadius)/1000);U<=FMath::FloorToInt((X+CrownRadius)/1000);++U)
            for(int32 V=FMath::FloorToInt((Y-CrownRadius)/1000);V<=FMath::FloorToInt((Y+CrownRadius)/1000);++V)
                Crowns.FindOrAdd(FIntPoint(U,V)).Emplace(X,Y,CrownRadius);
    }
    for(int32 I=0;I<2;++I) Trees[I]->AddInstances(Timber[I],false,false,false);
    auto* Colliders=NewObject<UInstancedStaticMeshComponent>(this,TEXT("CirqueTreeColliders"));
    Colliders->SetupAttachment(GetRootComponent());
    Colliders->SetMobility(EComponentMobility::Static);
    Colliders->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder")));
    Colliders->SetVisibility(false); Colliders->SetHiddenInGame(true); Colliders->SetCastShadow(false);
    SceneContact::Solid(Colliders,TEXT("tree"));
    Colliders->AddInstances(Cylinders,false,false,false);
    Colliders->RegisterComponent();
    UE_LOG(LogTemp,Display,TEXT("CIRQUE_SCENERY blocks=%d loose_stones=%d firs=%d saplings=%d"),RockTransforms.Num(),GravelTransforms.Num(),Timber[0].Num(),Timber[1].Num());
}
