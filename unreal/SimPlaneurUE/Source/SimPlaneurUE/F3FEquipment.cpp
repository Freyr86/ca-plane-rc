#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "ValleyAir.h"
#include "Components/LineBatchComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"

void ASlopeFlightPawn::SelectF3F()
{
    if(bF3FEnabled && !IsWalking())
    { F3FRun.Invalidate(TEXT("F3F désactivé")); bF3FEnabled=false; F3FPlacement=0; return; }
    if(!IsWalking()) { SiteNotice=TEXT("F3F : récupère le planeur pour préparer une manche."); return; }
    auto* PC=Cast<APlayerController>(GetController());
    if(PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)))
    {
        if(F3FCircuit.Valid() && FMath::Min(FVector::Dist2D(PilotPosition(),F3FCircuit.A),FVector::Dist2D(PilotPosition(),F3FCircuit.B))>400.)
        { SiteNotice=TEXT("Rejoins un pylône à moins de 4 m pour ranger le parcours."); return; }
        F3FCircuit.Installed=false; F3FCircuit.Save(GGameUserSettingsIni);
        F3FRun=FF3FRun(); bF3FEnabled=false; F3FPlacement=0;
        SiteNotice=TEXT("Parcours F3F rangé"); return;
    }
    bF3FEnabled=!bF3FEnabled;
    F3FRun.Invalidate(TEXT("F3F désactivé"));
    F3FPlacement=0;
    if(!bF3FEnabled) { SiteNotice=TEXT("F3F désactivé · parcours conservé"); return; }
    bBungeeSelected=false;
    Bungee.Release();
    if(!F3FCircuit.Valid()) { F3FDraft=FF3FCourse(); F3FPlacement=1; }
    SiteNotice=TEXT("F3F · 100 m · 10 branches · lancer à la main");
}

FString ASlopeFlightPawn::F3FPrompt() const
{
    if(!bF3FEnabled) return {};
    const FString Gates=FString::Printf(TEXT("G : portes %s"),F3FDraft.Gates?TEXT("visibles"):TEXT("masquées"));
    if(F3FPlacement==1) return TEXT("F3F · E poser la base A · ")+Gates+TEXT(" · F9 annuler");
    if(F3FPlacement==2) return FString::Printf(TEXT("F3F · B à 100 m · approche à 4 m et E · %s · F10 côté vol · F9 annuler"),*Gates);
    if(IsWalking()) return TEXT("F3F · rejoins le cercle doré, E préparer · Espace lancer · Maj+F9 près d’un pylône : ranger");
    return {};
}

void ASlopeFlightPawn::InteractF3F()
{
    if(!IsWalking() || !bHeldForLaunch || IsThrowing()) return;
    if(F3FPlacement)
    {
        if(!bF3FPlacementValid) { SiteNotice=TEXT("F3F : sol ferme et dégagé requis, à moins de 4 m du repère."); return; }
        if(F3FPlacement==1)
        {
            F3FDraft.A=F3FMarker; F3FPlacement=2;
            const double Bearing=ValleyAir::Settings().WindFromDegrees;
            F3FDraft.FlyingSide=FRotator(0,Bearing,0).Vector();
        }
        else
        {
            F3FDraft.B=F3FMarker; F3FDraft.Installed=true;
            if(!F3FDraft.Valid()) return;
            F3FCircuit=F3FDraft; F3FCircuit.Save(GGameUserSettingsIni); F3FPlacement=0;
            F3FRun=FF3FRun();
            SiteNotice=TEXT("Parcours enregistré · rejoins le cercle doré au centre, E pour préparer");
        }
        return;
    }
    if(!F3FCircuit.Valid()) return;
    const FVector Mid=(F3FCircuit.A+F3FCircuit.B)*.5;
    if(FVector::Dist2D(PilotPosition(),Mid)>1500.)
    { SiteNotice=TEXT("Rejoins la zone de lancer au milieu du parcours (15 m)."); return; }
    if(Flight.Loading().MassGrams>5000)
    { SiteNotice=TEXT("F3F : masse maximale 5 kg. Le VJX est adapté à cette discipline."); return; }
    if(bInBackpack) ToggleBackpack();
    Bungee.Release();
    F3FRun.Arm();
    SiteNotice=TEXT("Départ autorisé · lance dans les 30 s, puis entre par A vers B sous 30 s");
}

