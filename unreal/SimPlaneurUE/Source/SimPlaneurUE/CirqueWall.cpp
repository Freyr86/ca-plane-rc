#include "CirqueWall.h"
#include "CirqueLandscape.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    constexpr float FirstAngle=-2.50f,LastAngle=2.50f;

    CirqueWall::FStation Station(float Angle)
    {
        CirqueWall::FStation S;
        S.Angle=Angle;
        S.Edge=Cirque::RimEdge(Angle);
        const FVector2D Tangent=(Cirque::RimEdge(Angle+.0001f)-Cirque::RimEdge(Angle-.0001f)).GetSafeNormal();
        S.Outward={Tangent.Y,-Tangent.X};
        if(FVector2D::DotProduct(S.Outward,S.Edge-FVector2D(Cirque::CenterX,Cirque::CenterY))<0) S.Outward*=-1.;
        S.Centre=S.Edge+S.Outward*CirqueWall::Setback;
        return S;
    }

    CirqueWall::FStation AtDistance(float Distance)
    {
        const auto& P=CirqueWall::Path();
        int32 Low=0,High=P.Num()-1;
        while(High-Low>1)
        {
            const int32 Mid=(Low+High)/2;
            if(P[Mid].Distance<Distance) Low=Mid; else High=Mid;
        }
        const float T=FMath::Clamp((Distance-P[Low].Distance)/(P[High].Distance-P[Low].Distance),0.f,1.f);
        auto S=Station(FMath::Lerp(P[Low].Angle,P[High].Angle,T));
        S.Distance=Distance;
        return S;
    }

    bool Open(float Distance,float Margin=0.f)
    {
        for(const auto& Gap:CirqueWall::Passages())
            if(FMath::Abs(Distance-Gap.Distance)<=Gap.Width*.5f+Margin) return true;
        return false;
    }

    float Ground(const FVector2D& P)
    {
        const float S=ASlopeWorld::TerrainGridSpacing(P.X,P.Y);
        const float X0=FMath::FloorToFloat(P.X/S)*S,Y0=FMath::FloorToFloat(P.Y/S)*S;
        const float U=(P.X-X0)/S,V=(P.Y-Y0)/S;
        const float B=ASlopeWorld::TerrainVertexHeightCm(X0,Y0+S),C=ASlopeWorld::TerrainVertexHeightCm(X0+S,Y0);
        const bool Lower=U+V<=1.f;
        const float A=ASlopeWorld::TerrainVertexHeightCm(X0+(Lower?0:S),Y0+(Lower?0:S));
        return Lower?A+(C-A)*U+(B-A)*V:A+(A-B)*(U-1)+(A-C)*(V-1);
    }
}

const TArray<CirqueWall::FStation>& CirqueWall::Path()
{
    static const TArray<FStation> Points=[]
    {
        TArray<FStation> Result;
        constexpr int32 Steps=3600;
        Result.Reserve(Steps+1);
        for(int32 I=0;I<=Steps;++I)
        {
            auto S=Station(FMath::Lerp(FirstAngle,LastAngle,float(I)/Steps));
            if(!Result.IsEmpty()) S.Distance=Result.Last().Distance+(S.Centre-Result.Last().Centre).Size();
            Result.Add(S);
        }
        return Result;
    }();
    return Points;
}

const TArray<CirqueWall::FPassage>& CirqueWall::Passages()
{
    static const TArray<FPassage> Gaps=[]
    {
        TArray<FPassage> Result;
        const FVector2D Site(303700.,226000.);
        const FStation* Closest=nullptr;
        double Best=MAX_dbl;
        for(const auto& S:Path())
        {
            const double D=(S.Centre-Site).SizeSquared();
            if(D<Best) { Best=D; Closest=&S; }
        }
        // Place the main opening exactly in the approach from the picnic site.
        if(Closest) Result.Add({Closest->Centre,Closest->Outward,Closest->Distance,300.f});
        for(float D=24000.f;D<Path().Last().Distance-15000.f;D+=25000.f)
        {
            bool NearMain=false;
            for(const auto& Gap:Result) NearMain|=FMath::Abs(D-Gap.Distance)<11000.f;
            if(NearMain) continue;
            const auto S=AtDistance(D);
            Result.Add({S.Centre,S.Outward,D,300.f});
        }
        Result.Sort([](const FPassage& A,const FPassage& B){return A.Distance<B.Distance;});
        return Result;
    }();
    return Gaps;
}

