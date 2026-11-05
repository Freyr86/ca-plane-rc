#include "SlopeWorld.h"
#include "Airfield.h"
#include "SlopeFlightPawn.h"
#include "ValleyLandscape.h"
#include "CirqueLandscape.h"
#include "CirqueWall.h"
#include "SceneContact.h"
#include "GliderDynamics.h"
#include "ValleyAir.h"

#include "ProceduralMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 TerrainCells = 320;
    constexpr float TerrainSizeCm = 160000.0f;
    constexpr float CoarseStep = TerrainSizeCm / TerrainCells;
    constexpr float PatchSize = 9000.f;
    constexpr float FineStep = 50.f;
    constexpr float MountainStep = 2500.f;
    // Square patch aligned with the 25 m mountain grid. The fine edges snap to its triangles.
    const FVector2D AirfieldMin(20000.,-150000.);
    constexpr float AirfieldSize=25000.f, AirfieldStep=250.f;
    const FBox2D AirfieldBounds(AirfieldMin,AirfieldMin+FVector2D(AirfieldSize));
    // Local detail only: the outer 25 m terrain is replaced, not overlaid. Both
    // grids share their border vertices, avoiding cracks and duplicate collision.
    const FVector2D CirqueMin(155000.,47500.);
    constexpr float CirqueSize=225000.f, CirqueStep=500.f;
    constexpr int32 CirqueCells=450, CliffSubdivisions=5;
    constexpr float CliffStep=CirqueStep/CliffSubdivisions;
    static_assert(CirqueCells*CirqueStep==CirqueSize);
    const FBox2D CirqueBounds(CirqueMin,CirqueMin+FVector2D(CirqueSize));
    const FVector2D PatchMin(FMath::GridSnap(ASlopeWorld::PilotX-PatchSize/2,CoarseStep),
                             FMath::GridSnap(ASlopeWorld::PilotY-PatchSize/2,CoarseStep));

    UMaterialInterface* GroundMaterial()
    {
        return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Meadow/M_AlpineSward.M_AlpineSward"));
    }

    const TArray<uint8>& CirqueRefinement()
    {
        // A one-cell halo puts the 1 m / 5 m joins outside the steep wall. Cache
        // this immutable map so grass and collision proofs use the exact mesh.
        static const TArray<uint8> Refined=[]
        {
            TArray<uint8> Core, Result;
            Core.Init(0,CirqueCells*CirqueCells);
            Result.Init(0,CirqueCells*CirqueCells);
            for(int32 I=0;I<CirqueCells;++I)
                for(int32 J=0;J<CirqueCells;++J)
                {
                    const float X=CirqueMin.X+I*CirqueStep,Y=CirqueMin.Y+J*CirqueStep;
                    const FVector2D P(X+CirqueStep*.5f-Cirque::CenterX,Y+CirqueStep*.5f-Cirque::CenterY);
                    // The irregular lip stays within this generous radial band.
                    if(P.SizeSquared()<FMath::Square(Cirque::RimRadius-20000.f)
                        || P.SizeSquared()>FMath::Square(Cirque::RimRadius+20000.f)) continue;
                    bool Wall=Cirque::Sample(X+CirqueStep*.5f,Y+CirqueStep*.5f).Cliff>.005f;
                    for(int32 U=0;U<=1 && !Wall;++U)
                        for(int32 V=0;V<=1 && !Wall;++V)
                            Wall=Cirque::Sample(X+U*CirqueStep,Y+V*CirqueStep).Cliff>.005f;
                    Core[I*CirqueCells+J]=Wall;
                }
            for(int32 I=1;I<CirqueCells-1;++I)
                for(int32 J=1;J<CirqueCells-1;++J)
                    for(int32 U=-1;U<=1;++U)
                        for(int32 V=-1;V<=1;++V)
                            Result[I*CirqueCells+J]|=Core[(I+U)*CirqueCells+J+V];
            return Result;
        }();
        return Refined;
    }

    bool RefineCirqueCell(float X,float Y)
    {
        const int32 I=FMath::FloorToInt((X-CirqueMin.X)/CirqueStep);
        const int32 J=FMath::FloorToInt((Y-CirqueMin.Y)/CirqueStep);
        return I>=0 && I<CirqueCells && J>=0 && J<CirqueCells
            && CirqueRefinement()[I*CirqueCells+J]!=0;
    }
}

