#include "SceneContact.h"
#include "SlopeWorld.h"
#include "ValleyLandscape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"

bool SceneContact::CheckSite(UWorld* World)
{
    auto VerifySweep=[World](FVector Start,FVector End,FName Expected)
    {
        FTransform Rest;
        FName Kind;
        const bool Hit=SweepTopy(World,nullptr,FTransform(Start),FTransform(End),Rest,Kind);
        return Expected.IsNone() ? !Hit : Hit && Kind==Expected;
    };
    const FVector Site(ASlopeWorld::PilotX,ASlopeWorld::PilotY,ASlopeWorld::TerrainHeightCm(ASlopeWorld::PilotX,ASlopeWorld::PilotY));
    const FVector Lake(Valley::LakeX,Valley::LakeY,Valley::WaterLevel);
    const FVector Peak(-360000,0,ASlopeWorld::TerrainHeightCm(-360000,0));
    bool Ground=VerifySweep(Site+FVector(0,0,200),Site-FVector(0,0,200),TEXT("ground"));
    bool Remote=VerifySweep(Peak+FVector(0,0,2600),Peak-FVector(0,0,2600),TEXT("ground"));
    bool Water=VerifySweep(Lake+FVector(0,0,1000),Lake-FVector(0,0,1000),TEXT("water"));
    const float X=ASlopeWorld::PilotX-900, Y=ASlopeWorld::PilotY+2800;
    const FVector Oak(X,Y,ASlopeWorld::TerrainHeightCm(X,Y));
    bool Tree=VerifySweep(Oak+FVector(0,0,1300),Oak+FVector(0,0,500),TEXT("tree"));
    bool Clear=VerifySweep(Oak+FVector(-200,0,1300),Oak+FVector(200,0,1300),NAME_None);
    bool Rock=false;
    for (TActorIterator<ASlopeWorld> SiteActor(World);SiteActor;++SiteActor)
        for (UActorComponent* Item : SiteActor->GetComponentsByTag(UInstancedStaticMeshComponent::StaticClass(),TEXT("rock")))
        {
            auto* Rocks=CastChecked<UInstancedStaticMeshComponent>(Item);
            const FBox Bounds=Rocks->GetStaticMesh()->GetBoundingBox();
            for (int32 I=0;!Rock && I<FMath::Min(32,Rocks->GetInstanceCount());++I)
            {
                FTransform T;
                Rocks->GetInstanceTransform(I,T,true);
                const FVector P=T.TransformPosition(Bounds.GetCenter());
                Rock=VerifySweep(P+FVector(0,0,300),P-FVector(0,0,150),TEXT("rock"));
            }
        }
    UE_LOG(LogTemp,Display,TEXT("CONTACT_PROOF pilot_ground=%d mountain_ground=%d lake=%d rock=%d tree=%d clear_above=%d"),Ground,Remote,Water,Rock,Tree,Clear);
    return Ground && Remote && Water && Rock && Tree && Clear;
}

void SceneContact::Solid(UPrimitiveComponent* Component,FName Kind)
{
    Component->SetCollisionProfileName(TEXT("BlockAll"));
    Component->SetCollisionResponseToChannel(Channel,ECR_Block);
    Component->SetCanEverAffectNavigation(false);
    Component->ComponentTags.AddUnique(Kind);
}

bool SceneContact::SweepTopy(UWorld* World,const AActor* Plane,const FTransform& From,const FTransform& To,
                             FTransform& Rest,FName& Kind,FVector FuselageCentre,FVector2D SizeScale)
{
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(TopySceneContact),false,Plane);
    const double Angle=FMath::RadiansToDegrees(From.GetRotation().AngularDistance(To.GetRotation()));
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(Angle/3.),1,60);
    FTransform Start=From;
    for (int32 Step=1;Step<=Steps;++Step)
    {
        FTransform End;
        End.Blend(From,To,double(Step)/Steps);
        FHitResult First;
        auto Probe=[&](FVector LocalCentre,FVector Axis,float Radius,float HalfLength)
        {
            const FQuat Mid=FQuat::Slerp(Start.GetRotation(),End.GetRotation(),.5);
            const FQuat Rot=Mid*FQuat::FindBetweenNormals(FVector::UpVector,Axis);
            FHitResult Hit;
            if (World->SweepSingleByChannel(Hit,Start.TransformPosition(LocalCentre),End.TransformPosition(LocalCentre),
                Rot,Channel,FCollisionShape::MakeCapsule(Radius,HalfLength),Query) &&
                (!First.bBlockingHit || Hit.Time<First.Time)) First=Hit;
        };
        // The wing panels follow the Topy's dihedral. The central capsule covers nose and tail.
        Probe(FuselageCentre,FVector::ForwardVector,4.5f * SizeScale.X,43.f * SizeScale.X);
        for (float Side : {-1.f,1.f})
            Probe(FVector(0,Side*30,3.8) * SizeScale.Y,FVector(0,Side*59.5,7.5).GetSafeNormal(),4.0f,30.5f * SizeScale.Y);
        if (First.bBlockingHit)
        {
            Rest.Blend(Start,End,First.Time);
            const float Skin=First.bStartPenetrating ? First.PenetrationDepth+1.f : .5f;
            Rest.AddToTranslation(First.Normal*Skin);
            const UPrimitiveComponent* Surface=First.GetComponent();
            Kind=TEXT("ground");
            for (FName Name : {FName(TEXT("tree")),FName(TEXT("rock")),FName(TEXT("water"))})
                if (Surface && Surface->ComponentHasTag(Name)) Kind=Name;
            return true;
        }
        Start=End;
    }
    return false;
}
