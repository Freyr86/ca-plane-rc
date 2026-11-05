#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
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
        EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F12, EKeys::E})
        Interacting |= PlayerOwner->IsInputKeyDown(Key) || PlayerOwner->WasInputKeyJustPressed(Key);
    if (Interacting) LastEquipmentActivity = Now;
    const bool Airborne = !Plane.IsWalking() && !Plane.IsNoclip() && !Plane.IsMenuOpen();
    const float Target = Now-LastEquipmentActivity < 2.5 ? .9f : (Airborne ? .2f : .4f);
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
    // Decorative layers fade faster than the controls themselves.
    DrawRect(FLinearColor(.012f,.065f,.075f,.94f*FMath::Pow(EquipmentOpacity,3.f)),0,Bottom,Canvas->SizeX,76.f*Scale);
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
    const float Gap=7*Scale;
    const float Cell=(Canvas->SizeX-2*Margin-8*Gap)/9.f;
    int32 Slot=0;
    auto Item=[&](const FString& Key,const FString& Label,bool Active,bool Enabled,bool Quiet)
    {
        const float X=Margin+Slot++*(Cell+Gap), Y=Bottom+10*Scale, H=56*Scale;
        const FLinearColor Accent=Quiet?Muted:Gold;
        Rounded(X,Y+3*Scale,Cell,H,6*Scale,FLinearColor(0,0,0,.35f*EquipmentOpacity*EquipmentOpacity));
        Rounded(X,Y,Cell,H,6*Scale,Active?Accent:FLinearColor(.12f,.28f,.29f),Scale);
        Rounded(X+Scale,Y+Scale,Cell-2*Scale,H-2*Scale,5*Scale,Active?FLinearColor(.08f,.27f,.28f):Surface);
        float KW,KH; GetTextSize(Key,KW,KH,Font,.92f*Scale);
        const float KeyWidth=FMath::Min(Cell-18*Scale,FMath::Max(32*Scale,KW+14*Scale));
        Rounded(X+8*Scale,Y+7*Scale,KeyWidth,21*Scale,4*Scale,Enabled?Cream:Muted);
        const float KeyScale=FMath::Min(.92f*Scale,(KeyWidth-8*Scale)/FMath::Max(1.f,KW)*.92f*Scale);
        DrawText(Key,Fade(Petrol),X+14*Scale,Y+10*Scale,Font,KeyScale);
        Rounded(X+Cell-14*Scale,Y+13*Scale,5*Scale,5*Scale,2.5f*Scale,Active?Accent:Muted*.5f);
        float W,HText; GetTextSize(Label,W,HText,Font,.91f*Scale);
        const float TextScale=.91f*Scale*FMath::Min(1.f,(Cell-16*Scale)/FMath::Max(1.f,W));
        DrawText(Label,Fade(Enabled?Cream:Muted),X+8*Scale,Y+36*Scale,Font,TextScale);
    };
    Item(Plane.Radio.Shortcut(EGliderControl::Winch),TEXT("Treuil"),Plane.WinchSelected()||(Plane.BungeeStatus().Installed()&&Plane.BungeeStatus().IsWinch()),Plane.IsWalking(),false);
    Item(TEXT("F2"),TEXT("Carte"),false,true,false);
    for(int I=0;I<UE_ARRAY_COUNT(Entries);++I)
    {
        const auto& Entry=Entries[I];
        Item(Plane.Radio.Shortcut(Entry.Action),Entry.Label,Entry.Active,Entry.Enabled,Entry.Action==EGliderControl::Noclip);
    }
    FString Prompt=Plane.EquipmentPrompt();
    FString Notice=Plane.ExplorationNotice();
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
        Rounded(Margin,Y,Canvas->SizeX-2*Margin,58*Scale,6*Scale,FLinearColor(.025f,.12f,.14f,.92f));
        DrawText(Prompt.IsEmpty() ? (Plane.InBackpack()?TEXT("PLANEUR AU SAC · WASD / ZQSD marcher · Maj courir"):Notice) : Prompt,
            Fade(Gold),Margin+10*Scale,Y+8*Scale,Font,Scale);
        if (!Prompt.IsEmpty() || Plane.InBackpack())
        {
            float W,H; GetTextSize(Notice,W,H,Font,.86f*Scale);
            const float Fit=FMath::Min(1.f,(Canvas->SizeX-2*Margin-20*Scale)/FMath::Max(1.f,W));
            DrawText(Notice,Fade(Cream),Margin+10*Scale,Y+32*Scale,Font,.86f*Scale*Fit);
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