ASlopeWorld::ASlopeWorld()
{
    PrimaryActorTick.bCanEverTick = true;

    Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
    SetRootComponent(Terrain);
    Terrain->SetMobility(EComponentMobility::Static);
    SceneContact::Solid(Terrain,TEXT("ground"));
    // The complete heightfield must be available to the very first flight sweep.
    Terrain->bUseAsyncCooking = false;
    Lake=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Lake"));
    Lake->SetupAttachment(Terrain);
    Lake->SetMobility(EComponentMobility::Static);
    SceneContact::Solid(Lake,TEXT("water"));

    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetupAttachment(RootComponent);
    Sun->SetRelativeRotation(FRotator(-43.f,-128.f,0.f));
    Sun->SetIntensity(65000.f);
    Sun->SetLightColor(FLinearColor(1.0f,.99f,.975f));
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->bCastCloudShadows = false;
    Sun->SetAtmosphereSunLight(true);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(RootComponent);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetIntensity(1.f);
    SkyLight->bRealTimeCapture = false;

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(RootComponent);

    HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
    HeightFog->SetupAttachment(RootComponent);
    HeightFog->SetFogDensity(.013f);
    HeightFog->SetFogHeightFalloff(.10f);
    HeightFog->SetStartDistance(28000.f);
    HeightFog->SetVolumetricFog(false);
    HeightFog->SetVolumetricFogExtinctionScale(0.7f);

    Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
    Clouds->SetupAttachment(RootComponent);
    Clouds->SetLayerBottomAltitude(2.4f);
    Clouds->SetLayerHeight(1.0f);
    Clouds->SetViewSampleCountScale(.5f);
    Clouds->SetShadowViewSampleCountScale(.4f);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMat(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"));
    if (CloudMat.Succeeded()) Clouds->SetMaterial(CloudMat.Object);
}

namespace
{
    // West of the launch crest, the lip follows a broken line across D3 and D4.
    float CliffOffset(float X,float Y)
    {
        return ASlopeWorld::PilotY-Y-1730.f-175.f*FMath::Sin(X*.00047f)-72.f*FMath::Sin(X*.00114f);
    }
}

float ASlopeWorld::TerrainHeightCm(float X, float Y)
{
    const float Range = FVector2D(X-PilotX,Y-PilotY).Size();
    if (Range >= 26000.f)
    {
        const float Natural=Valley::Height(X,Y);
        if (!AirfieldBounds.IsInside(FVector2D(X,Y))) return Natural;
        const FVector2D P=Airfield::Local(X,Y);
        // Grade the runway and the small apron; blend the earthworks into the hillside.
        const double Outside=FMath::Max(FMath::Abs(P.X)-Airfield::HalfLength-350.,
            FMath::Max(-P.Y-Airfield::HalfWidth-350.,P.Y-Airfield::HalfWidth-350.
                -1300.*(1.-FMath::SmoothStep(650.,1600.,FMath::Abs(P.X)))));
        static const float Datum=Valley::Height(Airfield::Centre.X,Airfield::Centre.Y);
        return FMath::Lerp(Datum,Natural,float(FMath::SmoothStep(0.,3800.,Outside)));
    }
    const float Linear = FMath::Clamp((X + 8500.0f) / 27500.0f, 0.0f, 1.0f);
    const float Smooth = Linear * Linear * (3.0f - 2.0f * Linear);
    float Height = FMath::Lerp(2700.0f, -2400.0f, Smooth);
    Height += 520.0f * FMath::Exp(-FMath::Square((X - 4500.0f) / 7000.0f));
    Height += FMath::Sin(Y * 0.00015f) * 210.0f;
    Height += FMath::Sin((X + Y) * 0.000085f) * 270.0f;
    const float Detail = FMath::Exp(-FMath::Square((X - 8500.0f) / 12500.0f));
    Height += FMath::Sin(Y * 0.00021f + X * 0.00004f) * 630.0f * Detail;
    Height += FMath::Sin(Y * 0.00047f - X * 0.00009f) * 210.0f * Detail;
    const float Basin = FMath::Square((X-26000.f)/14000.f) + FMath::Square((Y-32000.f)/23500.f);
    Height -= 2200.f * FMath::Exp(-Basin);
    // Rounded shoulder at the pilot position, falling towards the lake.
    // Widen the eastern shoulder, feathering it around the low saddle at the valley origin.
    const float SaddleDistance=FVector2D(X,Y).Size();
    const float EastWidth=Y>PilotY ? FMath::Lerp(2500.f,7400.f,FMath::SmoothStep(3500.f,7100.f,SaddleDistance)) : 2500.f;
    Height += 7255.f * FMath::Exp(-FMath::Square((X-PilotX)/10600.f) - FMath::Square((Y-PilotY)/EastWidth));
    const float Face=CliffOffset(X,Y);
    const float Along=FMath::Exp(-FMath::Square((X-PilotX)/10500.f));
    Height += Along*(1750.f*FMath::SmoothStep(-1150.f,-300.f,Face)
                     -3500.f*FMath::SmoothStep(-300.f,320.f,Face)
                     +1750.f*FMath::SmoothStep(320.f,2400.f,Face));
    if (Range <= 8500.f) return Height;
    return FMath::Lerp(Height,Valley::Height(X,Y),FMath::SmoothStep(8500.f,26000.f,Range));
}

void ASlopeWorld::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildTerrain();
    if (GetComponentsByTag(UActorComponent::StaticClass(), TEXT("GeneratedScenery")).Num() == 0)
    {
        AddScenery();
        AddCirqueScenery();
        CirqueWall::Build(this);
        BuildChalet(-74000.,8500.,false);
        BuildChalet(ChaletX,ChaletY,true);
    }
    SkyLight->RecaptureSky();
}

