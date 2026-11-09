#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"

void ASimPlaneurHUD::UpdateEquipmentOpacity(const ASlopeFlightPawn& Plane)
{
    const double Now = FPlatformTime::Seconds();
    if (LastEquipmentUpdate < 0.) LastEquipmentActivity = LastEquipmentUpdate = Now;
    using A = EGliderControl;
    bool Interacting = Plane.IsMenuOpen() || !Plane.EquipmentConfirmation().IsEmpty();
    // Only interface/equipment actions wake the strip, never walking, looking
    // around or moving the transmitter sticks. Honour remapped radio bindings.
    for (A Action : {A::Winch, A::Backpack, A::Equipment, A::WindTrails,
        A::Vario, A::Camera, A::Relaunch, A::Noclip, A::TowRelease, A::Menu})
        Interacting |= Plane.Radio.Pressed(Action) || Plane.Radio.Command(Action) > .5f;
    if (!Plane.IsNoclip())
        Interacting |= Plane.Radio.Pressed(A::HandLaunch) || Plane.Radio.Command(A::HandLaunch) > .5f;
    for (FKey Key : {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5,
        EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::G, EKeys::F12, EKeys::E})
        Interacting |= PlayerOwner->IsInputKeyDown(Key) || PlayerOwner->WasInputKeyJustPressed(Key);
    if (Interacting) LastEquipmentActivity = Now;
    const float Target = Now-LastEquipmentActivity < 2.5 ? .9f : .5f;
    // Exponential interpolation is independent of frame rate, with a faster
    // return to readability and a gentle fade after the last interaction.
    const double Delta = FMath::Max(0., Now-LastEquipmentUpdate);
    const double Response = Target > EquipmentOpacity ? .12 : .55;
    EquipmentOpacity = FMath::Lerp(EquipmentOpacity, Target, float(1.-FMath::Exp(-Delta/Response)));
    LastEquipmentUpdate = Now;
}

