#include "PilotMovement.h"
#include "SceneContact.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

bool PilotMovement::ResolveStep(UWorld* World,const AActor* Pilot,const FVector& Start,const FVector& Ground,FVector& Result)
{
    if(FMath::Abs(Ground.Z-Start.Z)>MaxStep) return false;
    // Preserve the existing torso capsule and its clearance for feet on slopes.
    const FVector BodyOffset(0,0,95);
    const auto Shape=FCollisionShape::MakeCapsule(24,70);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(PilotStep),false,Pilot);
    auto Sweep=[&](const FVector& A,const FVector& B,FHitResult& Hit)
    { return World->SweepSingleByChannel(Hit,A,B,FQuat::Identity,SceneContact::Channel,Shape,Query); };
    const FVector From=Start+BodyOffset, To=Ground+BodyOffset;
    FHitResult Hit;
    if(!Sweep(From,To,Hit)) {Result=Ground; return true;}
    // A diagonal sweep can catch the front or rear edge of a small step.
    // Clear it vertically, move across, then settle down onto safe support.
    // Sweep every leg: this never bypasses a wall or a low ceiling.
    FVector Raised=From+FVector(0,0,MaxStep);
    if(Sweep(From,Raised,Hit))
    {
        if(Hit.bStartPenetrating) return false;
        Raised=FMath::Lerp(From,Raised,FMath::Max(0.f,Hit.Time-.01f));
    }
    const FVector Across(Ground.X,Ground.Y,Raised.Z);
    if(Sweep(Raised,Across,Hit)) return false;
    if(Sweep(Across,To,Hit))
    {
        if(Hit.bStartPenetrating || Hit.ImpactNormal.Z<.55 || Hit.ImpactPoint.Z-Start.Z>MaxStep
            || (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("water")))) return false;
        Result=Hit.Location-BodyOffset+FVector(0,0,.5);
        // A side support may prevent the full descent this frame. Advance
        // horizontally at its supported height; the following step clears it.
        if(Result.Z<Ground.Z-.1 || FMath::Abs(Result.Z-Start.Z)>MaxStep) return false;
    }
    else Result=Ground;
    return !World->OverlapBlockingTestByChannel(Result+BodyOffset,FQuat::Identity,SceneContact::Channel,Shape,Query);
}