void ASlopeWorld::UpdateSky()
{
    const auto Weather=ValleyAir::Settings();
    if (Weather.SolarMinutes==LastSolarMinutes && Weather.Sky==LastSky) return;
    LastSolarMinutes=Weather.SolarMinutes;
    LastSky=Weather.Sky;
    const FVector ToSun=ValleyAir::SunDirection(LastSolarMinutes);
    const float Day=FMath::Clamp(float(ToSun.Z),0.f,1.f);
    const bool Covered=LastSky==2, Mist=LastSky==3;
    Sun->SetWorldRotation((-ToSun).Rotation());
    Sun->SetIntensity(78000.f*FMath::Sqrt(Day)*(Covered?.24f:Mist?.48f:1.f));
    Sun->SetLightColor(FMath::Lerp(FLinearColor(1.f,.44f,.19f),FLinearColor(1.f,.99f,.975f),FMath::SmoothStep(0.f,.44f,Day)));
    Clouds->SetVisibility(LastSky!=0);
    if (!WeatherClouds && Clouds->GetMaterial())
    {
        WeatherClouds=UMaterialInstanceDynamic::Create(Clouds->GetMaterial(),this);
        Clouds->SetMaterial(WeatherClouds);
    }
    if (WeatherClouds)
    {
        if (Covered) WeatherClouds->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"),1.f);
        else WeatherClouds->ClearParameterValues();
    }
    Clouds->SetLayerBottomAltitude(Covered?1.1f:2.4f);
    Clouds->SetLayerHeight(Covered?2.8f:1.f);
    HeightFog->SetFogDensity(Mist?.065f:Covered?.023f:.013f);
    HeightFog->SetStartDistance(Mist?800.f:28000.f);
    HeightFog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.007f,.009f,.018f),FLinearColor(.50f,.64f,.78f),FMath::SmoothStep(-.08f,.22f,float(ToSun.Z))));
    SkyLight->SetIntensity(.05f+.95f*FMath::SmoothStep(-.08f,.25f,float(ToSun.Z)));
    SkyLight->RecaptureSky();
}

void ASlopeWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateSky();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    const APawn* Plane = PC ? PC->GetPawn() : nullptr;
    if (const auto* Pilot=Cast<ASlopeFlightPawn>(Plane)) UpdateWindsock(DeltaSeconds,Pilot->GetWeatherSeconds());
    if (GrassMaterial && Plane)
    {
        GrassMaterial->SetVectorParameterValue(TEXT("AircraftCentre"), FLinearColor(Plane->GetActorLocation()));
        GrassMaterial->SetVectorParameterValue(TEXT("WingDirection"), FLinearColor(Plane->GetActorRightVector()));
        if (PC->PlayerCameraManager)
            UpdateFieldGrass(PC->PlayerCameraManager->GetCameraLocation());
    }
}

void ASlopeWorld::BuildTerrain()
{
    const FVector2D ValleyMin(-TerrainSizeCm*.5f,-TerrainSizeCm*.5f);
    BuildTerrainSection(0, TerrainCells, CoarseStep, ValleyMin, FBox2D(PatchMin,PatchMin+FVector2D(PatchSize,PatchSize)),MountainStep);
    BuildTerrainSection(1, int32(PatchSize/FineStep), FineStep, PatchMin, FBox2D(ForceInit),CoarseStep);
    BuildTerrainSection(2, 448, MountainStep, FVector2D(-560000,-560000),FBox2D(ValleyMin,-ValleyMin),0.f);
    BuildTerrainSection(3,int32(AirfieldSize/AirfieldStep),AirfieldStep,AirfieldMin,FBox2D(ForceInit),MountainStep);
    BuildTerrainSection(4,int32(CirqueSize/CirqueStep),CirqueStep,CirqueMin,FBox2D(ForceInit),MountainStep);
    BuildLake();
}

float ASlopeWorld::TerrainGridSpacing(float X,float Y)
{
    if (CirqueBounds.IsInside(FVector2D(X,Y))) return RefineCirqueCell(X,Y)?CliffStep:CirqueStep;
    if (AirfieldBounds.IsInside(FVector2D(X,Y))) return AirfieldStep;
    if (X>=PatchMin.X && X<PatchMin.X+PatchSize && Y>=PatchMin.Y && Y<PatchMin.Y+PatchSize) return FineStep;
    return FMath::Abs(X)<TerrainSizeCm/2 && FMath::Abs(Y)<TerrainSizeCm/2 ? CoarseStep : MountainStep;
}

