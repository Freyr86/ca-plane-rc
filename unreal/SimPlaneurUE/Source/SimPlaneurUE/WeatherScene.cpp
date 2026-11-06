#include "WeatherScene.h"
#include "ValleyAir.h"
#include "SlopeFlightPawn.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 DropCount=512;
    const FVector2D Banks[]={ {200,300},{-1350,-550},{2800,2100},{3400,1700},{2250,1550},{1800,-1800} };
    FVector World(const FVector2D& XY,double Height) { return FVector(XY.X,XY.Y,Height)*100.; }
}

AWeatherScene::AWeatherScene()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WeatherRoot")));
    for(int32 I=0;I<18;++I)
    {
        auto* Fog=CreateDefaultSubobject<ULocalFogVolumeComponent>(*FString::Printf(TEXT("SlopeCloud%d"),I));
        Fog->SetupAttachment(RootComponent);
        Fog->SetMobility(EComponentMobility::Movable);
        Fog->SetVisibility(false);
        LowFog.Add(Fog);
    }
    for(int32 I=0;I<9;++I)
    {
        auto* Fog=CreateDefaultSubobject<ULocalFogVolumeComponent>(*FString::Printf(TEXT("ShowerVeil%d"),I));
        Fog->SetupAttachment(RootComponent);
        Fog->SetMobility(EComponentMobility::Movable);
        Fog->SetVisibility(false);
        RainFog.Add(Fog);
    }
    Drops=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LocalRainStreaks"));
    Drops->SetupAttachment(RootComponent);
    Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Drops->SetCastShadow(false);
    Drops->SetVisibility(false);
    Bolt=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LightningBolt"));
    Bolt->SetupAttachment(RootComponent);
    Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bolt->SetCastShadow(false);
    Bolt->SetVisibility(false);
    Flash=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("LightningFlash"));
    Flash->SetupAttachment(RootComponent);
    Flash->SetRelativeRotation(FRotator(-65,35,0));
    Flash->SetCastShadows(false);
    Flash->SetLightColor(FLinearColor(.68f,.78f,1.f));
    Flash->SetIntensity(0.f);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RainAsset(TEXT("/Game/Meadow/Weather/M_RainStreak"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoltAsset(TEXT("/Game/Meadow/Weather/M_Lightning"));
    if(RainAsset.Succeeded()) Drops->SetMaterial(0,RainAsset.Object);
    if(BoltAsset.Succeeded()) Bolt->SetMaterial(0,BoltAsset.Object);
}

void AWeatherScene::BeginPlay()
{
    Super::BeginPlay();
    if(Drops->GetMaterial(0))
    {
        RainMaterial=UMaterialInstanceDynamic::Create(Drops->GetMaterial(0),this);
        Drops->SetMaterial(0,RainMaterial);
    }
    TArray<int32> Indices;
    Particles.SetNum(DropCount);
    for(int32 I=0;I<DropCount;++I)
    {
        for(int32 J=0;J<4;++J) { Vertices.Add(FVector::ZeroVector); Normals.Add(FVector::UpVector); Colors.Add(FLinearColor::Transparent); }
        UV.Append({{0,0},{1,0},{1,1},{0,1}});
        const int32 B=I*4;
        Indices.Append({B,B+1,B+2,B,B+2,B+3});
    }
    Drops->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colors,{},false);
    UE_LOG(LogTemp,Display,TEXT("WEATHER_SCENE ready low_cloud_volumes=%d shower_volumes=%d drops=%d"),LowFog.Num(),RainFog.Num(),DropCount);
}

