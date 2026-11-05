#include "WinchStation.h"
#include "SceneContact.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AWinchStation::AWinchStation()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Drum=CreateDefaultSubobject<USceneComponent>(TEXT("Drum"));
    Drum->SetupAttachment(RootComponent);
    Drum->SetRelativeLocation({-24,0,30});
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Base(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    auto Part=[&](const TCHAR* Name,bool Round,USceneComponent* Parent,FVector P,FVector Size,FLinearColor Colour,FRotator R=FRotator::ZeroRotator,bool Solid=false)
    {
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(Parent);
        Mesh->SetStaticMesh(Round?Cylinder.Object:Cube.Object);
        Mesh->SetRelativeLocation(P); Mesh->SetRelativeRotation(R); Mesh->SetRelativeScale3D(Size/100.);
        Mesh->SetMaterial(0,Base.Object);
        Colours.Add(FName(Name),Colour);
        if(Solid) SceneContact::Solid(Mesh,TEXT("winch"));
        else Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    };
    const FLinearColor Metal(.12f,.15f,.16f), Ivory(.82f,.77f,.61f), Petrol(.02f,.17f,.18f), Gold(.95f,.53f,.11f);
    Part(TEXT("Frame"),false,RootComponent,{-30,0,7},{90,62,10},Metal,{},true);
    Part(TEXT("Battery"),false,RootComponent,{-56,0,24},{32,44,25},Petrol,{},true);
    Part(TEXT("Motor"),true,RootComponent,{-24,-38,30},{24,24,28},Metal,{0,0,90});
    Part(TEXT("CableDrum"),true,Drum,{0,0,0},{26,26,34},Ivory,{0,0,90});
    Part(TEXT("DiscLeft"),true,Drum,{0,-19,0},{39,39,3},Gold,{0,0,90});
    Part(TEXT("DiscRight"),true,Drum,{0,19,0},{39,39,3},Gold,{0,0,90});
    Part(TEXT("RotationMark"),false,Drum,{8,-21,0},{15,2,3},Metal);
    Part(TEXT("Axle"),true,RootComponent,{-24,0,30},{7,7,66},Metal,{0,0,90});
    for(int I=0;I<2;++I)
    {
        Part(I?TEXT("SupportRight"):TEXT("SupportLeft"),false,RootComponent,{-24,I?27.:-27.,19},{8,5,30},Petrol,{},true);
        Part(I?TEXT("PegRight"):TEXT("PegLeft"),true,RootComponent,{4,I?29.:-29.,10},{3,3,26},Metal);
    }
}

void AWinchStation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TInlineComponentArray<UStaticMeshComponent*> Parts(this);
    for(auto* Part:Parts)
    {
        auto* Paint=Part->CreateDynamicMaterialInstance(0);
        if(Paint) Paint->SetVectorParameterValue(TEXT("Color"),Colours.FindRef(Part->GetFName()));
    }
}

void AWinchStation::TurnDrum(double Metres)
{
    Drum->AddLocalRotation(FRotator(FMath::RadiansToDegrees(Metres/.13),0,0));
}