float ASlopeWorld::TerrainVertexHeightCm(float X,float Y)
{
    const FVector2D High=CirqueMin+FVector2D(CirqueSize);
    if(X<CirqueMin.X || X>High.X || Y<CirqueMin.Y || Y>High.Y) return TerrainHeightCm(X,Y);
    auto AlongY=[&](float Step)
    {
        const float Y0=FMath::FloorToFloat(Y/Step)*Step;
        return FMath::Lerp(TerrainHeightCm(X,Y0),TerrainHeightCm(X,Y0+Step),(Y-Y0)/Step);
    };
    auto AlongX=[&](float Step)
    {
        const float X0=FMath::FloorToFloat(X/Step)*Step;
        return FMath::Lerp(TerrainHeightCm(X0,Y),TerrainHeightCm(X0+Step,Y),(X-X0)/Step);
    };
    if(FMath::IsNearlyEqual(X,float(CirqueMin.X),.01f) || FMath::IsNearlyEqual(X,float(High.X),.01f)) return AlongY(MountainStep);
    if(FMath::IsNearlyEqual(Y,float(CirqueMin.Y),.01f) || FMath::IsNearlyEqual(Y,float(High.Y),.01f)) return AlongX(MountainStep);
    const float X0=FMath::FloorToFloat(X/CirqueStep)*CirqueStep,Y0=FMath::FloorToFloat(Y/CirqueStep)*CirqueStep;
    const bool OnX=FMath::IsNearlyEqual(X,X0,.01f),OnY=FMath::IsNearlyEqual(Y,Y0,.01f);
    // Both endpoints of every 5 m edge retain the analytic height. Only the
    // intervening 1 m vertices snap when the neighbouring cell is not refined.
    if(OnX && !OnY && RefineCirqueCell(X0-CirqueStep,Y0)!=RefineCirqueCell(X0,Y0)) return AlongY(CirqueStep);
    if(OnY && !OnX && RefineCirqueCell(X0,Y0-CirqueStep)!=RefineCirqueCell(X0,Y0)) return AlongX(CirqueStep);
    return TerrainHeightCm(X,Y);
}

void ASlopeWorld::BuildLake()
{
    constexpr int32 Across = 65, Along = 107;
    constexpr float Step = 1200.f;
    TArray<FVector> Points, Normals;
    TArray<int32> Indices;
    TArray<FVector2D> UV;
    for (int32 I=0; I<=Across; ++I)
    {
        for (int32 J=0; J<=Along; ++J)
        {
            const float X = Valley::LakeX + (I - Across*.5f)*Step;
            const float Y = Valley::LakeY + (J - Along*.5f)*Step;
            Points.Emplace(X, Y, Valley::WaterLevel);
            Normals.Add(FVector::UpVector);
            UV.Emplace(X/100.f,Y/100.f);
            if (I==Across || J==Along || Valley::ShoreDistance(X+Step*.5f,Y+Step*.5f)>4500.f) continue;
            const int32 A=I*(Along+1)+J, B=A+1, C=A+Along+1;
            Indices.Append({A,B,C,B,C+1,C});
        }
    }
    Lake->CreateMeshSection_LinearColor(0,Points,Indices,Normals,UV,{},{},true);
    Lake->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_ValleyWater")));
}