void AWeatherScene::UpdateFog(double Seconds)
{
    const auto W=ValleyAir::Settings();
    const bool Low=W.LowClouds && W.Sky>=3;
    const float Day=FMath::Clamp(float(ValleyAir::SunDirection(W.SolarMinutes).Z)*2.f,0.f,1.f);
    const FLinearColor CloudTint=FMath::Lerp(FLinearColor(.09f,.12f,.18f),W.Sky==6?FLinearColor(.38f,.43f,.49f):FLinearColor(.79f,.83f,.86f),Day);
    const double Bearing=FMath::DegreesToRadians(W.WindFromDegrees);
    const FVector2D Flow(-FMath::Cos(Bearing),-FMath::Sin(Bearing));
    for(int32 I=0;I<LowFog.Num();++I)
    {
        auto* Fog=LowFog[I].Get();
        Fog->SetVisibility(Low && W.CloudCoverage>.01);
        if(!Low) continue;
        const int32 Group=I/3,Part=I%3;
        const FVector2D Side(-Flow.Y,Flow.X);
        const FVector2D XY=Banks[Group]+Side*((Part-1)*190.)+Flow*(70.*FMath::Sin(Seconds*.003+Group));
        const double Base=ValleyAir::CloudBaseAt(XY,W);
        const double Radius=210.+Part*55.;
        // Overlapping analytic volumes form ragged banks across the crest.
        // They occupy actual world space and fade around the pilot when entered.
        Fog->SetWorldLocation(World(XY,Base+45.+Part*18.));
        Fog->SetWorldScale3D(FVector(Radius/5.));
        Fog->SetRadialFogExtinction(float((W.Sky==3?.48:.70)*W.CloudCoverage));
        // Height fog grows exponentially below its origin: in a cloud-sized
        // volume that hides the whole valley. Radial density has finite depth.
        Fog->SetHeightFogExtinction(0.f);
        Fog->SetHeightFogFalloff(2.f);
        Fog->SetHeightFogOffset(-.15f);
        Fog->SetFogAlbedo(CloudTint);
        Fog->SetFogPhaseG(.35f);
        Fog->SetFogStartDistance(0.f);
    }
    const auto Cells=ValleyAir::WeatherCells(Seconds,W);
    for(int32 I=0;I<RainFog.Num();++I)
    {
        auto* Fog=RainFog[I].Get();
        const bool Enabled=W.Sky>=5 && W.CloudCoverage>.01 && W.RainStrength>.01 && Cells.IsValidIndex(I);
        Fog->SetVisibility(Enabled);
        if(!Enabled) continue;
        const auto& Cell=Cells[I];
        const double Ground=ValleyAir::SurfaceAltitude(Cell.Centre.X,Cell.Centre.Y);
        const double Base=ValleyAir::CloudBaseAt(Cell.Centre,W);
        Fog->SetWorldLocation(World(Cell.Centre,(Ground+Base)*.5));
        Fog->SetWorldScale3D(FVector(Cell.RadiusMeters*.7/5.));
        Fog->SetRadialFogExtinction(float(W.RainStrength*(W.Sky==6?.34:.16)));
        Fog->SetHeightFogExtinction(.006f);
        Fog->SetHeightFogFalloff(2.5f);
        Fog->SetFogAlbedo(FMath::Lerp(FLinearColor(.06f,.09f,.15f),FLinearColor(.52f,.59f,.66f),Day));
        Fog->SetFogPhaseG(.15f);
    }
}

void AWeatherScene::UpdateRain(float Dt,const FVector& Eye,const FVector& Right,double Seconds)
{
    const auto W=ValleyAir::Settings();
    const FVector EarNED(Eye.X*.01,Eye.Y*.01,-Eye.Z*.01);
    LocalRain=float(ValleyAir::RainAt(EarNED,Seconds,W));
    const bool Enabled=LocalRain>.005f && RainMaterial;
    Drops->SetVisibility(Enabled);
    if(!Enabled) { for(auto& P:Particles) P.Speed=0; return; }
    const double Bearing=FMath::DegreesToRadians(W.WindFromDegrees);
    const FVector Wind(-FMath::Cos(Bearing)*W.WindSpeedMps*70.,-FMath::Sin(Bearing)*W.WindSpeedMps*70.,0);
    const float Day=FMath::Clamp(float(ValleyAir::SunDirection(W.SolarMinutes).Z)*2.f,0.f,1.f);
    // Unlit rain still passes through the physical daylight exposure. A small
    // night floor avoids luminous drops after sunset while preserving the day.
    RainMaterial->SetScalarParameterValue(TEXT("Brightness"),.3f+Day*1500.f);
    for(int32 I=0;I<Particles.Num();++I)
    {
        auto& D=Particles[I];
        if(D.Speed==0 || FMath::Abs(D.P.X-Eye.X)>1400 || FMath::Abs(D.P.Y-Eye.Y)>1400 || D.P.Z<Eye.Z-1400 || D.P.Z<D.Floor || D.P.Z>Eye.Z+2600)
        {
            D.P=Eye+FVector(Random.FRandRange(-1350,1350),Random.FRandRange(-1350,1350),Random.FRandRange(-400,2400));
            D.Speed=Random.FRandRange(1550,2300); D.Length=Random.FRandRange(25,60);
            D.Width=Random.FRandRange(.35f,.65f);
            // Cache the local surface on birth, including the short downwind
            // landing position. No world trace is needed for every drop/frame.
            const double Here=ValleyAir::SurfaceAltitude(D.P.X*.01,D.P.Y*.01)*100.;
            const double FlightTime=FMath::Clamp((D.P.Z-Here)/D.Speed,0.,3.);
            const FVector Landing=D.P+Wind*FlightTime;
            D.Floor=FMath::Max(Here,ValleyAir::SurfaceAltitude(Landing.X*.01,Landing.Y*.01)*100.)+5.;
        }
        const FVector Velocity=Wind+FVector(0,0,-D.Speed);
        D.P+=Velocity*Dt;
        const FVector Tail=D.P-Velocity.GetSafeNormal()*D.Length;
        const FVector Half=Right*D.Width;
        const int32 B=I*4;
        Vertices[B]=D.P-Half; Vertices[B+1]=D.P+Half; Vertices[B+2]=Tail+Half; Vertices[B+3]=Tail-Half;
        const float NearFade=FMath::Clamp(float((D.P-Eye).Size()/200.),0.f,1.f);
        const float Alpha=LocalRain*.43f*NearFade*(D.P.Z>D.Floor?1.f:0.f);
        for(int32 J=0;J<4;++J) Colors[B+J]=FLinearColor(.7f,.8f,.9f,Alpha);
    }
    Drops->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,{});
}

