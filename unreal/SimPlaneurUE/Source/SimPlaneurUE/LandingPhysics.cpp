#include "SlopeFlightPawn.h"
#include "LandingDamage.h"
#include "ProceduralMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void ASlopeFlightPawn::ClearGroundPhysics()
{
    bGroundPhysics=false; bLandingPaused=false; ImpactDamage=0;
    LandingBody->SetSimulatePhysics(false);
    LandingBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LandingBody->AttachToComponent(SceneRoot,FAttachmentTransformRules::KeepWorldTransform);
    LandingBody->SetRelativeTransform(FTransform::Identity);
}

void ASlopeFlightPawn::BeginGroundPhysics(const FTransform& Pose,const FHitResult& Hit,double Length,double Span,double Rise,double Sweep,FVector4 Wheel)
{
    const FVector V=Flight.VelocityNED();
    LandingVelocity=FVector(V.X,V.Y,-V.Z)*100.;
    LandingAngular=Flight.AngularVelocityWorld();
    const FVector AtContact=LandingVelocity+FVector::CrossProduct(LandingAngular,Hit.ImpactPoint-Pose.GetLocation());
    const double ImpactSpeed=FMath::Max(0.,-FVector::DotProduct(AtContact,Hit.ImpactNormal))*.01;
    ImpactDamage=LandingDamage(ImpactSpeed/Flight.ImpactTolerance(),ContactKind==TEXT("water"),ContactKind!=TEXT("ground"),
        Pose.GetRotation().GetUpVector().Z<.3);
    StopFlightAt(Pose); // Disable aerodynamic control; Chaos now owns motion.
    bGroundPhysics=true;
    LandingBody->ClearCollisionConvexMeshes();
    TArray<TArray<FVector>> Hulls;
    auto Box=[&](FVector Centre,FVector Extent)
    {
        TArray<FVector> Points;
        for(int X:{-1,1}) for(int Y:{-1,1}) for(int Z:{-1,1})
            Points.Add(Centre+FVector(X*Extent.X,Y*Extent.Y,Z*Extent.Z));
        Hulls.Add(Points);
    };
    Box(FuselageCentre,{Length*.5,Length*.052,Length*.052});
    for(int Side:{-1,1})
    {
        TArray<FVector> Points;
        for(int End:{0,1}) for(int X:{-1,1}) for(int Z:{-1,1})
        {
            const double Y=End*Span*.5;
            Points.Add({-End*Sweep+X*Length*(End?.035:.09),Side*Y,Y*Rise+Z*2.5});
        }
        Hulls.Add(Points);
    }
    if(Wheel.W>0.) Box(FVector(Wheel.X,Wheel.Y,Wheel.Z),FVector(Wheel.W*.8,Wheel.W*.4,Wheel.W));
    LandingBody->SetCollisionConvexMeshes(Hulls);
    LandingBody->SetWorldTransform(Pose);
    auto* Surface=NewObject<UPhysicalMaterial>(LandingBody);
    Surface->Friction=.45f; Surface->Restitution=.12f;
    LandingBody->SetPhysMaterialOverride(Surface);
    LandingBody->SetCollisionProfileName(TEXT("PhysicsActor"));
    LandingBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    LandingBody->SetMassOverrideInKg(NAME_None,Flight.Loading().MassGrams*.001f,true);
    LandingBody->SetLinearDamping(.12f);
    LandingBody->SetAngularDamping(.7f);
    LandingBody->SetUseCCD(true);
    LandingBody->SetSimulatePhysics(true);
    LandingBody->SetPhysicsLinearVelocity(LandingVelocity);
    LandingBody->SetPhysicsAngularVelocityInRadians(LandingAngular);
    LandingBody->WakeAllRigidBodies();
    SiteNotice=ImpactDamage==2?TEXT("CRASH · commandes coupées"):ImpactDamage==1?TEXT("DÉGÂTS · commandes coupées"):TEXT("Contact au sol · roulage et glissade");
}

void ASlopeFlightPawn::LandingHit(UPrimitiveComponent*,AActor*,UPrimitiveComponent* Other,FVector Impulse,const FHitResult&)
{
    if(!bGroundPhysics) return;
    const double DeltaSpeed=Impulse.Size()/FMath::Max(.05,Flight.Loading().MassGrams*.001)*.01;
    ImpactDamage=FMath::Max(ImpactDamage,LandingDamage(DeltaSpeed/Flight.ImpactTolerance(),Other && Other->ComponentHasTag(TEXT("water"))));
    if(ImpactDamage) SiteNotice=ImpactDamage==2?TEXT("CRASH · commandes coupées"):TEXT("DÉGÂTS · commandes coupées");
}

void ASlopeFlightPawn::UpdateGroundPhysics(float)
{
    if(bMenuOpen || bNoclip)
    {
        if(!bLandingPaused)
        {
            LandingVelocity=LandingBody->GetPhysicsLinearVelocity();
            LandingAngular=LandingBody->GetPhysicsAngularVelocityInRadians();
            LandingBody->SetSimulatePhysics(false);
            bLandingPaused=true;
        }
        return;
    }
    if(bLandingPaused)
    {
        LandingBody->SetSimulatePhysics(true);
        LandingBody->SetPhysicsLinearVelocity(LandingVelocity);
        LandingBody->SetPhysicsAngularVelocityInRadians(LandingAngular);
        bLandingPaused=false;
    }
    const FTransform Pose=LandingBody->GetComponentTransform();
    Flight.StopAt(Pose);
    PreviousPose=Pose;
    SetActorTransform(Pose);
    TargetPosition=Pose.GetLocation();
    AirspeedMps=LandingBody->GetPhysicsLinearVelocity().Size()*.01;
    SinkMps=-LandingBody->GetPhysicsLinearVelocity().Z*.01;
    AltitudeM=Pose.GetLocation().Z*.01;
    if(!LandingBody->IsAnyRigidBodyAwake())
    {
        bGroundPhysics=false;
        LandingBody->SetSimulatePhysics(false);
        LandingBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StopFlightAt(Pose);
    }
}