void ASlopeWorld::BuildTerrainSection(int32 Section, int32 Cells, float Spacing, FVector2D Origin, FBox2D Hole, float BorderStep)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    const int32 Points = Cells + 1;
    int32 RefinementCount=0;
    if(Section==4)
        for(uint8 Cell:CirqueRefinement()) RefinementCount+=Cell!=0;
    const int32 Capacity=Points*Points+RefinementCount*FMath::Square(CliffSubdivisions+1);
    Vertices.Reserve(Capacity);
    Normals.Reserve(Capacity);
    Colors.Reserve(Capacity);
    UVs.Reserve(Capacity);
    if(Section==4) Tangents.Reserve(Capacity);
    Triangles.Reserve(Cells*Cells*6+RefinementCount*(CliffSubdivisions*CliffSubdivisions-1)*6);

    auto AddVertex=[&](float X,float Y,float H)
    {
        // Use the same derivative at duplicated cell-edge vertices so local
        // refinement changes geometry without introducing a shading seam.
        float Step=FMath::Max(50.f,Spacing*.3f);
        if(Section==4)
        {
            const float ToEdge=FMath::Min(FMath::Min(X-float(CirqueMin.X),float(CirqueMin.X)+CirqueSize-X),
                FMath::Min(Y-float(CirqueMin.Y),float(CirqueMin.Y)+CirqueSize-Y));
            Step=FMath::Lerp(MountainStep*.3f,50.f,FMath::SmoothStep(0.f,MountainStep,ToEdge));
        }
        FVector Normal(
            TerrainHeightCm(X-Step,Y)-TerrainHeightCm(X+Step,Y),
            TerrainHeightCm(X,Y-Step)-TerrainHeightCm(X,Y+Step),2.f*Step);
        Normal.Normalize();
        Vertices.Emplace(X,Y,H);
        Normals.Add(Normal);
        if(Section==4)
        {
            FVector Tangent=FVector::VectorPlaneProject(FVector::ForwardVector,Normal).GetSafeNormal();
            if(Tangent.IsNearlyZero()) Tangent=FVector::VectorPlaneProject(FVector::RightVector,Normal).GetSafeNormal();
            Tangents.Emplace(Tangent,false);
        }
        const FVector2D Local=Airfield::Local(X,Y);
        UVs.Add(Section==3?FVector2D(Local.X/Airfield::HalfLength,Local.Y/Airfield::HalfWidth):FVector2D(X,Y)/4000.);
        const float AlpineRock=FMath::SmoothStep(.12f,.42f,1.f-float(Normal.Z))*FMath::SmoothStep(9000.f,22000.f,H);
        const float FaceRock=(1.f-FMath::SmoothStep(300.f,780.f,FMath::Abs(CliffOffset(X,Y))))
            *(1.f-FMath::SmoothStep(9000.f,17000.f,FMath::Abs(X-PilotX)))
            *FMath::SmoothStep(.24f,.55f,1.f-float(Normal.Z));
        const auto CirqueSurface=Cirque::Sample(X,Y);
        const float Rock=FMath::Max(FMath::Max(AlpineRock,FaceRock),FMath::Max(CirqueSurface.Cliff,CirqueSurface.Scree));
        // The limestone rim is a grassy plateau, not the bare alpine zone
        // selected solely by altitude elsewhere in the mountain range.
        const float Alpine=FMath::SmoothStep(17300.f,31600.f,H)*(1.f-CirqueSurface.Meadow);
        const float Shore=(1.f-FMath::SmoothStep(20.f,520.f,H-Valley::WaterLevel))
            *(1.f-FMath::SmoothStep(1000.f,3500.f,Valley::ShoreDistance(X,Y)));
        const float Wood=FMath::SmoothStep(.43f,.68f,Valley::Woodland(X,Y))*(1.f-Alpine);
        Colors.Add(FLinearColor(Wood,FMath::Max(Rock,Alpine*.85f),Shore,Section==4?CirqueSurface.Scree:1.f));
    };
    for (int32 XIndex = 0; XIndex < Points; ++XIndex)
    {
        for (int32 YIndex = 0; YIndex < Points; ++YIndex)
        {
            const float X = Origin.X + XIndex * Spacing;
            const float Y = Origin.Y + YIndex * Spacing;
            float H = Section==4?TerrainVertexHeightCm(X,Y):TerrainHeightCm(X,Y);
            // Snap the border of the fine mesh to the linear edges of the coarse triangles.
            if (Section!=4 && BorderStep > 0.f && (XIndex == 0 || XIndex == Cells))
            {
                const float Y0 = FMath::FloorToFloat(Y/BorderStep)*BorderStep;
                H = FMath::Lerp(TerrainHeightCm(X,Y0),TerrainHeightCm(X,Y0+BorderStep),(Y-Y0)/BorderStep);
            }
            else if (Section!=4 && BorderStep > 0.f && (YIndex == 0 || YIndex == Cells))
            {
                const float X0 = FMath::FloorToFloat(X/BorderStep)*BorderStep;
                H = FMath::Lerp(TerrainHeightCm(X0,Y),TerrainHeightCm(X0+BorderStep,Y),(X-X0)/BorderStep);
            }
            AddVertex(X,Y,H);
        }
    }
    int32 RefinedCells=0;
    for (int32 XIndex = 0; XIndex < Cells; ++XIndex)
    {
        for (int32 YIndex = 0; YIndex < Cells; ++YIndex)
        {
            const float X = Origin.X + XIndex*Spacing, Y = Origin.Y + YIndex*Spacing;
            const FVector2D Mid(X+Spacing*.5f,Y+Spacing*.5f);
            if ((Hole.bIsValid && Hole.IsInside(Mid)) || (Section==2 && (AirfieldBounds.IsInside(Mid) || CirqueBounds.IsInside(Mid))))
                continue;
            if(Section==4 && RefineCirqueCell(X,Y))
            {
                constexpr int32 FinePoints=CliffSubdivisions+1;
                const int32 First=Vertices.Num();
                for(int32 I=0;I<FinePoints;++I)
                    for(int32 J=0;J<FinePoints;++J)
                    {
                        const float U=X+I*CliffStep,V=Y+J*CliffStep;
                        AddVertex(U,V,TerrainVertexHeightCm(U,V));
                    }
                for(int32 I=0;I<CliffSubdivisions;++I)
                    for(int32 J=0;J<CliffSubdivisions;++J)
                    {
                        const int32 A=First+I*FinePoints+J,B=A+1,C=A+FinePoints;
                        Triangles.Append({A,B,C,B,C+1,C});
                    }
                ++RefinedCells;
                continue;
            }
            const int32 A = XIndex * Points + YIndex;
            const int32 B = A + 1;
            const int32 C = A + Points;
            const int32 D = C + 1;
            // ProceduralMesh uses clockwise front faces in Unreal's left-handed space.
            Triangles.Append({A, B, C, B, D, C});
        }
    }
    UProceduralMeshComponent* Target=Terrain;
    if(Section==4)
    {
        // Give the distant high-detail patch its own bounds. A section on the
        // world-sized root would be submitted even when the cirque is offscreen.
        const auto Existing=GetComponentsByTag(UProceduralMeshComponent::StaticClass(),TEXT("CirqueTerrain"));
        Target=Existing.Num()?CastChecked<UProceduralMeshComponent>(Existing[0]):nullptr;
        if(!Target)
        {
            Target=NewObject<UProceduralMeshComponent>(this,TEXT("CirqueTerrain"));
            Target->SetupAttachment(Terrain);
            Target->SetMobility(EComponentMobility::Static);
            Target->ComponentTags.Add(TEXT("CirqueTerrain"));
            SceneContact::Solid(Target,TEXT("ground"));
            Target->bUseAsyncCooking=false;
            Target->RegisterComponent();
        }
    }
    Target->CreateMeshSection_LinearColor(Section==4?0:Section, Vertices, Triangles, Normals, UVs, Colors, Tangents, true, false);
    auto* Material=Section==3 ? LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_AirfieldSward"))
        : Section==4 ? LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_CirqueSward")) : GroundMaterial();
    Target->SetMaterial(Section==4?0:Section,Material ? Material : GroundMaterial());
    if(Section==4) UE_LOG(LogTemp,Display,TEXT("CIRQUE_TERRAIN refined_cells=%d vertices=%d triangles=%d"),RefinedCells,Vertices.Num(),Triangles.Num()/3);
}