void ASlopeFlightPawn::UpdateF3F(float Dt)
{
    F3FLines->Flush();
    if(F3FAudio && (bMenuOpen || FPlatformTime::Seconds()>F3FBeepUntil)) F3FAudio->Stop();
    if(bMenuOpen || bNoclip) return;
    auto* PC=Cast<APlayerController>(GetController());
    if(!PC) return;
    const FLinearColor Gold(1.f,.69f,.26f), Blue(.25f,.8f,.94f), Red(.95f,.25f,.15f);
    auto Line=[&](FVector A,FVector B,FLinearColor C,float W=2.f) { F3FLines->DrawLine(A,B,C,0,W,0.f); };
    if(bF3FEnabled && F3FPlacement && IsWalking())
    {
        if(PC->WasInputKeyJustPressed(EKeys::G)) F3FDraft.Gates=!F3FDraft.Gates;
        FVector Candidate=PilotPosition()+FRotator(0,LaunchHeading,0).Vector()*200.;
        if(F3FPlacement==2)
        {
            FVector Axis=(PilotPosition()-F3FDraft.A).GetSafeNormal2D();
            if(FVector::Dist2D(PilotPosition(),F3FDraft.A)<300.) Axis=FRotator(0,LaunchHeading,0).Vector();
            Candidate=F3FDraft.A+Axis*10000.;
            FVector Side(-Axis.Y,Axis.X,0);
            if(FVector::DotProduct(Side,F3FDraft.FlyingSide)<0.) Side=-Side;
            if(PC->WasInputKeyJustPressed(EKeys::F10)) Side=-Side;
            F3FDraft.FlyingSide=Side;
        }
        FHitResult Ground;
        bF3FPlacementValid=FindPilotGround(Candidate,Ground) && Ground.ImpactNormal.Z>.6
            && FVector::Dist2D(PilotPosition(),Candidate)<=400.;
        F3FMarker=Ground.bBlockingHit?Ground.ImpactPoint:Candidate;
        if(F3FPlacement==2) F3FDraft.B=F3FMarker;
        const FLinearColor Colour=bF3FPlacementValid?Gold:Red;
        for(int I=0;I<24;++I)
        {
            const double A=I*UE_TWO_PI/24., B=(I+1)*UE_TWO_PI/24.;
            Line(F3FMarker+FVector(80*FMath::Cos(A),80*FMath::Sin(A),10),F3FMarker+FVector(80*FMath::Cos(B),80*FMath::Sin(B),10),Colour);
        }
    }
    if(bF3FEnabled && PC->WasInputKeyJustPressed(EKeys::E)) InteractF3F();
    if(bF3FEnabled && F3FRun.State==FF3FRun::EState::Ready)
    {
        F3FRun.Now+=FMath::Clamp(double(Dt),0.,.25);
        if(F3FRun.Now>30.) F3FRun.Invalidate(TEXT("Lancer non effectué dans les 30 s"));
    }
    const FF3FCourse& C=F3FPlacement==2?F3FDraft:F3FCircuit;
    if(!C.Installed && F3FPlacement!=2) return;
    const FVector Side=C.FlyingSide;
    for(int I=0;I<2;++I)
    {
        const FVector P=I?C.B:C.A;
        const FLinearColor Colour=I?Blue:Gold;
        // Solid-looking flag strips remain present even with virtual gates off.
        Line(P,P+FVector(0,0,430),FLinearColor(.85f,.86f,.8f),5);
        for(int K=0;K<14;++K)
            Line(P+FVector(0,0,420-K*7),P+Side*(120-K*3)+FVector(0,0,420-K*7),Colour,6);
        if((bF3FEnabled || F3FPlacement==2) && C.Gates)
        {
            // A finite visual grid represents an infinite judging plane. Both
            // gates use the same horizontal normal, irrespective of ground slope.
            for(int J=0;J<=4;++J)
            {
                const FVector Base=P+Side*(J*3000.);
                Line(Base-FVector(0,0,5000),Base+FVector(0,0,15000),Colour,2.f);
                Line(P+FVector(0,0,-5000+J*5000),P+Side*12000+FVector(0,0,-5000+J*5000),Colour,2.f);
            }
        }
    }
    if(bF3FEnabled)
    {
        // A small navigation ring locates the centre; the existing 15 m launch
        // allowance is unchanged. Keep it independent of the virtual gates.
        const FVector Centre=(C.A+C.B)*.5;
        if(IsWalking() && FVector::DistSquared2D(PilotPosition(),Centre)<FMath::Square(20000.))
        {
            // Project once per placement onto collision surfaces, including
            // terraces/rocks and the actual terrain triangles on steep slopes.
            if(F3FCentreRing.IsEmpty() || !F3FCentreCache.Equals(Centre,1.))
            {
                F3FCentreCache=Centre;
                FHitResult Ground;
                F3FCentreGround=FindPilotGround(Centre,Ground)?Ground.ImpactPoint:
                    FVector(Centre.X,Centre.Y,ASlopeWorld::TerrainHeightCm(Centre.X,Centre.Y));
                F3FCentreRing.Reset(48);
                for(int I=0;I<48;++I)
                {
                    const double Angle=I*UE_TWO_PI/48.;
                    FVector P=Centre+FVector(300.*FMath::Cos(Angle),300.*FMath::Sin(Angle),0);
                    if(FindPilotGround(P,Ground)) P=Ground.ImpactPoint;
                    else P.Z=ASlopeWorld::TerrainHeightCm(P.X,P.Y);
                    F3FCentreRing.Add(P+FVector(0,0,15));
                }
            }
            // Foreground keeps grass from hiding the navigation aid. A world
            // trace first prevents it showing through a hill or building.
            FHitResult Occlusion;
            const bool Visible=!GetWorld()->LineTraceSingleByChannel(Occlusion,
                PilotPosition()+FVector(0,0,170),F3FCentreGround+FVector(0,0,60),SceneContact::Channel,
                FCollisionQueryParams(SCENE_QUERY_STAT(F3FCentreVisibility),false,this));
            if(Visible) for(int I=0;I<F3FCentreRing.Num();++I)
                F3FLines->DrawLine(F3FCentreRing[I],F3FCentreRing[(I+1)%F3FCentreRing.Num()],Gold,SDPG_Foreground,3.f,0.f);
        }
        // Safety line follows the terrain while its judging plane stays vertical.
        for(int I=0;I<50;++I)
        {
            FVector P=FMath::Lerp(C.A,C.B,I/50.), Q=FMath::Lerp(C.A,C.B,(I+.65)/50.);
            P.Z=ASlopeWorld::TerrainHeightCm(P.X,P.Y)+12.; Q.Z=ASlopeWorld::TerrainHeightCm(Q.X,Q.Y)+12.;
            Line(P,Q,Red,3);
        }
        if(F3FPlacement==2)
        {
            const FVector Mid=(C.A+C.B)*.5;
            Line(Mid+FVector(0,0,300),Mid+Side*900+FVector(0,0,300),Gold,3);
            Line(Mid+Side*900+FVector(0,0,300),Mid+Side*700+C.Axis()*130+FVector(0,0,300),Gold,3);
            Line(Mid+Side*900+FVector(0,0,300),Mid+Side*700-C.Axis()*130+FVector(0,0,300),Gold,3);
        }
    }
}