void AWeatherScene::Strike(double Seconds)
{
    const auto W=ValleyAir::Settings();
    const auto Cells=ValleyAir::WeatherCells(Seconds,W);
    if(Cells.IsEmpty()) return;
    const auto& Cell=Cells[LightningSerial%Cells.Num()];
    const FVector2D Point=Cell.Centre+FVector2D(StormRandom.FRandRange(-150,150),StormRandom.FRandRange(-150,150));
    const double Ground=ValleyAir::SurfaceAltitude(Point.X,Point.Y);
    LightningPosition=World(Point,Ground);
    const double Top=FMath::Max(Ground+220.,ValleyAir::CloudBaseAt(Point,W)+120.);
    TArray<FVector> V,N; TArray<FVector2D> Tex; TArray<FLinearColor> C; TArray<int32> Tri;
    FVector Previous=World(Point+FVector2D(40,15),Top);
    for(int32 I=1;I<=20;++I)
    {
        const double T=double(I)/20.;
        FVector Next=FMath::Lerp(World(Point+FVector2D(40,15),Top),LightningPosition,T);
        if(I<20) Next+=FVector(StormRandom.FRandRange(-1100,1100),StormRandom.FRandRange(-1100,1100),0);
        for(int32 Axis=0;Axis<2;++Axis)
        {
            // A sub-pixel core is lost in temporal anti-aliasing at a distance.
            const FVector Half=Axis==0?FVector(60,0,0):FVector(0,60,0);
            const int32 B=V.Num();
            V.Append({Previous-Half,Previous+Half,Next+Half,Next-Half});
            Tex.Append({{0,0},{1,0},{1,1},{0,1}});
            for(int J=0;J<4;++J) { N.Add(FVector::UpVector); C.Add(FLinearColor(.7f,.82f,1.f,1.f)); }
            Tri.Append({B,B+1,B+2,B,B+2,B+3});
        }
        Previous=Next;
    }
    Bolt->CreateMeshSection_LinearColor(0,V,Tri,N,Tex,C,{},false);
    LightningTime=GetWorld()->GetTimeSeconds();
    ++LightningSerial;
    UE_LOG(LogTemp,Display,TEXT("WEATHER_LIGHTNING serial=%d location=%s vertices=%d material=%s"),LightningSerial,*LightningPosition.ToString(),V.Num(),*GetPathNameSafe(Bolt->GetMaterial(0)));
}

double AWeatherScene::GetLightningAge() const
{
    return GetWorld()?GetWorld()->GetTimeSeconds()-LightningTime:0.;
}

bool AWeatherScene::IsLightningVisible() const
{
    return Bolt && Bolt->IsVisible();
}

void AWeatherScene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!PC || !PC->PlayerCameraManager) return;
    const auto* Plane=Cast<ASlopeFlightPawn>(PC->GetPawn());
    const double Time=Plane?Plane->GetWeatherVisualSeconds():GetWorld()->GetTimeSeconds();
    LowUpdate+=DeltaSeconds; RainUpdate+=DeltaSeconds;
    const auto W=ValleyAir::Settings();
    const bool Changed=LastSky!=W.Sky;
    if(LowUpdate>=.2f || Changed) { UpdateFog(Time); LowUpdate=0; }
    if(RainUpdate>=1.f/30.f)
    {
        const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
        const FVector Right=FRotationMatrix(PC->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
        UpdateRain(FMath::Min(RainUpdate,.1f),Eye,Right,Time); RainUpdate=0;
    }
    const double Now=GetWorld()->GetTimeSeconds();
    if(Changed) NextLightning=Now+8.;
    if(W.Sky==6 && W.CloudCoverage>.01 && W.RainStrength>.05 && Now>=NextLightning)
    {
        Strike(Time); NextLightning=Now+StormRandom.FRandRange(24,43);
    }
    const double Age=Now-LightningTime;
    const bool Pulse=W.Sky==6 && (Age<.25 || (Age>.34 && Age<.45));
    Bolt->SetVisibility(Pulse);
    Flash->SetIntensity(Pulse?18000.f:0.f);
    LastSky=W.Sky;
}
