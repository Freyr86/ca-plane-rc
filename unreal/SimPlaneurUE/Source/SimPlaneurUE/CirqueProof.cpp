#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "CirqueLandscape.h"
#include "CirqueSite.h"
#include "CirqueWall.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ASlopeFlightPawn::TestCirque(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    // Leave several rendered frames after each camera cut for temporal rendering,
    // grass planting and texture streaming before taking the following capture.
    if(!bAppearanceReady || Elapsed<5.f+MenuProofStep*6.f) return;
    bool OK=true;
    auto View=[this](const FVector& Position,const FVector& Target)
    {
        bMenuOpen=false; bNoclip=true; bHeldForLaunch=true; bInBackpack=true;
        bShowWindTrails=false;
        PlaneVisual->SetHiddenInGame(true,true);
        NoclipPosition=Position; NoclipRotation=(Target-Position).Rotation();
        if(auto* PC=Cast<APlayerController>(GetController()))
            if(auto* HUD=PC->GetHUD()) HUD->bShowHUD=false;
    };
    auto Capture=[](const TCHAR* Name)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false,false);
    };
    auto MainPassage=[]() -> const CirqueWall::FPassage*
    {
        const CirqueWall::FPassage* Best=nullptr;
        for(const auto& Passage:CirqueWall::Passages())
            if(!Best || FVector2D::DistSquared(Passage.Centre,CirqueSite::Pilot)
                <FVector2D::DistSquared(Best->Centre,CirqueSite::Pilot)) Best=&Passage;
        return Best;
    };
    switch(MenuProofStep)
    {
    case 0:
    {
        View({185000.,20000.,70000.},{Cirque::CenterX,Cirque::CenterY,18000.});
        // Trace only the actual terrain component: scenery collision is checked
        // independently and must not obscure comparison with the heightfield.
        UPrimitiveComponent* Terrain=nullptr;
        UPrimitiveComponent* CoarseTerrain=nullptr;
        for(TActorIterator<ASlopeWorld> Site(GetWorld());Site;++Site)
        {
            CoarseTerrain=Cast<UPrimitiveComponent>(Site->GetRootComponent());
            const auto Local=Site->GetComponentsByTag(UPrimitiveComponent::StaticClass(),TEXT("CirqueTerrain"));
            if(!Local.IsEmpty()) Terrain=Cast<UPrimitiveComponent>(Local[0]);
            if(Terrain) break;
        }
        OK=Terrain!=nullptr;
        double LargestError=0.;
        int Samples=0;
        if(Terrain)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(CirqueGroundProof),true,this);
            // Sample the basin, scree, near-vertical wall and summit, including
            // off-grid coordinates on both triangles of the local heightfield.
            for(float A:{-20.f,30.f,80.f,130.f})
                for(float R:{20000.f,45000.f,56000.f,62500.f,65000.f,67500.f,76000.f})
                    for(float Shift:{0.f,157.f})
                    {
                        const float Angle=FMath::DegreesToRadians(A);
                        const float X=Cirque::CenterX+FMath::Cos(Angle)*R+Shift;
                        const float Y=Cirque::CenterY+FMath::Sin(Angle)*R-Shift*.37f;
                        const float Step=ASlopeWorld::TerrainGridSpacing(X,Y);
                        const float X0=FMath::FloorToFloat(X/Step)*Step,Y0=FMath::FloorToFloat(Y/Step)*Step;
                        const float U=(X-X0)/Step,V=(Y-Y0)/Step;
                        const float B=ASlopeWorld::TerrainVertexHeightCm(X0,Y0+Step);
                        const float C=ASlopeWorld::TerrainVertexHeightCm(X0+Step,Y0);
                        const bool Lower=U+V<=1.f;
                        const float Corner=Lower?ASlopeWorld::TerrainVertexHeightCm(X0,Y0):ASlopeWorld::TerrainVertexHeightCm(X0+Step,Y0+Step);
                        const float Dx=Lower?C-Corner:Corner-B,Dy=Lower?B-Corner:Corner-C;
                        const float Expected=Lower?Corner+Dx*U+Dy*V:Corner+Dx*(U-1)+Dy*(V-1);
                        FHitResult Hit;
                        const bool Found=Terrain->LineTraceComponent(Hit,{X,Y,150000.},{X,Y,-100000.},Query);
                        const double Error=Found?FMath::Abs(Hit.ImpactPoint.Z-Expected):1.e6;
                        LargestError=FMath::Max(LargestError,Error);
                        OK &= Found && Error<2.;
                        ++Samples;
                    }
        }
        UE_LOG(LogTemp,Display,TEXT("CIRQUE_PROOF terrain samples=%d max_error_cm=%.3f success=%d"),Samples,LargestError,OK);
        // Compare the independent collision components along all four joins.
        // Trace just inside each side, then extrapolate each hit triangle back
        // by 2 cm to the common edge; natural slope does not inflate the error.
        double LargestJoinError=0.; int JoinSamples=0;
        OK &= CoarseTerrain!=nullptr;
        if(Terrain && CoarseTerrain)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(CirqueJoinProof),true,this);
            const FVector2D Low(155000.,47500.),High(380000.,272500.);
            for(int Edge=0;Edge<4;++Edge)
                for(int I=1;I<89;I+=11)
                {
                    const double Along=(I+.37)*2500.;
                    const FVector2D P=Edge<2?FVector2D(Edge==0?Low.X:High.X,Low.Y+Along)
                        :FVector2D(Low.X+Along,Edge==2?Low.Y:High.Y);
                    const FVector2D Inward=Edge<2?FVector2D(Edge==0?1.:-1.,0.)
                        :FVector2D(0.,Edge==2?1.:-1.);
                    const FVector2D Fine=P+Inward*2.,Coarse=P-Inward*2.;
                    FHitResult A,B;
                    const bool FineHit=Terrain->LineTraceComponent(A,{Fine.X,Fine.Y,150000.},{Fine.X,Fine.Y,-100000.},Query);
                    const bool CoarseHit=CoarseTerrain->LineTraceComponent(B,{Coarse.X,Coarse.Y,150000.},{Coarse.X,Coarse.Y,-100000.},Query);
                    auto EdgeHeight=[&P](const FHitResult& Hit)
                    {
                        return Hit.ImpactPoint.Z-(Hit.ImpactNormal.X*(P.X-Hit.ImpactPoint.X)
                            +Hit.ImpactNormal.Y*(P.Y-Hit.ImpactPoint.Y))/Hit.ImpactNormal.Z;
                    };
                    const bool Valid=FineHit && CoarseHit && A.ImpactNormal.Z>.01 && B.ImpactNormal.Z>.01;
                    const double Error=Valid?FMath::Abs(EdgeHeight(A)-EdgeHeight(B)):1.e6;
                    LargestJoinError=FMath::Max(LargestJoinError,Error);
                    OK &= Valid && Error<2.;
                    ++JoinSamples;
                }
        }
        UE_LOG(LogTemp,Display,TEXT("CIRQUE_PROOF joins samples=%d max_error_cm=%.3f success=%d"),JoinSamples,LargestJoinError,OK);
        break;
    }
    case 1:
        Capture(TEXT("cirque-01-ensemble.png"));
        break;
    case 2:
        View({Cirque::CenterX,Cirque::CenterY,195000.},{Cirque::CenterX,Cirque::CenterY,11000.});
        // Keep north (+X) at the top of the overhead view.
        NoclipRotation=FRotator(-90.,0.,0.);
        break;
    case 3:
        Capture(TEXT("cirque-02-dessus.png"));
        break;
    case 4:
    {
        const FVector Rim(306000.,226000.,ASlopeWorld::TerrainHeightCm(306000.f,226000.f)+450.);
        View(Rim,{257000.,141000.,12000.});
        break;
    }
    case 5:
        Capture(TEXT("cirque-03-belvedere.png"));
        break;
    case 6:
        View({245000.,85000.,24000.},{294000.,204000.,22000.});
        break;
    case 7:
        Capture(TEXT("cirque-04-falaise.png"));
        break;
    case 8:
    {
        // The exploration camera must still respect the new basin collision.
        NoclipPosition={Cirque::CenterX,Cirque::CenterY,-100000.};
        KeepNoclipAboveGround();
        FHitResult Hit;
        OK=FindPilotGround(NoclipPosition,Hit,true)
            && NoclipPosition.Z>=Hit.ImpactPoint.Z+84.;
        break;
    }
    case 9:
    {
        // Exercise the same site selector used by the menu, including its
        // ground trace and launch bearing, before inspecting the furniture.
        bNoclip=false; bMenuOpen=true;
        SwitchLaunchSite(4-SelectedSite);
        OK=SelectedSite==4 && FVector::Dist2D(PilotPosition(),FVector(CirqueSite::Pilot,0))<1.
            && FMath::Abs(FRotator::NormalizeAxis(LaunchHeading-CirqueSite::Heading))<.1;
        const FVector Ground=CirqueSite::Ground(CirqueSite::Pilot.X,CirqueSite::Pilot.Y);
        OK &= FMath::Abs(PilotPosition().Z-Ground.Z)<2.;
        ASlopeWorld* WorldSite=nullptr;
        for(TActorIterator<ASlopeWorld> It(GetWorld());It;++It) {WorldSite=*It;break;}
        OK &= WorldSite!=nullptr;
        auto Top=[&](FName Tag,FVector2D P,double Low,double High)
        {
            if(!WorldSite) return false;
            const FVector Foot=CirqueSite::Ground(P.X,P.Y);
            double Highest=-BIG_NUMBER;
            const FCollisionQueryParams Query(SCENE_QUERY_STAT(CirqueFurnitureProof),true,this);
            for(auto* Item:WorldSite->GetComponentsByTag(UPrimitiveComponent::StaticClass(),Tag))
            {
                auto* Part=CastChecked<UPrimitiveComponent>(Item);
                FHitResult Hit;
                if(Part->LineTraceComponent(Hit,Foot+FVector(0,0,5000),Foot-FVector(0,0,30),Query))
                    Highest=FMath::Max(Highest,Hit.ImpactPoint.Z-Foot.Z);
            }
            return Highest>=Low && Highest<=High;
        };
        // Pick the middle of a seat plank, not the intentional gap between them.
        const bool Left=Top(TEXT("CirqueBench"),CirqueSite::BenchLeft+CirqueSite::Right*6.,42,85);
        const bool Right=Top(TEXT("CirqueBench"),CirqueSite::BenchRight-CirqueSite::Right*6.,42,85);
        const bool Table=Top(TEXT("CirqueTable"),CirqueSite::Table,70,120);
        const bool Mast=Top(TEXT("CirqueWindsockMast"),CirqueSite::Sock,350,410);
        bool Sock=false,Anchored=false;
        if(WorldSite)
        {
            const FVector Foot=CirqueSite::Ground(CirqueSite::Sock.X,CirqueSite::Sock.Y);
            double Bottom=BIG_NUMBER;
            for(auto* Item:WorldSite->GetComponentsByTag(UPrimitiveComponent::StaticClass(),TEXT("CirqueWindsockMast")))
                Bottom=FMath::Min(Bottom,CastChecked<UPrimitiveComponent>(Item)->Bounds.GetBox().Min.Z);
            Anchored=FMath::Abs(Bottom-Foot.Z)<15.;
            const auto Socks=WorldSite->GetComponentsByTag(UPrimitiveComponent::StaticClass(),TEXT("CirqueWindsock"));
            if(Socks.Num()==1)
            {
                const FVector P=CastChecked<UPrimitiveComponent>(Socks[0])->GetComponentLocation();
                Sock=FVector::Dist2D(P,Foot)<30. && FMath::Abs(P.Z-Foot.Z-370.)<2.;
            }
        }
        OK &= Left && Right && Table && Mast && Sock && Anchored;
        UE_LOG(LogTemp,Display,TEXT("CIRQUE_SITE_PROOF site=%d left_bench=%d right_bench=%d table=%d mast=%d sock=%d anchored=%d success=%d"),
            SelectedSite,Left,Right,Table,Mast,Sock,Anchored,OK);
        const FVector2D P=CirqueSite::Point(-1600,1900);
        View(CirqueSite::Ground(P.X,P.Y)+FVector(0,0,950),Ground+FVector(0,0,100));
        break;
    }
    case 10:
        Capture(TEXT("cirque-site-01-mobilier.png"));
        break;
    case 11:
    {
        const auto* Passage=MainPassage();
        OK=Passage!=nullptr;
        if(Passage)
        {
            // Stop one metre beyond the wall, still on the plateau before the
            // cliff edge: this tests the opening without asking the pilot to fall.
            auto WalkAcross=[&](FVector2D Centre,FVector2D Outward)
            {
                const FVector2D Start=Centre+Outward*250.;
                FHitResult Ground;
                if(!FindPilotGround(FVector(Start,0),Ground)) return -1.;
                bMenuOpen=false; bNoclip=false; bHeldForLaunch=true; bInBackpack=true;
                PilotSite=Ground.ImpactPoint; PilotOffset=FVector2D::ZeroVector;
                const FVector Before=PilotPosition();
                for(int I=0;I<32;++I) MovePilot(FVector(-Outward,0),.02f,false);
                return FVector::DotProduct(PilotPosition()-Before,FVector(-Outward,0));
            };
            const double GapTravel=WalkAcross(Passage->Centre,Passage->Outward);
            const CirqueWall::FStation* Solid=nullptr;
            const double Desired=Passage->Distance+900.;
            for(const auto& Station:CirqueWall::Path())
            {
                bool ByGap=false;
                for(const auto& Gap:CirqueWall::Passages())
                    ByGap |= FMath::Abs(Station.Distance-Gap.Distance)<Gap.Width*.5f+250.f;
                if(ByGap || !CirqueWall::Footprint(Station.Centre.X,Station.Centre.Y)) continue;
                if(!Solid || FMath::Abs(Station.Distance-Desired)<FMath::Abs(Solid->Distance-Desired)) Solid=&Station;
            }
            const double WallTravel=Solid?WalkAcross(Solid->Centre,Solid->Outward):-1.;
            OK &= GapTravel>330. && Solid && WallTravel>=0. && WallTravel<240.;
            UE_LOG(LogTemp,Display,TEXT("CIRQUE_SITE_PROOF walk_gap_cm=%.1f walk_wall_cm=%.1f passages=%d success=%d"),
                GapTravel,WallTravel,CirqueWall::Passages().Num(),OK);
            const FVector2D Along(-Passage->Outward.Y,Passage->Outward.X);
            const FVector2D Eye=Passage->Centre+Passage->Outward*600.+Along*1200.;
            View(CirqueSite::Ground(Eye.X,Eye.Y)+FVector(0,0,260),
                CirqueSite::Ground(Passage->Centre.X,Passage->Centre.Y)+FVector(0,0,45));
        }
        break;
    }
    case 12:
        Capture(TEXT("cirque-site-02-passage.png"));
        break;
    case 13:
    {
        const auto* Passage=MainPassage();
        OK=Passage!=nullptr;
        if(Passage)
        {
            const FVector2D P=CirqueSite::Point(-9000,-5000);
            const FVector2D Target=(CirqueSite::Pilot+Passage->Centre)*.5;
            View(CirqueSite::Ground(P.X,P.Y)+FVector(0,0,4500),
                CirqueSite::Ground(Target.X,Target.Y)+FVector(0,0,100));
        }
        break;
    }
    case 14:
        Capture(TEXT("cirque-site-03-ensemble.png"));
        break;
    default:
        UE_LOG(LogTemp,Display,TEXT("CIRQUE_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    UE_LOG(LogTemp,Display,TEXT("CIRQUE_PROOF step=%d success=%d eye=%s"),MenuProofStep,OK,*NoclipPosition.ToString());
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
