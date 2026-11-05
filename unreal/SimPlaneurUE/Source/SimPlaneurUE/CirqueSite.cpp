#include "CirqueSite.h"
#include "CirqueWall.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace CirqueSite
{
    namespace
    {
        // Keep this reference independent of Pilot: the wall opening is fixed
        // while the picnic site moves from the original position to its edge.
        const CirqueWall::FPassage Approach=[]
        {
            const FVector2D Original(303700.,226000.);
            const auto& Gaps=CirqueWall::Passages();
            const CirqueWall::FPassage* Best=&Gaps[0];
            for(const auto& Gap:Gaps)
                if(FVector2D::DistSquared(Gap.Centre,Original)<FVector2D::DistSquared(Best->Centre,Original)) Best=&Gap;
            return *Best;
        }();
    }
    const FVector2D Forward=-Approach.Outward;
    const FVector2D Right(-Forward.Y,Forward.X);
    const float Heading=FMath::RadiansToDegrees(FMath::Atan2(Forward.Y,Forward.X));
    const FVector2D Pilot=Approach.Centre+Approach.Outward*400.;
    // The picnic group sits beside the approach, leaving the opening clear.
    // Both benches face the table, with their long edges parallel to it.
    const FVector2D Table=Point(0,-400);
    const FVector2D BenchLeft=Point(0,-510);
    const FVector2D BenchRight=Point(0,-290);
    const FVector2D Sock=Point(-100,500);
}

FVector CirqueSite::Ground(float X,float Y)
{
    const float S=ASlopeWorld::TerrainGridSpacing(X,Y);
    const float X0=FMath::FloorToFloat(X/S)*S,Y0=FMath::FloorToFloat(Y/S)*S;
    const float U=(X-X0)/S,V=(Y-Y0)/S;
    const float B=ASlopeWorld::TerrainVertexHeightCm(X0,Y0+S),C=ASlopeWorld::TerrainVertexHeightCm(X0+S,Y0);
    const bool Lower=U+V<=1.f;
    const float A=ASlopeWorld::TerrainVertexHeightCm(X0+(Lower?0:S),Y0+(Lower?0:S));
    const float Dx=Lower?C-A:A-B,Dy=Lower?B-A:A-C;
    return {X,Y,Lower?A+Dx*U+Dy*V:A+Dx*(U-1)+Dy*(V-1)};
}

bool CirqueSite::FurnitureFootprint(float X,float Y)
{
    auto Inside=[&](FVector2D Centre,double Depth,double Width)
    {
        const FVector2D P=FVector2D(X,Y)-Centre;
        return FMath::Abs(FVector2D::DotProduct(P,Right))<Depth && FMath::Abs(FVector2D::DotProduct(P,Forward))<Width;
    };
    return Inside(BenchLeft,42,105) || Inside(BenchRight,42,105) || Inside(Table,65,125)
        || FVector2D::DistSquared({X,Y},Sock)<FMath::Square(22.);
}

void ASlopeWorld::BuildCirqueFurniture()
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if(!ensure(Cube && Base)) return;
    auto Paint=[&](FLinearColor Color)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);
        M->SetVectorParameterValue(TEXT("Color"),Color);
        return M;
    };
    UMaterialInterface* Timber[]={Paint({.32f,.155f,.063f}),Paint({.27f,.119f,.043f}),Paint({.36f,.18f,.081f})};
    auto* Frame=Paint({.075f,.042f,.022f});
    auto Part=[&](FVector Centre,FVector Size,UMaterialInterface* Material,FName Tag,FRotator Bearing)
    {
        auto* C=NewObject<UStaticMeshComponent>(this);
        C->SetupAttachment(GetRootComponent());
        C->SetMobility(EComponentMobility::Static);
        C->SetStaticMesh(Cube);
        C->SetRelativeLocationAndRotation(Centre,Bearing);
        C->SetRelativeScale3D(Size/100.);
        C->SetMaterial(0,Material);
        SceneContact::Solid(C,TEXT("bench"));
        C->ComponentTags.Add(Tag);
        C->RegisterComponent();
    };
    auto Bench=[&](FVector2D Centre,double Facing)
    {
        const FVector Forward(CirqueSite::Right*Facing,0),Right(-CirqueSite::Forward*Facing,0);
        const FRotator Bearing(0,CirqueSite::Heading+Facing*90.,0);
        auto At=[&](double Depth,double Width,double Z) { return FVector(Centre,Z)+Forward*Depth+Right*Width; };
        double Ground=-BIG_NUMBER;
        for(int Rear:{-1,1}) for(int Side:{-1,1})
        {
            const FVector P=At(Rear*18,Side*72,0);
            Ground=FMath::Max(Ground,CirqueSite::Ground(P.X,P.Y).Z);
        }
        const double Seat=Ground+48;
        for(int Side:{-1,1})
        {
            for(int Rear:{-1,1})
            {
                FVector P=At(Rear*18,Side*72,0);
                const double Foot=CirqueSite::Ground(P.X,P.Y).Z-6,Top=Rear<0?Seat+51:Seat-4;
                P.Z=(Foot+Top)*.5;
                Part(P,{7,8,Top-Foot},Frame,TEXT("CirqueBench"),Bearing);
            }
            Part(At(0,Side*72,Seat-5),{48,8,7},Frame,TEXT("CirqueBench"),Bearing);
        }
        for(int I=0;I<4;++I) Part(At(-18+I*12,0,Seat),{11,185,4},Timber[I%3],TEXT("CirqueBench"),Bearing);
        for(int I=0;I<3;++I) Part(At(-12,0,Seat+22+I*12),{4,185,10},Timber[(I+1)%3],TEXT("CirqueBench"),Bearing);
    };
    Bench(CirqueSite::BenchLeft,1.); Bench(CirqueSite::BenchRight,-1.);

    const FVector Forward(CirqueSite::Right,0),Right(-CirqueSite::Forward,0);
    const FRotator Bearing(0,CirqueSite::Heading+90.,0);
    auto At=[&](double Depth,double Width,double Z) { return FVector(CirqueSite::Table,Z)+Forward*Depth+Right*Width; };
    double Ground=-BIG_NUMBER;
    for(int Depth:{-1,1}) for(int Side:{-1,1})
    {
        const FVector P=At(Depth*35,Side*90,0);
        Ground=FMath::Max(Ground,CirqueSite::Ground(P.X,P.Y).Z);
    }
    const double Top=Ground+79;
    for(int Depth:{-1,1}) for(int Side:{-1,1})
    {
        FVector P=At(Depth*35,Side*90,0);
        const double Foot=CirqueSite::Ground(P.X,P.Y).Z-7;
        P.Z=(Foot+Top-5)*.5;
        Part(P,{10,10,Top-5-Foot},Frame,TEXT("CirqueTable"),Bearing);
    }
    for(int Side:{-1,1}) Part(At(0,Side*90,Top-9),{96,9,12},Frame,TEXT("CirqueTable"),Bearing);
    Part(At(0,0,Ground+27),{10,190,10},Frame,TEXT("CirqueTable"),Bearing);
    for(int I=0;I<7;++I) Part(At(-42+I*14,0,Top),{13,220,5},Timber[I%3],TEXT("CirqueTable"),Bearing);
    UE_LOG(LogTemp,Display,TEXT("CIRQUE_SITE furniture benches=2 tables=1 location_m=%.2f,%.2f wall_setback_cm=400"),
        CirqueSite::Pilot.X*.01,CirqueSite::Pilot.Y*.01);
}