bool CirqueWall::Footprint(float X,float Y,float ExtraMarginCm)
{
    const FVector2D P(X,Y),R=P-FVector2D(Cirque::CenterX,Cirque::CenterY);
    if(R.SizeSquared()<FMath::Square(44000.f) || R.SizeSquared()>FMath::Square(88000.f)) return false;
    const float A=FMath::Atan2(-R.X*Cirque::BackAxisY+R.Y*Cirque::BackAxisX,
        R.X*Cirque::BackAxisX+R.Y*Cirque::BackAxisY);
    if(A<FirstAngle-.03f || A>LastAngle+.03f) return false;
    const auto& Pts=Path();
    int32 Low=0,High=Pts.Num()-1;
    while(High-Low>1)
    {
        const int32 Mid=(Low+High)/2;
        if(Pts[Mid].Angle<A) Low=Mid; else High=Mid;
    }
    // Offset-normal curves differ slightly from their source polar angle.
    // A bounded local neighbourhood covers that displacement even in ravines.
    const double Limit=FMath::Square(Width*.5+ExtraMarginCm);
    for(int32 I=FMath::Max(0,Low-10);I<FMath::Min(Pts.Num()-1,High+10);++I)
    {
        const FVector2D AB=Pts[I+1].Centre-Pts[I].Centre;
        const double T=FMath::Clamp(FVector2D::DotProduct(P-Pts[I].Centre,AB)/AB.SizeSquared(),0.,1.);
        if((P-Pts[I].Centre-AB*T).SizeSquared()>Limit) continue;
        const float D=FMath::Lerp(Pts[I].Distance,Pts[I+1].Distance,float(T));
        if(!Open(D)) return true;
    }
    return false;
}