void ASimPlaneurHUD::DrawEquipmentHUD(const ASlopeFlightPawn& Plane)
{
    const FLinearColor Petrol(.025f,.12f,.14f), Cream(.95f,.91f,.77f), Gold(1.f,.70f,.27f), Coral(.92f,.25f,.16f);
    UFont* Font=GEngine->GetSmallFont();
    const float Scale=FMath::Clamp(Canvas->SizeX/1400.f,.6f,1.4f);
    const float Margin=12.f*Scale;
    const float Bottom=Canvas->SizeY-76.f*Scale;
    const FLinearColor Muted(.47f,.61f,.60f), Surface(.04f,.17f,.19f);
    auto Fade=[&](FLinearColor Colour) { Colour.A *= EquipmentOpacity; return Colour; };
    auto Rounded=[&](float X,float Y,float Width,float Height,float Radius,FLinearColor Colour,float Stroke=0.f)
    {
        TArray<FVector2D> Outline;
        TArray<FVector2D> Inset;
        const FVector2D Centres[]={{X+Radius,Y+Radius},{X+Width-Radius,Y+Radius},
            {X+Width-Radius,Y+Height-Radius},{X+Radius,Y+Height-Radius}};
        for(int Corner=0;Corner<4;++Corner)
            for(int Segment=0;Segment<=6;++Segment)
            {
                const double Angle=FMath::DegreesToRadians(180.+Corner*90.+Segment*15.);
                const FVector2D Direction(FMath::Cos(Angle),FMath::Sin(Angle));
                Outline.Add(Centres[Corner]+Direction*Radius);
                Inset.Add(Centres[Corner]+Direction*(Radius-Stroke));
            }
        TArray<FCanvasUVTri> Triangles;
        for(int I=0;I<Outline.Num();++I)
        {
            FCanvasUVTri Tri;
            // Draw borders as rings, so translucent fills do not stack over a
            // second solid card and obscure the world at the lowest opacity.
            Tri.V0_Pos=Stroke>0.f ? Inset[I] : FVector2D(X+Width*.5,Y+Height*.5);
            Tri.V1_Pos=Outline[I]; Tri.V2_Pos=Outline[(I+1)%Outline.Num()];
            Tri.V0_Color=Tri.V1_Color=Tri.V2_Color=Fade(Colour);
            Triangles.Add(Tri);
            if (Stroke>0.f)
            {
                Tri.V1_Pos=Tri.V2_Pos;
                Tri.V2_Pos=Inset[(I+1)%Inset.Num()];
                Triangles.Add(Tri);
            }
        }
        FCanvasTriangleItem Shape(Triangles,GWhiteTexture);
        Shape.BlendMode=SE_BLEND_Translucent;
        Canvas->DrawItem(Shape);
    };
    // One continuous petrol rail, matching the menu and the ivory instruments.
    // All layers share the same 50% idle / 90% interaction opacity.
    const FLinearColor Ink(.009f,.078f,.076f), Ivory(1.f,.955f,.82f),
        Accent(.85f,.142f,.077f), Brass(.75f,.41f,.075f);
    DrawRect(Fade(Ink),0,Bottom,Canvas->SizeX,76.f*Scale);
    if(!CompassFont)
    {
        CompassFont=NewObject<UFont>(this);
        CompassFont->FontCacheType=EFontCacheType::Runtime;
        CompassFont->GetMutableInternalCompositeFont()=*FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),12).GetCompositeFont();
    }
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto LabelText=[&](const FString& Value,float X,float Y,float Size,FLinearColor Colour,float MaxWidth,FName Face=TEXT("BoldItalic"))
    {
        FSlateFontInfo Info(CompassFont,FMath::Max(6,FMath::RoundToInt(Size*Scale)),Face);
        const FVector2D Extent=Measure->Measure(Value,Info);
        const float Fit=FMath::Min(1.f,MaxWidth/FMath::Max(1.f,float(Extent.X)));
        FCanvasTextItem Text(FVector2D(X,Y),FText::FromString(Value),Info,Fade(Colour));
        Text.Scale=FVector2D(Fit,Fit);
        Text.bDontCorrectStereoscopic=true;
        Canvas->DrawItem(Text);
        return float(Extent.X)*Fit;
    };
    auto Solid=[&](const TArray<FVector2D>& Points,FLinearColor Colour)
    {
        TArray<FCanvasUVTri> Tris;
        for(int I=1;I+1<Points.Num();++I)
        {
            FCanvasUVTri T;
            T.V0_Pos=Points[0]; T.V1_Pos=Points[I]; T.V2_Pos=Points[I+1];
            T.V0_Color=T.V1_Color=T.V2_Color=Fade(Colour); Tris.Add(T);
        }
        FCanvasTriangleItem Item(Tris,GWhiteTexture); Item.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Item);
    };
    // Small line drawings keep the shortcuts legible without texture assets.
    auto Icon=[&](int Kind,float X,float Y,FLinearColor Colour)
    {
        auto L=[&](double AX,double AY,double BX,double BY)
        { DrawLine(X+AX*Scale,Y+AY*Scale,X+BX*Scale,Y+BY*Scale,Fade(Colour),1.5f*Scale); };
        auto C=[&](double CX,double CY,double R)
        { for(int I=0;I<24;++I) {const double A=I*UE_TWO_PI/24,B=(I+1)*UE_TWO_PI/24; L(CX+R*FMath::Cos(A),CY+R*FMath::Sin(A),CX+R*FMath::Cos(B),CY+R*FMath::Sin(B));} };
        switch(Kind)
        {
        case 0: // Winch drum
            L(3,5,3,22); L(27,5,27,22); L(3,8,27,8); L(3,19,27,19);
            for(int I=7;I<27;I+=4) L(I,7,I,20);
            L(0,24,30,24); L(0,13,3,13); L(27,13,32,13); break;
        case 1: // Folded map
            L(2,6,11,2); L(11,2,21,6); L(21,6,30,2); L(30,2,30,22);
            L(30,22,21,26); L(21,26,11,22); L(11,22,2,26); L(2,26,2,6);
            L(11,2,11,22); L(21,6,21,26); break;
        case 2: // Backpack
            L(7,8,7,26); L(7,26,26,26); L(26,26,26,8); L(26,8,7,8);
            L(12,8,12,3); L(12,3,21,3); L(21,3,21,8);
            L(11,17,22,17); L(11,17,11,23); L(11,23,22,23); L(22,23,22,17); break;
        case 3: // Elastic coil and anchor
            L(0,15,5,15); L(27,15,32,15);
            for(int I=0;I<5;++I) {L(5+I*4,15,7+I*4,6); L(7+I*4,6,11+I*4,24); L(11+I*4,24,13+I*4,15);} break;
        case 4: // Air streams
            L(0,8,24,8); L(5,15,32,15); L(0,22,22,22); L(24,8,28,4); L(22,22,27,26); break;
        case 5: // Vario chevrons
            L(5,14,16,3); L(16,3,27,14); L(5,25,16,14); L(16,14,27,25); break;
        case 6: // Camera
            L(2,8,9,8); L(9,8,12,3); L(12,3,21,3); L(21,3,24,8); L(24,8,31,8);
            L(31,8,31,26); L(31,26,2,26); L(2,26,2,8); C(16,17,6); break;
        case 7: // Return arrow above a glider
            L(3,18,30,18); L(16,10,16,28); L(10,26,22,26);
            L(27,8,8,8); L(8,8,13,3); L(8,8,13,13); break;
        case 8: // F3F gates
            L(6,2,6,28); L(26,2,26,28); L(6,2,16,7); L(16,7,6,12);
            L(26,2,36,7); L(36,7,26,12); L(6,24,26,24); break;
        case 9: // Exploration eye
            L(0,15,8,7); L(8,7,24,7); L(24,7,32,15); L(32,15,24,23);
            L(24,23,8,23); L(8,23,0,15); C(16,15,5); L(2,28,30,2); break;
        }
    };
    struct FEntry { EGliderControl Action; FString Label; bool Active; bool Enabled; };
    const FString BagLabel=Plane.InBackpack()?TEXT("Sortir le planeur"):Plane.IsHeldForLaunch()?TEXT("Ranger le planeur"):TEXT("Récupérer à pied");
    const FEntry Entries[] = {
        {EGliderControl::Backpack,BagLabel,Plane.InBackpack(),Plane.IsWalking()},
        {EGliderControl::Equipment,TEXT("Élastique"),Plane.BungeeSelected()||(Plane.BungeeStatus().Installed()&&!Plane.BungeeStatus().IsWinch()),Plane.IsWalking()},
        {EGliderControl::WindTrails,TEXT("Filets d’air"),Plane.TrailsVisible(),true},
        {EGliderControl::Vario,TEXT("Vario"),Plane.VarioEnabled(),true},
        {EGliderControl::Camera,TEXT("Caméra"),false,!Plane.IsWalking()&&!Plane.IsNoclip()},
        {EGliderControl::Relaunch,TEXT("Récupérer"),false,true},
        {EGliderControl::Noclip,TEXT("Noclip"),Plane.IsNoclip(),true}};
    const float Cell=(Canvas->SizeX-2*Margin)/10.f;
    int32 Slot=0;
    auto Item=[&](const FString& Key,const FString& Label,bool Active,bool Enabled,bool Quiet)
    {
        const int Index=Slot++;
        if(Plane.IsTutorialActive())
        {
            const auto Step=Plane.TutorialStatus().Step;
            if(Index==0 || Index==3 || Index==8 || Index==9) Enabled=false;
            if(Index==2) Enabled=Enabled && (Step==EFirstFlightStep::Launch || Step==EFirstFlightStep::Retrieve);
            if(Index==7) Enabled=Step>=EFirstFlightStep::Launch && Step!=EFirstFlightStep::Complete;
        }
        const float X=Margin+Index*Cell, Y=Bottom+7*Scale, H=62*Scale;
        const FLinearColor Foreground=(Quiet||!Enabled)?Muted:Ivory;
        const bool Selected=Active && (Index==0 || Index==3 || Index==8);
        if(Selected)
        {
            Solid({{X+9*Scale,Y},{X+Cell-3*Scale,Y},{X+Cell-12*Scale,Y+H},{X,Y+H}},Accent);
            DrawRect(Fade(Ivory),X+12*Scale,Y+H-5*Scale,Cell-32*Scale,2*Scale);
        }
        else if(Index>0) DrawRect(Fade(Brass),X,Y+6*Scale,Scale,H-12*Scale);
        const float KeyWidth=FMath::Clamp(float(Key.Len())*8.f*Scale+12*Scale,30*Scale,Cell-55*Scale);
        Rounded(X+10*Scale,Y+5*Scale,KeyWidth,19*Scale,2*Scale,Foreground);
        LabelText(Key,X+15*Scale,Y+5*Scale,12,Ink,KeyWidth-8*Scale,TEXT("Bold"));
        Icon(Index,X+Cell-46*Scale,Y+3*Scale,Foreground);
        if(Active && !Selected) Rounded(X+Cell-9*Scale,Y+4*Scale,5*Scale,5*Scale,2.5f*Scale,Gold);
        LabelText(Label,X+10*Scale,Y+35*Scale,13,Foreground,Cell-19*Scale);
    };
    Item(Plane.Radio.Shortcut(EGliderControl::Winch),TEXT("Treuil"),Plane.WinchSelected()||(Plane.BungeeStatus().Installed()&&Plane.BungeeStatus().IsWinch()),Plane.IsWalking(),false);
    Item(TEXT("F2"),TEXT("Carte"),false,true,false);
    for(int I=0;I<UE_ARRAY_COUNT(Entries);++I)
    {
        const auto& Entry=Entries[I];
        if(Entry.Action==EGliderControl::Noclip) Item(TEXT("F9"),TEXT("F3F"),Plane.F3FEnabled(),Plane.IsWalking()||Plane.F3FEnabled(),false);
        Item(Plane.Radio.Shortcut(Entry.Action),Entry.Label,Entry.Active,Entry.Enabled,Entry.Action==EGliderControl::Noclip);
    }
    FString Prompt=Plane.EquipmentPrompt();
    FString Notice=Plane.ExplorationNotice();
    if(!Plane.F3FPrompt().IsEmpty()) Prompt=Plane.F3FPrompt();
    if(Prompt.IsEmpty() && Plane.IsHeldForLaunch() && !Plane.InBackpack())
    {
        Prompt=Plane.IsThrowing()?TEXT("Lancer en cours…"):TEXT("PLANEUR EN MAIN · ")+Plane.Radio.Shortcut(EGliderControl::HandLaunch)+TEXT(" pour lancer");
        Notice=TEXT("WASD / ZQSD marcher · Maj courir · Souris regarder");
    }
    if(!Plane.GetContactLabel().IsEmpty() && !Plane.IsHeldForLaunch() && !Plane.IsSettling())
    {
        Prompt=TEXT("PLANEUR POSÉ · rejoins-le pour le récupérer avec ")+Plane.Radio.Shortcut(EGliderControl::Backpack);
        Notice=Plane.GetContactLabel()+TEXT(" · Maj pour courir");
    }
    if(!Plane.EquipmentConfirmation().IsEmpty()) Notice=Plane.EquipmentConfirmation();
    if (!Plane.IsNoclip() && (!Prompt.IsEmpty() || !Notice.IsEmpty() || Plane.InBackpack()))
    {
        const float Y=Bottom-64*Scale;
        const FString Main=Prompt.IsEmpty() ? (Plane.InBackpack()?TEXT("PLANEUR AU SAC · WASD / ZQSD marcher · Maj courir"):Notice) : Prompt;
        const FSlateFontInfo HintFont(CompassFont,FMath::Max(6,FMath::RoundToInt(12*Scale)),TEXT("BoldItalic"));
        const float Desired=FMath::Max(float(Measure->Measure(Main,HintFont).X),float(Measure->Measure(Notice,HintFont).X))+42*Scale;
        const float HintWidth=FMath::Clamp(Desired,340*Scale,Canvas->SizeX-2*Margin);
        Rounded(Margin,Y,HintWidth,55*Scale,5*Scale,Ink);
        DrawRect(Fade(Gold),Margin+10*Scale,Y+9*Scale,2*Scale,36*Scale);
        LabelText(Main,Margin+23*Scale,Y+6*Scale,12,Ivory,HintWidth-36*Scale);
        if (!Prompt.IsEmpty() || Plane.InBackpack())
        {
            LabelText(Notice,Margin+23*Scale,Y+31*Scale,10,Ivory,HintWidth-36*Scale,TEXT("Regular"));
        }
    }
    const auto& Bungee=Plane.BungeeStatus();
    if (!Bungee.Connected()) return;
    const float DialScale=Scale*InstrumentScale;
    const FVector2D Centre(Canvas->SizeX-30*Scale-102*DialScale,Bottom-83*Scale-102*DialScale);
    const float Radius=102*DialScale;
    // Filled ivory dial, dark bezel, gold inner rim, engraved ticks and red needle.
    TArray<FCanvasUVTri> Fan;
    auto Point=[&](double Degrees,double R)
    {
        const double A=FMath::DegreesToRadians(Degrees);
        return Centre+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*DialScale;
    };
    for (int I=0; I<72; ++I)
    {
        FCanvasUVTri Tri;
        Tri.V0_Pos=Centre; Tri.V1_Pos=Point(I*5,102); Tri.V2_Pos=Point((I+1)*5,102);
        Tri.V0_Color=Tri.V1_Color=Tri.V2_Color=Cream;
        Fan.Add(Tri);
    }
    FCanvasTriangleItem Disc(Fan,GWhiteTexture);
    Disc.BlendMode=SE_BLEND_Translucent;
    Canvas->DrawItem(Disc);
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float W)
    { DrawLine(A.X,A.Y,B.X,B.Y,C,W*DialScale); };
    for(int I=0;I<72;++I)
    {
        Line(Point(I*5,102),Point((I+1)*5,102),Petrol,7);
        Line(Point(I*5,97),Point((I+1)*5,97),Gold,2);
    }
    const double Maximum=FMath::Max(10.,FMath::CeilToDouble(Bungee.ScaleNewtons()/10.)*10.);
    const double Value=Plane.BungeeTension();
    for(int I=0;I<=40;++I)
    {
        const double Angle=135.+270.*I/40.;
        Line(Point(Angle,90),Point(Angle,I%5==0?77:84),Petrol,I%5==0?2.f:1.f);
        if(I%10==0)
        {
            const FString Label=FString::Printf(TEXT("%.0f"),Maximum*I/40.);
            float W,H; GetTextSize(Label,W,H,Font,.85f*DialScale);
            const FVector2D P=Point(Angle,66);
            DrawText(Label,Petrol,P.X-W/2,P.Y-H/2,Font,.85f*DialScale);
        }
    }
    const double Low=135.+270.*(Bungee.IsWinch()?Bungee.Winch.MaximumForce()*.3:Bungee.Stiffness*18.)/Maximum;
    const double High=135.+270.*(Bungee.IsWinch()?Bungee.Winch.MaximumForce():Bungee.Stiffness*32.)/Maximum;
    for(double A=Low;A<High;A+=3.) Line(Point(A,94),Point(FMath::Min(A+3.,High),94),FLinearColor(.15f,.48f,.32f),4);
    const double Angle=135.+270.*FMath::Clamp(Value/Maximum,0.,1.);
    Line(Point(Angle+180,15),Point(Angle,76),Coral,3);
    DrawRect(Petrol,Centre.X-4*DialScale,Centre.Y-4*DialScale,8*DialScale,8*DialScale);
    DrawText(TEXT("TRACTION · N"),Petrol,Centre.X-42*DialScale,Centre.Y-35*DialScale,Font,.85f*DialScale);
    const FString Reading=FString::Printf(TEXT("%.1f N"),Value);
    float W,H; GetTextSize(Reading,W,H,Font,1.2f*DialScale);
    DrawText(Reading,Petrol,Centre.X-W/2,Centre.Y+31*DialScale,Font,1.2f*DialScale);
    const FString Label=Bungee.IsWinch()?FString::Printf(TEXT("TREUIL · %.0f %%"),Plane.WinchCommand()*100.):TEXT("ÉLASTIQUE");
    GetTextSize(Label,W,H,Font,.8f*DialScale);
    DrawText(Label,Petrol,Centre.X-W/2,Centre.Y+60*DialScale,Font,.8f*DialScale);
}
