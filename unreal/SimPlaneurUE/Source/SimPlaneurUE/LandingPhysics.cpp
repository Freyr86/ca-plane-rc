#include "SlopeFlightPawn.h"
#include "LandingDamage.h"
#include "ProceduralMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Misc/ConfigCacheIni.h"

void ASlopeFlightPawn::SetResistanceMode(EAircraftResistance Mode)
{
    Resistance=static_cast<EAircraftResistance>(FMath::Clamp(static_cast<int32>(Mode),0,2));
    GConfig->SetInt(TEXT("SimPlaneur.Physics"),TEXT("ResistanceMode"),static_cast<int32>(Resistance),GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

void ASlopeFlightPawn::ClearGroundPhysics()
{
    bGroundPhysics=false; bLandingPaused=false; bLandingLookReleased=false; ImpactDamage=0;
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
    const double ImpactSpeed=LandingContactSpeed(LandingVelocity,LandingAngular,Hit.ImpactPoint-Pose.GetLocation(),Hit.ImpactNormal);
    ImpactDamage=LandingDamage(ImpactSpeed/Flight.ImpactTolerance(),ContactKind==TEXT("water"),ContactKind!=TEXT("ground"),
        Pose.GetRotation().GetUpVector().Z<.3,Resistance);
    StopFlightAt(Pose,true); // Disable aerodynamics; keep following Chaos motion.
    bLandingLookReleased=false;
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

void ASlopeFlightPawn::LandingHit(UPrimitiveComponent*,AActor*,UPrimitiveComponent* Other,FVector Impulse,const FHitResult& Hit)
{
    if(!bGroundPhysics || bLandingPaused) return;
    // Chaos reports a resolved impulse: J/m also includes rebound and support
    // impulses. Use the pre-physics approach velocity, as at first touchdown.
    const double ImpactSpeed=LandingContactSpeed(LandingVelocity,LandingAngular,
        Hit.ImpactPoint-LandingBody->GetCenterOfMass(),Hit.ImpactNormal);
    const int32 NewDamage=LandingDamage(ImpactSpeed/Flight.ImpactTolerance(),Other && Other->ComponentHasTag(TEXT("water")),
        false,false,Resistance);
    if(NewDamage>ImpactDamage)
        UE_LOG(LogTemp,Display,TEXT("LANDING_IMPACT approach=%.3f impulse_delta=%.3f damage=%d component=%s"),
            ImpactSpeed,Impulse.Size()/FMath::Max(.05,Flight.Loading().MassGrams*.001)*.01,NewDamage,*GetNameSafe(Other));
    ImpactDamage=FMath::Max(ImpactDamage,NewDamage);
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
    // Pawn ticks before physics. Hit callbacks read these velocities after the
    // solver, so a bounce cannot be mistaken for a harder incoming collision.
    LandingVelocity=LandingBody->GetPhysicsLinearVelocity();
    LandingAngular=LandingBody->GetPhysicsAngularVelocityInRadians();
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
        // Only release the camera once Chaos has actually put the body to
        // sleep. A pilot who already chose to walk keeps their manual view.
        StopFlightAt(Pose,bLandingLookReleased);
        bLandingLookReleased=false;
    }
}
