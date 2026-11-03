#include "SlopeWorld.h"
#include "Airfield.h"
#include "SceneContact.h"
#include "ValleyAir.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

void ASlopeWorld::BuildLaunchSite()
{
    UStaticMesh* Board=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    UStaticMesh* Tube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (!ensure(Board && Tube && Base)) return;
    auto Paint=[&](FLinearColor Color)
    {
        auto* Material=UMaterialInstanceDynamic::Create(Base,this);
        Material->SetVectorParameterValue(TEXT("Color"),Color);
        return Material;
    };
    auto* Metal=Paint(FLinearColor(.32,.36,.37));
    auto* Frame=Paint(FLinearColor(.075,.042,.022));
    UMaterialInterface* Timber[]={Paint({.32,.155,.063}),Paint({.27,.119,.043}),Paint({.36,.18,.081})};
    auto Part=[&](UStaticMesh* Mesh,FVector Centre,FVector Size,UMaterialInterface* Material,FName Collision)
    {
        auto* Component=NewObject<UStaticMeshComponent>(this);
        Component->SetupAttachment(GetRootComponent());
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeLocation(Centre);
        Component->SetRelativeScale3D(Size/100.);
        Component->SetMaterial(0,Material);
        if (Collision.IsNone()) Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        else SceneContact::Solid(Component,Collision);
        Component->RegisterComponent();
        return Component;
    };

    // Seat kept level; each post reaches the actual hillside independently.
    const float X=PilotX+445, Y=PilotY-195;
    // Face downhill, with the long boards running along the contour.
    const FVector Forward=FVector(TerrainHeightCm(X-10,Y)-TerrainHeightCm(X+10,Y),
        TerrainHeightCm(X,Y-10)-TerrainHeightCm(X,Y+10),0).GetSafeNormal();
    const FRotator Bearing=Forward.Rotation();
    const FVector Right=FRotationMatrix(Bearing).GetUnitAxis(EAxis::Y);
    auto BenchPoint=[&](double Depth,double Width,double Height)
    {
        return FVector(X,Y,Height)+Forward*Depth+Right*Width;
    };
    auto Plank=[&](FVector Centre,FVector Size,UMaterialInterface* Material)
    {
        Part(Board,Centre,Size,Material,TEXT("bench"))->SetRelativeRotation(Bearing);
    };
    const float Seat=TerrainHeightCm(X,Y)+48;
    for (int Side : {-1,1})
    {
        for (int Rear : {-1,1})
        {
            FVector Leg=BenchPoint(Rear*18,Side*72,0);
            const float Foot=TerrainHeightCm(Leg.X,Leg.Y)-6, Top=Rear<0 ? Seat+51 : Seat-4;
            Leg.Z=(Foot+Top)*.5;
            Plank(Leg,{7,8,Top-Foot},Frame);
        }
        Plank(BenchPoint(0,Side*72,Seat-5),{48,8,7},Frame);
    }
    for (int I=0;I<4;++I)
        Plank(BenchPoint(-18+I*12,0,Seat),{11,185,4},Timber[I%3]);
    for (int I=0;I<3;++I)
        Plank(BenchPoint(-12,0,Seat+22+I*12),{4,185,10},Timber[(I+1)%3]);

    auto AddWindsock=[&](float MastX,float MastY)
    {
        // Anchor to the rendered terrain triangles, especially on the chalet's 25 m grid.
        const FTransform& TerrainToWorld=Terrain->GetComponentTransform();
        const FVector Above=TerrainToWorld.TransformPosition({MastX,MastY,1000000.});
        const FVector Below=TerrainToWorld.TransformPosition({MastX,MastY,-1000000.});
        FHitResult Hit;
        if (!ensureMsgf(Terrain->LineTraceComponent(Hit,Above,Below,FCollisionQueryParams(SCENE_QUERY_STAT(WindsockFoot),true)),
            TEXT("Missing terrain under windsock at %.0f, %.0f"),MastX,MastY)) return;
        const double Ground=TerrainToWorld.InverseTransformPosition(Hit.ImpactPoint).Z;
        const FVector Normal=TerrainToWorld.InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
        const FVector Pivot=FVector(MastX,MastY,Ground+370);
        Part(Tube,{MastX,MastY,Ground+182},{6,6,376},Metal,TEXT("mast"));
        Part(Tube,FVector(MastX,MastY,Ground)+Normal*4,{25,25,12},Metal,TEXT("mast"))
            ->SetRelativeRotation(FRotationMatrix::MakeFromZ(Normal).Rotator());

        auto* Windsock=NewObject<UProceduralMeshComponent>(this);
        Windsock->SetupAttachment(GetRootComponent());
        Windsock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Windsock->SetCanEverAffectNavigation(false);
        Windsock->RegisterComponent();
        auto* Orange=Paint({.97,.105,.014});
        auto* Ivory=Paint({.94,.91,.80});
        constexpr int32 Sides=24;
        for (int Band=0;Band<5;++Band)
        {
            TArray<FVector> Points,Normals;
            TArray<int32> Faces;
            for (int End=0;End<2;++End)
            {
                const double T=(Band+End)/5., Radius=17.-10.*T;
                for (int J=0;J<Sides;++J)
                {
                    const double Angle=2.*PI*J/Sides;
                    const FVector Out(0,FMath::Cos(Angle),FMath::Sin(Angle));
                    Points.Add(FVector(140*T,0,-10*T*T)+Radius*Out);
                    Normals.Add((Out+FVector(.07,0,0)).GetSafeNormal());
                }
            }
            for (int A=0;A<Sides;++A)
            {
                const int B=(A+1)%Sides, C=A+Sides, D=B+Sides;
                Faces.Append({A,C,B,B,C,D,A,B,C,B,D,C}); // Open tube visible from both sides.
            }
            Windsock->CreateMeshSection_LinearColor(Band,Points,Faces,Normals,{},{},{},false);
            Windsock->SetMaterial(Band,Band%2 ? Ivory : Orange);
        }
        SiteWindsocks.Add({Windsock,Pivot});
    };
    AddWindsock(PilotX+760,PilotY+270);
    AddWindsock(ChaletX+950,ChaletY+450);
    // A compact timber equipment hut. The door and front window face the flight line.
    const float Floor=TerrainHeightCm(Airfield::Hut.X,Airfield::Hut.Y);
    auto* Boards=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Meadow/Lakeside/M_CabinBoards"));
    auto* Roof=Paint({.073,.107,.104});
    auto* Glass=Paint({.058,.17,.20});
    auto HutPart=[&](double Along,double Front,double Z,FVector Size,UMaterialInterface* Material,double Pitch=0.)
    {
        const FVector2D P=Airfield::Point(Along,Airfield::HutSide-Front);
        Part(Board,FVector(P,Floor+Z),Size,Material,TEXT("hut"))->SetRelativeRotation(FRotator(Pitch,45.,0.));
    };
    HutPart(0,0,3,{312,440,16},Frame);
    HutPart(0,0,116,{300,430,214},Boards ? Boards : Timber[0]);
    // Shed roof rises toward the door; close the wedge beneath it with timber courses.
    for(int I=0;I<7;++I)
        HutPart(0,11.+21.*I,226.+6.*I,{278.-42.*I,430,6},Boards ? Boards : Timber[0]);
    HutPart(0,0,249,{352,474,12},Roof,8.);
    HutPart(-107,151,107,{9,101,200},Frame);
    HutPart(-107,157,105,{5,87,190},Timber[1]);
    HutPart(-77,162,100,{5,5,12},Metal);
    HutPart(70,152,146,{10,133,90},Frame);
    HutPart(70,158,146,{4,121,78},Glass);
    HutPart(70,161,146,{4,5,78},Timber[2]);
    HutPart(70,161,105,{15,144,6},Timber[2]);
    HutPart(-107,175,4,{47,120,9},Frame);
    AddWindsock(Airfield::Sock.X,Airfield::Sock.Y);
    UpdateWindsock(0,0);
}