void ASlopeWorld::AddScenery()
{
    FRandomStream Random(4522);
    auto Group = [this](const TCHAR* Path)
    {
        auto* GroupMesh = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        GroupMesh->ComponentTags.Add(TEXT("GeneratedScenery"));
        GroupMesh->SetupAttachment(Terrain);
        GroupMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
        GroupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GroupMesh->SetCanEverAffectNavigation(false);
        GroupMesh->RegisterComponent();
        return GroupMesh;
    };
    auto* Boulders = Group(TEXT("/PCGBiomeSample/Meshes/PCG_Boulder_01"));
    SceneContact::Solid(Boulders,TEXT("rock"));
    auto Place = [&Random](UHierarchicalInstancedStaticMeshComponent* Batch, float X, float Y, float Height, float Embed)
    {
        if (!Batch->GetStaticMesh()) return;
        const FBox Bounds = Batch->GetStaticMesh()->GetBoundingBox();
        const float Scale = Height / FMath::Max(1.,Bounds.GetSize().Z);
        const float Z = ASlopeWorld::TerrainHeightCm(X,Y) - Bounds.Min.Z * Scale - Embed;
        Batch->AddInstance(FTransform(FRotator(0,Random.FRandRange(0,360),0), FVector(X,Y,Z), FVector(Scale)));
    };

    auto* Oaks=Group(TEXT("/Game/ArchVis/SampleScene/Tree/HillTree_02"));
    auto* Firs=Group(TEXT("/Game/Meadow/SilverFir/StaticMeshes/SilverFir"));
    UHierarchicalInstancedStaticMeshComponent* TreeGroups[] = {Oaks,Firs,
        Group(TEXT("/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_a")),
        Group(TEXT("/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_b")),
        Group(TEXT("/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_c"))};
    TArray<FTransform> Timber[UE_ARRAY_COUNT(TreeGroups)];
    for (int32 Stand=0; Stand<4700; ++Stand)
    {
        const float Range = Stand<1550 ? 104000.f : 268000.f;
        const FVector2D Centre(Random.FRandRange(-Range,Range),Random.FRandRange(-Range,Range));
        if (Stand>=1550 && FMath::Abs(Centre.X)<104000 && FMath::Abs(Centre.Y)<104000) continue;
        const float Cover=Valley::Woodland(Centre.X,Centre.Y);
        if (Cover<.455f) continue;
        const float Radius=Random.FRandRange(2100,7500);
        const int32 Count=FMath::Clamp(FMath::RoundToInt(12+125*(Cover-.425f)),8,58);
        for (int32 I=0;I<Count;++I)
        {
            const float A=Random.FRandRange(0,2*PI), R=Radius*FMath::Sqrt(Random.FRand());
            const float X=Centre.X+FMath::Cos(A)*R, Y=Centre.Y+FMath::Sin(A)*R;
            const float Dist=FVector2D(X-PilotX,Y-PilotY).Size();
            if (Dist<6200 || FVector2D(X,Y).Size()<5600) continue;
            const auto CirqueSurface=Cirque::Sample(X,Y);
            const float Z=CirqueSurface.Influence>0.f ? Valley::NaturalHeight(X,Y) : TerrainHeightCm(X,Y);
            if (Z<Valley::WaterLevel+65.f || Z>27800.f || Random.FRand()<FMath::SmoothStep(15000.f,27800.f,Z)) continue;
            // Preserve the seeded stands around the lake. Outlying stands now
            // use the same detailed trees as the reference stand by the house.
            const bool LakeStand=X>=-135000 && X<=65000 && Y>=-75000 && Y<=125000;
            const bool InnerStand=LakeStand || Dist<39000.f || (Dist<71000.f && I%5==0);
            const bool LowlandOak=InnerStand && Z<3500.f && Random.FRand()<.41f;
            const bool Riverside=Z<6200.f && Random.FRand()<.48f;
            const bool YoungPine=LakeStand && R>.70f*Radius && I%6==0;
            const int32 Kind=YoungPine ? 2+(Stand+I)%3 : (LowlandOak || (!InnerStand && Riverside) ? 0 : 1);
            auto* Batch=TreeGroups[Kind];
            if (!Batch->GetStaticMesh()) continue;
            const FBox Bounds=Batch->GetStaticMesh()->GetBoundingBox();
            const float Height=YoungPine ? Random.FRandRange(165,430) : Random.FRandRange(800,1850);
            const float Scale=Height/FMath::Max(1.,Bounds.GetSize().Z);
            const float WidthScale=Scale*Random.FRandRange(.90f,1.45f);
            const float Yaw=Random.FRandRange(0,360);
            const FVector2D Field=Airfield::Local(X,Y);
            if ((FMath::Abs(Field.X)<Airfield::HalfLength+2400 && FMath::Abs(Field.Y)<Airfield::HalfWidth+1000)
                || FVector2D::DistSquared(FVector2D(X,Y),Airfield::Hut)<FMath::Square(1500.)) continue;
            if (FVector2D(X-ChaletX-450.f,Y-ChaletY).SizeSquared()<FMath::Square(1550.f) ||
                FVector2D(X+74000.f,Y-8500.f).SizeSquared()<FMath::Square(1000.f)) continue;
            // Consume the same seeded random sequence before filtering: distant
            // stands outside the new site retain their previous appearance.
            if (CirqueSurface.Influence<.15f && CirqueSurface.Cliff<.12f && CirqueSurface.Scree<.20f)
                Timber[Kind].Emplace(FRotator(0,Yaw,0),FVector(X,Y,TerrainHeightCm(X,Y)-Bounds.Min.Z*Scale-(YoungPine ? 3.f : 12.f)),
                                     FVector(WidthScale,WidthScale,Scale));
        }
    }
    for (int32 Kind=0;Kind<UE_ARRAY_COUNT(TreeGroups);++Kind) TreeGroups[Kind]->AddInstances(Timber[Kind],false,false,false);
    UE_LOG(LogTemp,Display,TEXT("SimPlaneur: woodland oaks=%d silver firs=%d young pines=%d"),Timber[0].Num(),Timber[1].Num(),Timber[2].Num()+Timber[3].Num()+Timber[4].Num());
    for (int32 I=0;I<540;++I)
    {
        const float X=Random.FRandRange(-80000,80000),Y=Random.FRandRange(-80000,80000);
        if (TerrainHeightCm(X,Y)>Valley::WaterLevel && FVector2D(X-PilotX,Y-PilotY).Size()>2200)
            Place(Boulders,X,Y,Random.FRandRange(45,170),19);
    }
    // Two landmarks give the pilot a close scale reference.
    Place(TreeGroups[0],PilotX-900,PilotY+2800,920,8);
    Place(Oaks,PilotX+3750,PilotY+700,1020,8);

    auto* TreeColliders = NewObject<UInstancedStaticMeshComponent>(this);
    TreeColliders->SetupAttachment(Terrain);
    TreeColliders->SetMobility(EComponentMobility::Static);
    TreeColliders->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder")));
    TreeColliders->SetVisibility(false);
    TreeColliders->SetHiddenInGame(true);
    TreeColliders->SetCastShadow(false);
    SceneContact::Solid(TreeColliders,TEXT("tree"));
    TArray<FTransform> Cylinders;
    for (const auto* Trees : TreeGroups)
    {
        if (!Trees->GetStaticMesh()) continue;
        const FBox Bounds=Trees->GetStaticMesh()->GetBoundingBox();
        const FVector Size=Bounds.GetSize();
        for (int32 Index=0;Index<Trees->GetInstanceCount();++Index)
        {
            FTransform Tree;
            Trees->GetInstanceTransform(Index,Tree);
            const FVector Scale=Tree.GetScale3D();
            const float Radius=.35f*FMath::Max(Size.X*Scale.X,Size.Y*Scale.Y);
            const float Height=Size.Z*Scale.Z;
            const FVector Centre=Tree.TransformPosition(FVector(0,0,Bounds.GetCenter().Z));
            if (Centre.X>=-138000 && Centre.X<=68000 && Centre.Y>=-78000 && Centre.Y<=128000)
            {
                const float R=.25f*(Size.X*Scale.X+Size.Y*Scale.Y);
                for (int32 A=FMath::FloorToInt((Centre.X-R)/1000); A<=FMath::FloorToInt((Centre.X+R)/1000); ++A)
                    for (int32 B=FMath::FloorToInt((Centre.Y-R)/1000); B<=FMath::FloorToInt((Centre.Y+R)/1000); ++B)
                        Crowns.FindOrAdd(FIntPoint(A,B)).Emplace(Centre.X,Centre.Y,R);
            }
            Cylinders.Emplace(FQuat::Identity,Centre,FVector(Radius/50.f,Radius/50.f,Height/100.f));
        }
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("ContactProof")) && FParse::Param(FCommandLine::Get(),TEXT("SimSmoke")))
    {
        // Aim the test obstacle at the wing, including drift under the actual weather.
        FGliderDynamics Probe;
        if (Probe.LoadModel(Probe.TopyModelPath()))
        {
            for (int32 I = 0; I < 240; ++I)
            {
                ValleyAir::SampleGlider(Probe, I * Probe.TimeStep);
                Probe.Step(0, 0);
            }
            const FVector Wingtip = Probe.WorldPose().TransformPosition(FVector(0, 60, 7.5));
            Cylinders.Emplace(FQuat::Identity, Wingtip, FVector(.36, .36, 14));
        }
    }
    TreeColliders->AddInstances(Cylinders,false,false,false);
    TreeColliders->RegisterComponent();
    UE_LOG(LogTemp,Display,TEXT("SimPlaneur: %d static tree cylinders"),Cylinders.Num());

    auto* Flowers = Group(TEXT("/Game/Meadow/Dandelion/StaticMeshes/Dandelion"));
    Flowers->SetCullDistances(1600,2500);
    auto* Blades = Group(TEXT("/Game/Meadow/BentGrass/StaticMeshes/BentGrass"));
    auto* Fescue = Group(TEXT("/Game/Meadow/FloweringFescue/StaticMeshes/FloweringFescue"));
    auto* LowGrass = Group(TEXT("/Game/Meadow/CreepingBent/StaticMeshes/CreepingBent"));
    auto* LeafMaterial = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/M_LivingGrass"));
    GrassMaterial = LeafMaterial ? UMaterialInstanceDynamic::Create(LeafMaterial,this) : nullptr;
    for (auto* Patch : {Blades,Fescue,LowGrass})
    {
        Patch->SetCullDistances(3800,5000);
        Patch->SetEvaluateWorldPositionOffset(true);
        if (GrassMaterial) Patch->SetMaterial(0,GrassMaterial);
    }
    LowGrass->SetCullDistances(2200,3400);
    LowGrass->SetCastShadow(false);
    PilotGrass = {Blades,Fescue,LowGrass};
    TArray<FTransform> Leaves, SeedHeads, Understory;
    // Jittered crowns prevent bare random gaps at the pilot's feet. The cover thins
    // gradually down the slope, where sub-pixel leaves give way to the ground material.
    constexpr float Spacing = 36.f;
    for (int32 Row=-116; Row<=116; ++Row)
    {
        for (int32 Col=-116; Col<=116; ++Col)
        {
            const FVector2D Offset((Row+Random.FRandRange(-.45f,.45f))*Spacing,(Col+Random.FRandRange(-.45f,.45f))*Spacing);
            const float Distance = Offset.Size();
            const float Density = FMath::Clamp(FMath::Square(1350.f/FMath::Max(1350.f,Distance)),0.f,1.f);
            if (Distance>4100 || Random.FRand()>Density) continue;
            const float X=PilotX+Offset.X, Y=PilotY+Offset.Y;
            const float Z=TerrainHeightCm(X,Y);
            const FVector Normal = FVector(TerrainHeightCm(X-5,Y)-TerrainHeightCm(X+5,Y),
                                           TerrainHeightCm(X,Y-5)-TerrainHeightCm(X,Y+5),10).GetSafeNormal();
            if (Normal.Z<.60f || Z<Valley::WaterLevel+60) continue;
            const float Shade=CrownCover(X,Y);
            if (Shade>.01f && Random.FRand()<Shade*.85f) continue;
            const float Yaw = Random.FRandRange(-PI,PI);
            const FQuat Rotation=FRotationMatrix::MakeFromZX(Normal,FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),0)).ToQuat();
            const float Pocket=FMath::PerlinNoise2D(Offset/220.f);
            const float Width=Random.FRandRange(.78f,1.2f);
            const float Height=Random.FRandRange(.68f,1.1f)+.32f*Pocket;
            auto& Instances = Shade>.5f ? Understory : Random.FRand()<.2f+.17f*Pocket ? SeedHeads : Leaves;
            Instances.Emplace(Rotation,FVector(X,Y,Z-1.f),FVector(Width,Width,Height));
            if (Distance < 2000 && Random.FRand() < .83f)
            {
                const float U = X + .5f*Spacing, V = Y + .5f*Spacing;
                const float Spread = Random.FRandRange(1.02f,1.35f);
                Understory.Emplace(FQuat(Normal,1.7f)*Rotation,FVector(U,V,TerrainHeightCm(U,V)-1.f),
                                   FVector(Spread,Spread,Random.FRandRange(.55f,.88f)));
            }
            if (Pocket>.14f && Random.FRand()<.015f && Distance<2300)
                Place(Flowers,X,Y,Random.FRandRange(16,26),1.f);
        }
    }
    Blades->AddInstances(Leaves,false,false,false);
    Fescue->AddInstances(SeedHeads,false,false,false);
    LowGrass->AddInstances(Understory,false,false,false);
    UE_LOG(LogTemp,Display,TEXT("SimPlaneur: meadow has %d blade clumps, %d flowering clumps and %d low clumps"),Leaves.Num(),SeedHeads.Num(),Understory.Num());
}
