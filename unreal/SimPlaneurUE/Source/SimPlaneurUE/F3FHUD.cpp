#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ASimPlaneurHUD::DrawF3F(const ASlopeFlightPawn& Plane)
{
    if(!Plane.F3FEnabled()) return;
    const auto& R=Plane.F3FStatus();
    using S=FF3FRun::EState;
    const float Scale=FMath::Clamp(Canvas->SizeX/1400.f,.75f,1.15f);
    const float X=18*Scale,Y=90*Scale,W=275*Scale;
    const FLinearColor Gold(1,.71f,.29f),Cream(.95f,.92f,.8f),Blue(.3f,.8f,1.f);
    DrawRect(FLinearColor(.025f,.12f,.14f,.88f),X,Y,W,162*Scale);
    DrawRect(Gold,X,Y,3*Scale,162*Scale);
    auto Text=[&](const FString& T,float DY,FLinearColor C,float Size=1.f)
    { DrawText(T,C,X+14*Scale,Y+DY*Scale,GEngine->GetSmallFont(),Scale*Size); };
    Text(TEXT("F3F  /  ENTRAÎNEMENT"),10,Gold);
    FString Status;
    switch(R.State)
    {
    case S::Idle: Status=TEXT("POSE / PRÉPARATION"); break;
    case S::Ready: Status=FString::Printf(TEXT("LANCER SOUS %.0f s"),FMath::Max(0.,30.-R.Now)); break;
    case S::Entry: Status=R.Late?TEXT("ENTRÉE TARDIVE · CHRONO LANCÉ"):FString::Printf(TEXT("ENTRER A → B · %.0f s"),FMath::Max(0.,30.-(R.Now-R.LaunchTime))); break;
    case S::Racing: Status=FString::Printf(TEXT("BRANCHES %02d / 10  ·  BASE %s"),R.Legs,R.Legs%2?TEXT("A"):TEXT("B")); break;
    case S::Clearance: Status=TEXT("REMONTÉE À LA HAUTEUR DU DÉPART"); break;
    case S::Finished: Status=TEXT("10 / 10  ·  TEMPS FINAL"); break;
    case S::Invalid: Status=TEXT("MANCHE NON VALIDÉE"); break;
    }
    Text(Status,34,R.State==S::Invalid?FLinearColor(1,.4f,.25f):Cream,.9f);
    Text(FString::Printf(TEXT("%06.2f s"),R.Time()),55,Cream,2.1f);
    for(int I=0;I<10;++I) DrawRect(I<R.Legs?Gold:FLinearColor(.15f,.30f,.32f),X+(14+I*24)*Scale,Y+94*Scale,19*Scale,4*Scale);
    Text(R.Reason.IsEmpty()?FString::Printf(TEXT("100 m · pénalités sécurité : %d pts"),R.SafetyCrossings*100):R.Reason,108,Gold,.78f);
    if(R.WeatherRetry) Text(TEXT("MÉTÉO HORS LIMITES · revol possible"),130,Blue,.82f);
    else if(!R.Splits.IsEmpty()) Text(FString::Printf(TEXT("Dernière branche : %.2f s"),R.Splits.Last()-(R.Splits.Num()>1?R.Splits[R.Splits.Num()-2]:0.)),130,Blue,.85f);
    else Text(TEXT("E préparer · F9 activer / quitter"),130,Blue,.85f);
    // World labels distinguish bases without imposing an artificial height gate.
    const auto& C=Plane.F3FCourse();
    if(C.Valid()) for(int I=0;I<2;++I)
    {
        const FVector P=Project((I?C.B:C.A)+FVector(0,0,470));
        if(P.Z>0 && P.X>0 && P.X<Canvas->SizeX && P.Y>0 && P.Y<Canvas->SizeY-150)
            DrawText(I?TEXT("B"):TEXT("A"),I?Blue:Gold,P.X,P.Y,GEngine->GetMediumFont(),Scale);
    }
}