void ASlopeWorld::UpdateWindsock(float Dt,double Time)
{
    for (FSiteWindsock& Sock : SiteWindsocks)
    {
        auto* Windsock=Sock.Mesh.Get();
        if (!Windsock) continue;
        const FVector SamplePoint(Sock.Pivot.X*.01,Sock.Pivot.Y*.01,-Sock.Pivot.Z*.01);
        const FVector Air=ValleyAir::Velocity(SamplePoint,Time);
        const FVector Target(Air.X,Air.Y,-Air.Z);
        Sock.Flow=Sock.Initialized ? FMath::Lerp(Sock.Flow,Target,1.-FMath::Exp(-3.*Dt)) : Target;
        Sock.Initialized=true;
        const double Speed=Sock.Flow.Size2D();
        FRotator Facing=Windsock->GetRelativeRotation();
        if (Speed>.08) Facing.Yaw=FMath::RadiansToDegrees(FMath::Atan2(Sock.Flow.Y,Sock.Flow.X));
        const double Fill=FMath::Clamp(Speed/4.5,0.,1.);
        Facing.Pitch=FMath::Lerp(-84.,FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Sock.Flow.Z,Speed)),-18.,24.),FMath::SmoothStep(0.,1.,Fill));
        Facing.Pitch+=Fill*1.5*FMath::Sin(Time*4.4);
        // Mouth touches the swivel at mast head; tail lies downwind.
        Windsock->SetRelativeLocationAndRotation(Sock.Pivot+FRotator(0,Facing.Yaw,0).Vector()*14.,Facing);
    }
}
