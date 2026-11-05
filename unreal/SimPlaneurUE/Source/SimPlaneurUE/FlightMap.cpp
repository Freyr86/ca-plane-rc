#include "FlightMap.h"
#include "Airfield.h"
#include "CirqueLandscape.h"
#include "CirqueSite.h"
#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SFlightMap::Construct(const FArguments& Args)
{
    Plane=Args._Plane;
    const FString Root=FPaths::ProjectContentDir()/TEXT("Slate/Maps");
    if(auto* Texture=FImageUtils::ImportFileAsTexture2D(Root/TEXT("massif.png"))) Massif=FDeferredCleanupSlateBrush::CreateBrush(Texture);
    if(auto* Texture=FImageUtils::ImportFileAsTexture2D(Root/TEXT("lac.png"))) Lake=FDeferredCleanupSlateBrush::CreateBrush(Texture);
    SetClipping(EWidgetClipping::ClipToBounds);
    SetCursor(EMouseCursor::GrabHand);
    Recenter();
}

void SFlightMap::Recenter()
{
    Centre=Plane->MapPositionXY();
    ClampView();
    bHasCursor=false;
}

void SFlightMap::ClampView()
{
    const FVector2D Size=GetCachedGeometry().GetLocalSize();
    const double HalfHeight=FMath::Min(5600.,.5*Span*Size.Y/FMath::Max(1.,Size.X));
    Centre.X=FMath::Clamp(Centre.X,-5600.+HalfHeight,5600.-HalfHeight);
    Centre.Y=FMath::Clamp(Centre.Y,-5600.+Span*.5,5600.-Span*.5);
}

FVector2D SFlightMap::WorldAt(FVector2D Local) const
{
    const FVector2D Size=GetCachedGeometry().GetLocalSize();
    const FVector2D D=(Local-.5*Size)*(Span/FMath::Max(1.,Size.X));
    return Centre+FVector2D(-D.Y,D.X);
}

FString SFlightMap::Coordinates() const
{
    const FVector2D P=bHasCursor && IsHovered() ? WorldAt(CursorLocal) : Centre;
    return FString::Printf(TEXT("%s     X  %+.0f m      Y  %+.0f m"),bHasCursor && IsHovered()?TEXT("POINTEUR"):TEXT("CENTRE"),P.X,P.Y);
}

void SFlightMap::Zoom(double Factor,FVector2D Anchor)
{
    const FVector2D Before=WorldAt(Anchor);
    Span=FMath::Clamp(Span*Factor,80.,11200.);
    Centre+=Before-WorldAt(Anchor);
    ClampView();
}

FReply SFlightMap::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
    CursorLocal=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    bHasCursor=true;
    Zoom(FMath::Pow(.75,E.GetWheelDelta()),CursorLocal);
    return FReply::Handled();
}
FReply SFlightMap::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
    DragStart=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    return FReply::Handled().CaptureMouse(SharedThis(this));
}
FReply SFlightMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    CursorLocal=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    bHasCursor=true;
    if(HasMouseCapture())
    {
        Centre+=WorldAt(DragStart)-WorldAt(CursorLocal);
        DragStart=CursorLocal;
        ClampView();
    }
    return FReply::Handled();
}
FReply SFlightMap::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)
{
    if(HasMouseCapture() && E.GetEffectingButton()==EKeys::LeftMouseButton) return FReply::Handled().ReleaseMouseCapture();
    return FReply::Unhandled();
}