void CirqueWall::Build(ASlopeWorld* Site)
{
    if(!Site || !Site->GetComponentsByTag(UActorComponent::StaticClass(),TEXT("CirqueWallSolid")).IsEmpty()) return;
    auto* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_CirqueLimestone"));
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/PCGBiomeSample/Meshes/PCG_Boulder_01"));
    if(!ensure(Stone && Mesh)) return;
    auto* Core=NewObject<UProceduralMeshComponent>(Site,TEXT("CirqueDryStoneWall"));
    Core->SetupAttachment(Site->GetRootComponent());
    Core->SetMobility(EComponentMobility::Static);
    Core->ComponentTags.Add(TEXT("CirqueWallSolid"));
    Core->ComponentTags.Add(TEXT("CirqueWall"));
    Core->ComponentTags.Add(TEXT("GeneratedScenery"));
    Core->bUseAsyncCooking=false;
    SceneContact::Solid(Core,TEXT("rock"));
    Core->RegisterComponent();

    auto* Details=NewObject<UHierarchicalInstancedStaticMeshComponent>(Site,TEXT("CirqueWallStones"));
    Details->SetupAttachment(Site->GetRootComponent());
    Details->SetMobility(EComponentMobility::Static);
    Details->ComponentTags.Add(TEXT("CirqueWallDetail"));
    Details->ComponentTags.Add(TEXT("GeneratedScenery"));
    Details->SetStaticMesh(Mesh);
    for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I) Details->SetMaterial(I,Stone);
    Details->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Details->SetCanEverAffectNavigation(false);
    Details->SetCullDistances(7500,11000);
    Details->RegisterComponent();

    TArray<FVector> Vertices,Normals;
    TArray<FVector2D> UV;
    TArray<int32> Indices;
    TArray<FTransform> Stones;
    FRandomStream Random(30372260);
    const auto& Pts=Path();
    TArray<float> Z;
    Z.Reserve(Pts.Num());
    for(const auto& S:Pts) Z.Add(Ground(S.Centre));
    const FBox Bounds=Mesh->GetBoundingBox();
    const FVector MeshSize=Bounds.GetSize();
    auto Quad=[&](FVector A,FVector B,FVector C,FVector D)
    {
        const int32 First=Vertices.Num();
        const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        Vertices.Append({A,B,C,D});
        for(int32 I=0;I<4;++I) Normals.Add(N);
        UV.Append({{0,0},{1,0},{1,1},{0,1}});
        Indices.Append({First,First+2,First+1,First,First+3,First+2});
    };
    auto Rock=[&](FVector Centre,FVector Size,FRotator Rotation)
    {
        const FVector Scale=Size/MeshSize;
        const FQuat Q=Rotation.Quaternion();
        Stones.Emplace(Q,Centre-Q.RotateVector(Bounds.GetCenter()*Scale),Scale);
    };
    int32 Segments=0;
    for(int32 I=0;I<Pts.Num()-1;++I)
    {
        const auto& A=Pts[I]; const auto& B=Pts[I+1];
        const float Length=B.Distance-A.Distance,Middle=(A.Distance+B.Distance)*.5f;
        if(Open(Middle,Length*.5f+18.f)) continue;
        const FVector2D Across=(A.Outward+B.Outward).GetSafeNormal();
        const FVector2D Along=(B.Centre-A.Centre).GetSafeNormal();
        const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(Along.Y,Along.X));
        const float HeightA=Height+4.f*FMath::PerlinNoise1D(A.Distance/280.f);
        const float HeightB=Height+4.f*FMath::PerlinNoise1D(B.Distance/280.f);
        const float HalfBase=Width*.5f-7.f,HalfTop=Width*.5f-12.f;
        // The continuous core carries collision and the far silhouette. Stones
        // only add close surface detail: no separate physics body per stone.
        FVector AL(A.Centre-A.Outward*HalfBase,Z[I]-12),AR(A.Centre+A.Outward*HalfBase,Z[I]-12);
        FVector BL(B.Centre-B.Outward*HalfBase,Z[I+1]-12),BR(B.Centre+B.Outward*HalfBase,Z[I+1]-12);
        FVector ALT(A.Centre-A.Outward*HalfTop,Z[I]+HeightA),ART(A.Centre+A.Outward*HalfTop,Z[I]+HeightA);
        FVector BLT(B.Centre-B.Outward*HalfTop,Z[I+1]+HeightB),BRT(B.Centre+B.Outward*HalfTop,Z[I+1]+HeightB);
        Quad(AL,BL,BLT,ALT); Quad(BR,AR,ART,BRT); Quad(ALT,BLT,BRT,ART);
        // Closed ends stop both walking and glider sweeps at each doorway jamb.
        if(I==0 || Open(A.Distance,Length+18.f)) Quad(AR,AL,ALT,ART);
        if(I==Pts.Num()-2 || Open(B.Distance,Length+18.f)) Quad(BL,BR,BRT,BLT);
        ++Segments;
        const int32 Count=FMath::Max(1,FMath::RoundToInt(Length/53.f));
        for(int32 J=0;J<Count;++J)
        {
            const float T=(J+.5f)/Count;
            const FVector2D P=FMath::Lerp(A.Centre,B.Centre,double(T));
            const float GroundZ=FMath::Lerp(Z[I],Z[I+1],T),PieceLength=Length/Count;
            for(int32 Row=0;Row<3;++Row)
                for(float Side:{-1.f,1.f})
                {
                    const FVector2D Centre=P+Along*Random.FRandRange(-5,5)+Across*(Side*(22.f-Row*1.8f));
                    Rock(FVector(Centre,GroundZ+12.f+Row*21.f+Random.FRandRange(-2,2)),
                        {PieceLength*Random.FRandRange(.99f,1.12f),Random.FRandRange(25,32),Random.FRandRange(22,27)},
                        FRotator(Random.FRandRange(-7,7),Yaw+Random.FRandRange(-8,8),Random.FRandRange(-6,6)));
                }
            Rock(FVector(P,GroundZ+72.f+Random.FRandRange(-2,2)),
                {PieceLength*Random.FRandRange(1.02f,1.13f),Random.FRandRange(49,59),Random.FRandRange(17,22)},
                FRotator(Random.FRandRange(-5,5),Yaw+Random.FRandRange(-9,9),Random.FRandRange(-5,5)));
        }
    }
    Core->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,{},{},true);
    Core->SetMaterial(0,Stone);
    Details->AddInstances(Stones,false,false,false);
    UE_LOG(LogTemp,Display,TEXT("CIRQUE_WALL length_m=%.1f segments=%d passages=%d stones=%d collision_triangles=%d"),
        Pts.Last().Distance*.01,Segments,Passages().Num(),Stones.Num(),Indices.Num()/3);
}
