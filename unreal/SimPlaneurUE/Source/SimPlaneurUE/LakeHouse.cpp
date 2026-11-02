#include "SlopeWorld.h"
#include "SceneContact.h"
#include "ValleyLandscape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

void ASlopeWorld::BuildChalet(double X,double Y,bool Terrace)
{
    const double Scale=Terrace ? .82 : 1.;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    auto* Boards=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/Lakeside/M_CabinBoards"));
    auto* Hull=Terrace ? nullptr : LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Meadow/Lakeside/LakeSkiff/StaticMeshes/LakeSkiff"));
    if (!ensure(Cube && Base && Boards && (Terrace || Hull))) return;
    auto Paint=[&](FLinearColor Tint)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);
        M->SetVectorParameterValue(TEXT("Color"),Tint);
        return M;
    };
    auto Batch=[&](UMaterialInterface* Mat,FName Kind)
    {
        auto* C=NewObject<UInstancedStaticMeshComponent>(this);
        C->SetupAttachment(GetRootComponent());
        C->SetStaticMesh(Cube);
        C->SetMaterial(0,Mat);
        SceneContact::Solid(C,Kind);
        C->RegisterComponent();
        return C;
    };
    auto* Walls=Batch(Boards,TEXT("house"));
    auto* Stone=Batch(Paint({.21f,.195f,.165f}),TEXT("house"));
    auto* Slate=Batch(Paint({.105f,.135f,.145f}),TEXT("house"));
    auto* Trim=Batch(Paint({.43f,.34f,.21f}),TEXT("house"));
    auto* Glass=Batch(Paint({.052f,.16f,.20f}),TEXT("house"));
    auto* Pier=Batch(Boards,Terrace ? TEXT("terrace") : TEXT("dock"));
    const double Floor=Terrace ? FMath::Max(TerrainHeightCm(X-300,Y+300),TerrainHeightCm(X-300,Y-300))+30. : TerrainHeightCm(X-350,Y+345)+26.;
    auto Box=[&](UInstancedStaticMeshComponent* C,double Eastward,double Across,double Z,FVector Size,FRotator Rot=FRotator::ZeroRotator)
    {
        C->AddInstance(FTransform(Rot,FVector(X+Eastward*Scale,Y+Across*Scale,Floor+(Z-Floor)*Scale),Size*Scale/100.));
    };

    const double Foot=Terrace ? Floor-280. : TerrainHeightCm(X+370,Y-350)-30.;
    Box(Stone,0,0,(Floor+Foot)/2,{720,690,Floor-Foot});
    Box(Walls,0,0,Floor+152,{710,680,304});
    // Close the two gables with narrowing courses of horizontal siding.
    for (int I=0;I<17;++I)
        Box(Walls,0,0,Floor+309+10*I,{710,680.-40.*I,10});
    for (int Side : {-1,1})
    {
        Box(Slate,0,Side*186,Floor+380,{810,425,16},FRotator(0,0,Side*26.565));
        Box(Trim,0,Side*371,Floor+290,{810,12,18});
        for (int End : {-1,1}) Box(Trim,End*355,Side*340,Floor+150,{13,13,304});
    }
    Box(Stone,-185,130,Floor+439,{54,64,170});
    Box(Slate,-185,130,Floor+524,{68,78,12});
    // Front windows, crossbars and a recessed-colour door facing the water.
    Box(Trim,357,0,Floor+102,{10,104,204});
    Box(Walls,364,0,Floor+98,{7,85,190});
    Box(Trim,371,31,Floor+98,{6,6,15});
    for (int Side : {-1,1})
    {
        Box(Trim,357,Side*203,Floor+162,{11,144,132});
        Box(Glass,364,Side*203,Floor+162,{5,125,113});
        Box(Trim,369,Side*203,Floor+162,{5,5,113});
        Box(Trim,369,Side*203,Floor+162,{5,125,5});
        Box(Trim,364,Side*203,Floor+95,{24,156,8});
        Box(Trim,-30,Side*342,Floor+165,{182,10,126});
        Box(Glass,-30,Side*349,Floor+165,{164,5,108});
        Box(Trim,-30,Side*354,Floor+165,{6,5,108});
    }
    if (Terrace)
    {
        // The open front overlooks the slope. Side rails leave a clear throwing lane.
        Box(Pier,520,0,Floor-9,{334,684,16});
        for (int I=0;I<21;++I) Box(Pier,361+16*I,0,Floor,{15.6,684,5});
        for (int Side : {-1,1})
        {
            for (int Offset : {370,525,680})
            {
                const double Bottom=Floor+(TerrainHeightCm(X+Offset*Scale,Y+Side*325*Scale)-25.-Floor)/Scale;
                Box(Walls,Offset,Side*325,(Floor+108+Bottom)/2,{12,12,Floor+108-Bottom});
            }
            Box(Walls,525,Side*325,Floor+108,{326,13,10});
            Box(Walls,525,Side*325,Floor+51,{326,7,8});
        }
        for (int I=0;I<24;++I)
        {
            const double Along=702+32*I, Top=Floor-17*(I+1);
            const double Ground=Floor+(TerrainHeightCm(X+Along*Scale,Y)-Floor)/Scale;
            if (Top<Ground-8) break;
            Box(Pier,Along,0,(Top+Ground-22)/2,{32,155,Top-Ground+22});
        }
        return;
    }
    for (int I=0; I<5; ++I)
    {
        const double Top=Floor-I*17, Bottom=TerrainHeightCm(X+385+32*I,Y)-15;
        if (Top>Bottom) Box(Stone,385+32*I,0,(Top+Bottom)/2,{32,142,Top-Bottom});
    }

    // Find the crossing on the house centreline. The first planks overlap the bank.
    double Shore=0;
    while (Shore<4000 && TerrainHeightCm(X+Shore,Y)>Valley::WaterLevel) Shore+=10;
    const double Start=Shore-330, Deck=Valley::WaterLevel+53;
    Box(Pier,Start+517,0,Deck-7,{1055,183,6});
    for (int I=0;I<48;++I) Box(Pier,Start+I*22.,0,Deck,{21.4,188,9});
    for (int I=0;I<5;++I)
        for (int Side : {-1,1})
        {
            const double Along=Start+I*250, Across=Side*82;
            const double Bottom=TerrainHeightCm(X+Along,Y+Across)-35.;
            const double Top=Deck+25.;
            if (Top>Bottom) Box(Pier,Along,Across,(Bottom+Top)/2,{15,15,Top-Bottom});
        }
    auto* Boat=NewObject<UStaticMeshComponent>(this);
    Boat->SetupAttachment(GetRootComponent());
    Boat->SetStaticMesh(Hull);
    Boat->SetRelativeLocationAndRotation(FVector(X+Start+815,Y+178,Valley::WaterLevel-21),FRotator(0,-90,0));
    SceneContact::Solid(Boat,TEXT("boat"));
    Boat->RegisterComponent();
    UE_LOG(LogTemp,Display,TEXT("LAKESIDE house=(%.0f,%.0f) shore=%.1fm dock_length=10.6m boat=%s"),X*.01,Y*.01,Shore*.01,*Boat->GetStaticMesh()->GetName());
}