int32 SFlightMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Draw,int32 L,const FWidgetStyle&,bool) const
{
    const FVector2f Size(G.GetLocalSize());
    const double Scale=Size.X/Span;
    auto Px=[&](double X,double Y){return FVector2f(Size.X*.5+(Y-Centre.Y)*Scale,Size.Y*.5-(X-Centre.X)*Scale);};
    auto Box=[&](int Depth,FVector2f P,FVector2f S,const FSlateBrush* Brush,FLinearColor Tint){
        FSlateDrawElement::MakeBox(Draw,L+Depth,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Tint);
    };
    auto Label=[&](int Depth,FVector2f P,const FString& S,FLinearColor Tint){
        FSlateDrawElement::MakeText(Draw,L+Depth,G.ToPaintGeometry(FVector2f(160,18),FSlateLayoutTransform(P)),S,FCoreStyle::GetDefaultFontStyle("Bold",9),ESlateDrawEffect::None,Tint);
    };
    const auto White=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Dark(.009f,.026f,.031f,.91f), Grid(.04f,.11f,.14f,.37f);
    Box(0,{0,0},Size,White,Dark);
    if(Massif) Box(1,Px(5600,-5600),FVector2f(11200*Scale),Massif->GetSlateBrush(),FLinearColor::White);
    if(Lake && Span<4500) Box(2,Px(650,-750),FVector2f(2000*Scale),Lake->GetSlateBrush(),FLinearColor::White);
    const double Target=95./Scale, Decade=FMath::Pow(10.,FMath::FloorToDouble(FMath::LogX(10.,Target)));
    const double Ratio=Target/Decade, Step=Decade*(Ratio<=1.?1.:Ratio<=2.?2.:Ratio<=5.?5.:10.);
    const double West=Centre.Y-Span*.5, South=Centre.X-Size.Y*.5/Scale;
    for(double Y=Step*FMath::CeilToDouble(West/Step);Y<=West+Span;Y+=Step)
    {
        const float U=Px(Centre.X,Y).X;
        Box(3,{U,0},{1,Size.Y},White,Grid);
        Box(4,{U+2,2},{61,16},White,Dark);
        Label(5,{U+4,2},FString::Printf(TEXT("Y %+.0f"),Y),FLinearColor::White);
    }
    for(double X=Step*FMath::CeilToDouble(South/Step);X<=South+Size.Y/Scale;X+=Step)
    {
        const float V=Px(X,Centre.Y).Y;
        Box(3,{0,V},{Size.X,1},White,Grid);
        if(V>18){Box(4,{2,V},{61,16},White,Dark);Label(5,{4,V},FString::Printf(TEXT("X %+.0f"),X),FLinearColor::White);}
    }
    TArray<FBox2D> Labels;
    auto Mark=[&](FVector2D World,const TCHAR* Name,FLinearColor Color,float Offset){
        const FVector2f P=Px(World.X,World.Y);
        if(P.X<0 || P.Y<0 || P.X>Size.X || P.Y>Size.Y) return;
        Box(6,P-FVector2f(4),FVector2f(8),White,Dark);
        Box(7,P-FVector2f(2.5f),FVector2f(5),White,Color);
        const float Width=FMath::Max(80.f,FCString::Strlen(Name)*6.f+12.f);
        FVector2f TextPos(FMath::Clamp(P.X+6.f,7.f,Size.X-Width-7.f),0);
        for(int Trial=0;Trial<15;++Trial)
        {
            const float Shift=Trial==0?0.f:((Trial+1)/2)*20.f*(Trial%2?1.f:-1.f);
            TextPos.Y=FMath::Clamp(P.Y+Offset+Shift,20.f,Size.Y-48.f);
            const FBox2D Bounds(FVector2D(TextPos)-FVector2D(2),FVector2D(TextPos)+FVector2D(Width+2,18));
            if(Labels.ContainsByPredicate([&](const auto& R){return R.Intersect(Bounds);}) && Trial<14) continue;
            Labels.Add(Bounds); break;
        }
        if(FMath::Abs(TextPos.Y-P.Y)>20.f)
            FSlateDrawElement::MakeLines(Draw,L+6,G.ToPaintGeometry(),{P,TextPos+FVector2f(0,8)},ESlateDrawEffect::None,Color,true,1.f);
        Box(6,TextPos,{Width,16},White,Dark);
        Label(7,TextPos+FVector2f(3,0),Name,Color);
    };
    Mark({ASlopeWorld::PilotX*.01,ASlopeWorld::PilotY*.01},TEXT("01  Prairie"),FLinearColor(1,.80f,.36f),-18);
    Mark({ASlopeWorld::ChaletX*.01,ASlopeWorld::ChaletY*.01},TEXT("02  Chalet"),FLinearColor(1,.80f,.36f),-18);
    const FVector2D NW=Airfield::Point(-Airfield::HalfLength,0)*.01, SE=Airfield::Point(Airfield::HalfLength,0)*.01;
    FSlateDrawElement::MakeLines(Draw,L+5,G.ToPaintGeometry(),{Px(NW.X,NW.Y),Px(SE.X,SE.Y)},ESlateDrawEffect::None,
        FLinearColor(.45f,.70f,.28f),true,FMath::Max(2.f,float(2*Airfield::HalfWidth*.01*Scale)));
    Mark(Airfield::Centre*.01,TEXT("03  Piste"),FLinearColor(1,.80f,.36f),-18);
    Mark({Cirque::CenterX*.01,Cirque::CenterY*.01},TEXT("Cirque des falaises"),FLinearColor(.91f,.88f,.72f),-18);
    Mark(CirqueSite::Pilot*.01,TEXT("04  Belvédère du cirque"),FLinearColor(1,.80f,.36f),-18);
    int32 Bungees=0,Winches=0;
    for(const auto& E:Plane->InstalledEquipment())
    {
        const bool Bungee=E.Type==ELaunchEquipment::Bungee;
        if(Bungee) ++Bungees; else ++Winches;
        const FVector2D A(E.Anchor*.01),D(E.Departure*.01);
        const FVector2f AP=Px(A.X,A.Y),DP=Px(D.X,D.Y);
        const FLinearColor Colour=ASlopeFlightPawn::EquipmentColour(E.Colour);
        FSlateDrawElement::MakeLines(Draw,L+5,G.ToPaintGeometry(),{AP,DP},ESlateDrawEffect::None,Colour,true,2.f);
        const FString Name=FString::Printf(TEXT("%s #%d"),Bungee?TEXT("Élastique"):TEXT("Treuil"),E.Id);
        Mark(A,*Name,Colour,-18);
        // Distinct endpoints become readable as the map is zoomed in.
        if((AP-DP).Size()>26.f) Mark(D,TEXT("Départ"),FLinearColor(.5f,1.f,.65f),2);
        else Box(7,DP-FVector2f(2),FVector2f(4),White,FLinearColor(.5f,1.f,.65f));
    }
    Box(8,{10,Size.Y-27},{370,20},White,Dark);
    Label(9,{17,Size.Y-25},FString::Printf(TEXT("MATÉRIEL  ·  Élastiques %d/10  ·  Treuils %d/10"),Bungees,Winches),FLinearColor(1,.91f,.68f));
    const FVector2D Pilot(Plane->PilotPosition()*.01);
    Mark(Pilot,TEXT("Pilote"),FLinearColor(.65f,1.f,.87f),1);
    if(Plane->IsNoclip() || !Plane->IsHeldForLaunch())
        Mark(Plane->MapPositionXY(),Plane->IsNoclip()?TEXT("Caméra"):TEXT("Planeur"),FLinearColor(.62f,.86f,1.f),18);
    return L+9;
}
