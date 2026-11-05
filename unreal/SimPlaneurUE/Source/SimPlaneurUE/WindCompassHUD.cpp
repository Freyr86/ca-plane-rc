#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "ValleyAir.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"

void ASimPlaneurHUD::DrawWindCompass(const ASlopeFlightPawn& Plane)
{
    if(!CompassFont)
    {
        // Canvas requires a UFont even when a Slate font description is supplied.
        CompassFont=NewObject<UFont>(this);
        CompassFont->FontCacheType=EFontCacheType::Runtime;
        CompassFont->GetMutableInternalCompositeFont()=*FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),12).GetCompositeFont();
    }
    FVector Eye; FRotator View;
    PlayerOwner->GetPlayerViewPoint(Eye,View);
    const FVector Wind=Plane.GetLocalWind(), Relative=Plane.GetRelativeFlow();
    const float S=InstrumentScale*FMath::Clamp(Canvas->SizeX/1280.f,.72f,1.5f);
    const FVector2D Centre(Canvas->SizeX*.5,83*S);
    // Use the menu's linear palette and runtime Slate fonts for crisp typography.
    const FLinearColor Ink(.009f,.078f,.076f), Paper(1.f,.886f,.65f),
        Highlight(1.f,.955f,.82f), Gold(.75f,.41f,.075f), Muted(.08f,.19f,.17f),
        Cyan(.0f,.38f,.46f), Coral(.85f,.142f,.077f);
    auto At=[&](FVector2D P){return Centre+P*S;};
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor Colour,float Width=1.f)
    { A=At(A); B=At(B); DrawLine(A.X,A.Y,B.X,B.Y,Colour,Width*S); };
    auto Shape=[&](const TArray<FVector2D>& Points,FLinearColor Top,FLinearColor Bottom,float YMin,float YMax)
    {
        TArray<FCanvasUVTri> Tris;
        auto Colour=[&](double Y){return FMath::Lerp(Top,Bottom,float(FMath::Clamp((Y-YMin)/FMath::Max(1.f,YMax-YMin),0.,1.)));};
        for(int I=1;I+1<Points.Num();++I)
        {
            FCanvasUVTri T;
            T.V0_Pos=At(Points[0]); T.V1_Pos=At(Points[I]); T.V2_Pos=At(Points[I+1]);
            T.V0_Color=Colour(Points[0].Y); T.V1_Color=Colour(Points[I].Y); T.V2_Color=Colour(Points[I+1].Y);
            Tris.Add(T);
        }
        FCanvasTriangleItem Item(Tris,GWhiteTexture); Item.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Item);
    };
    auto Solid=[&](const TArray<FVector2D>& P,FLinearColor Colour){Shape(P,Colour,Colour,0,1);};
    auto Outline=[&](const TArray<FVector2D>& P,FLinearColor Colour,float Width)
    {for(int I=0;I<P.Num();++I) Line(P[I],P[(I+1)%P.Num()],Colour,Width);};
    auto Circle=[&](FVector2D Pos,float Radius,FLinearColor Top,FLinearColor Bottom)
    {
        TArray<FVector2D> P;
        for(int I=0;I<128;++I) {const double A=UE_TWO_PI*I/128.; P.Add(Pos+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);}
        Shape(P,Top,Bottom,Pos.Y-Radius,Pos.Y+Radius);
    };
    auto Text=[&](const FString& Value,FVector2D Pos,int32 Size,FLinearColor Colour,FName Face=TEXT("Bold"),bool Centered=false)
    {
        FSlateFontInfo FontInfo(CompassFont,FMath::Max(4,FMath::RoundToInt(Size*S)),Face);
        FCanvasTextItem Item(At(Pos),FText::FromString(Value),FontInfo,Colour);
        Item.bCentreX=Item.bCentreY=Centered;
        Item.bDontCorrectStereoscopic=true;
        Canvas->DrawItem(Item);
    };
    auto Number=[](double Value,int Decimals=1)
    {return FString::Printf(TEXT("%.*f"),Decimals,Value).Replace(TEXT("."),TEXT(","));};

    // Wing cards: curved tips, soft depth, warm face and fine double border.
    TArray<FVector2D> Left={{-54,-32}};
    for(int I=0;I<=32;++I)
    {
        const double A=FMath::DegreesToRadians(-90.-I*180./32.);
        Left.Add({-234.+48.*FMath::Cos(A),32.*FMath::Sin(A)});
    }
    Left.Add({-54,32});
    TArray<FVector2D> Right;
    for(int I=Left.Num()-1;I>=0;--I) Right.Add({-Left[I].X,Left[I].Y});
    for(const auto* Card:{&Left,&Right})
    {
        TArray<FVector2D> Shadow=*Card;
        for(auto& P:Shadow) P.Y+=4;
        Solid(Shadow,FLinearColor(0,0,0,.17f));
        Shape(*Card,Highlight,Paper,-32,32);
        Outline(*Card,Paper,5); Outline(*Card,Ink,1.6f);
    }
    Line({-264,21},{-232,26},Gold,2); Line({264,21},{232,26},Gold,2);
    Line({-228,-21},{-228,21},Gold,1); Line({228,-21},{228,21},Gold,1);
    Line({-266,-6},{-247,-6},Cyan,2);
    for(int I=0;I<18;++I)
    {
        const double A=FMath::DegreesToRadians(90.-I*10.),B=FMath::DegreesToRadians(80.-I*10.);
        Line(FVector2D(-247,-12)+FVector2D(FMath::Cos(A),FMath::Sin(A))*6,
             FVector2D(-247,-12)+FVector2D(FMath::Cos(B),FMath::Sin(B))*6,Cyan,2);
    }
    Line({-267,2},{-241,2},Cyan,2); Line({-262,10},{-250,10},Cyan,2);
    for(int Row=0;Row<3;++Row)
        for(int I=0;I<20;++I)
        {
            const auto P=[&](int N){return FVector2D(241.+N*1.25,-9.+Row*8+FMath::Sin(N*UE_PI/12.)*2.5);};
            Line(P(I),P(I+1),Gold,1.8f);
        }
    auto Readout=[&](const TCHAR* Title,const FVector& Flow,double Speed,float X,FLinearColor Colour)
    {
        const FString Bearing=Flow.SizeSquared2D()<.0025?TEXT("—"):FString::Printf(TEXT("%03d°"),FMath::RoundToInt(ValleyAir::FromBearing(Flow))%360);
        Text(Title,{X-66,-25},10,Ink,TEXT("BoldItalic"));
        Text(Bearing,{X+40,-24},9,Colour);
        Text(Number(Speed)+TEXT(" m/s"),{X,9},17,Ink,TEXT("BoldItalic"),true);
    };
    Readout(TEXT("VENT"),Wind,Wind.Size2D(),-148,Cyan);
    Readout(TEXT("RELATIF"),Relative,Relative.Size(),148,Gold);

    for(int I=4;I>0;--I) Circle({0,3},67+I,FLinearColor(0,0,0,.025f),FLinearColor(0,0,0,.04f));
    Circle({0,0},67,Highlight,Paper);
    Circle({0,0},63.5f,Ink,Ink);
    Circle({0,0},61.5f,Gold,Gold);
    Circle({0,0},60,Highlight,Paper);
    auto Point=[&](double Bearing,double Radius){return ValleyAir::CompassDirection(Bearing,View.Yaw)*Radius;};
    for(int I=0;I<72;++I)
        Line(Point(I*5.,57),Point(I*5.,I%6==0?49:I%3==0?52:54.5),Muted,I%6==0?1.5f:.7f);
    const TCHAR* Cardinals[]={TEXT("N"),TEXT("E"),TEXT("S"),TEXT("O")};
    for(int I=0;I<4;++I) Text(Cardinals[I],Point(I*90.,40),14,I==0?Coral:Ink,TEXT("BoldItalic"),true);
    const FVector2D NorthSide=Point(90,4);
    Solid({Point(0,58),Point(0,51)+NorthSide,Point(0,51)-NorthSide},Coral);

    // Both flows come FROM the displayed heading; the rose follows camera yaw.
    auto Needle=[&](const FVector& Flow,FLinearColor Colour,double Length,double Width)
    {
        if(Flow.SizeSquared2D()<.0025) return;
        const FVector2D D=Point(ValleyAir::FromBearing(Flow),1), P(-D.Y,D.X);
        Solid({-D*4-P*1.1,D*(Length-7)-P*1.1,D*(Length-7)+P*1.1,-D*4+P*1.1},Colour);
        Solid({D*Length,D*(Length-8)+P*Width,D*(Length-8)-P*Width},Colour);
    };
    Needle(Wind,Cyan,33,4.3); Needle(Relative,Gold,25,3.4);
    const FVector2D Forward=Point(Plane.GetActorRotation().Yaw,1), Across(-Forward.Y,Forward.X);
    auto Icon=[&](double X,double Y){return Across*X+Forward*Y;};
    Solid({Icon(0,10),Icon(1.6,6),Icon(1,-15),Icon(0,-17),Icon(-1,-15),Icon(-1.6,6)},Ink);
    for(double Side:{-1.,1.})
    {
        Solid({Icon(0,3),Icon(Side*21,-1),Icon(Side*20,-3),Icon(0,0)},Ink);
        Solid({Icon(0,-12),Icon(Side*6,-13),Icon(Side*6,-15),Icon(0,-14)},Ink);
    }
    Circle({0,0},3.2f,Paper,Paper); Circle({0,0},1.5f,Ink,Ink);

    TArray<FVector2D> Pod;
    for(int Side=0;Side<2;++Side)
        for(int I=0;I<=24;++I)
        {
            const double A=FMath::DegreesToRadians(-90.+Side*180.+I*7.5);
            Pod.Add(FVector2D(Side?-69:69,94)+FVector2D(FMath::Cos(A),FMath::Sin(A))*19);
        }
    TArray<FVector2D> PodShadow=Pod; for(auto& P:PodShadow) P.Y+=3;
    Solid(PodShadow,FLinearColor(0,0,0,.15f));
    Shape(Pod,Highlight,Paper,75,113); Outline(Pod,Paper,4); Outline(Pod,Ink,1.4f);
    const double Vario=FMath::Abs(Plane.GetSinkMps())<.05?0.:-Plane.GetSinkMps();
    const double Up=Vario<0?-1.:1.;
    for(int I=0;I<2;++I)
    {
        const double Y=94+I*7-3;
        Line({-74,Y+Up*3},{-68,Y-Up*3},Gold,2); Line({-68,Y-Up*3},{-62,Y+Up*3},Gold,2);
    }
    Line({-52,83},{-52,105},Gold,1);
    Text(TEXT("VARIO"),{-42,79},8,Muted);
    Text((Vario>=0?TEXT("+"):TEXT(""))+Number(Vario)+TEXT(" m/s"),{15,99},14,Ink,TEXT("BoldItalic"),true);
}
