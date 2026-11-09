#include "SimPlaneurHUD.h"
#include "SlopeFlightPawn.h"
#include "FirstFlightTutorial.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

void ASimPlaneurHUD::DrawFirstFlightTutorial(const ASlopeFlightPawn& Plane)
{
    if(!Plane.IsTutorialActive() || !CompassFont) return;
    const FFirstFlightTutorial& T=Plane.TutorialStatus();
    using S=EFirstFlightStep;
    const float Scale=FMath::Clamp(Canvas->SizeX/1400.f,.8f,1.3f);
    const float X=18*Scale, Y=160*Scale, Width=348*Scale;
    const FLinearColor Ink(.009f,.078f,.076f,.94f), Paper(1.f,.955f,.82f),
        Coral(.85f,.142f,.077f), Gold(1.f,.70f,.27f), Soft(.56f,.72f,.68f), Valid(.32f,.80f,.59f);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto Text=[&](const FString& Value,float PX,float PY,int Size,FLinearColor Colour,FName Face=TEXT("Regular"),float MaxWidth=0.f)
    {
        const FSlateFontInfo Font(CompassFont,FMath::Max(6,FMath::RoundToInt(Size*Scale)),Face);
        const float W=Measure->Measure(Value,Font).X;
        const float Fit=MaxWidth>0.f ? FMath::Min(1.f,MaxWidth/FMath::Max(1.f,W)) : 1.f;
        FCanvasTextItem Item(FVector2D(PX,PY),FText::FromString(Value),Font,Colour);
        Item.Scale=FVector2D(Fit,Fit);
        Item.bDontCorrectStereoscopic=true;
        Canvas->DrawItem(Item);
    };
    auto Shortcut=[&](EGliderControl A){const FString Key=Plane.Radio.Shortcut(A); return A==EGliderControl::Menu && Key==TEXT("Libre")?FString(TEXT("Échap")):Key;};
    FString Heading, Body;
    int Step=1;
    switch(T.Step)
    {
    case S::Walk:
        Heading=TEXT("Rejoins le départ");
        Body=TEXT("Bienvenue au chalet ! Le Topy est dans ton sac. Rejoins le repère doré : ZQSD / WASD pour marcher, Maj pour courir et la souris pour regarder.");
        Step=1; break;
    case S::Windsock:
        Heading=TEXT("D’où vient le vent ?");
        Body=TEXT("Regarde la manche à air quelques instants. Son ouverture fait face au vent ; sa pointe indique où l’air se dirige. On prépare le lancement face au vent.");
        Step=2; break;
    case S::Airflow:
        Heading=TEXT("Rends l’air visible");
        Body=Shortcut(EGliderControl::WindTrails)+TEXT(" : active les filets d’air. Ils montrent le déplacement de l’air ; les filets dorés signalent les ascendances. Tu pourras les garder pendant le vol.");
        Step=3; break;
    case S::Controls:
        Heading=TEXT("Vérifie les deux commandes");
        Body=TEXT("Bouge la direction à gauche puis à droite, et la profondeur dans les deux sens, puis recentre les manches. Le schéma suit tes commandes ; les extrémités validées deviennent vertes.");
        Step=4; break;
    case S::Launch:
        Heading=TEXT("À toi de prendre l’air !");
        Body=Plane.InBackpack() ? Shortcut(EGliderControl::Backpack)+TEXT(" : sors le Topy du sac. Regarde vers la zone de vol, face au vent. Essaie ensuite de rester en l’air jusqu’au signal d’atterrissage.")
            : Shortcut(EGliderControl::HandLaunch)+TEXT(" : lance le Topy devant toi. Fais des mouvements doux et garde-le devant la pente. Objectif : 2 min 30 de vol, à ton rythme.");
        Step=5; break;
    case S::Fly:
        Heading=TEXT("Prends tes repères en vol");
        Body=TEXT("Reste devant la pente et fais de larges virages. Cherche les filets ascendants pour conserver ta hauteur. Un posé avant le signal ? Tu pourras relancer et continuer.");
        Step=6; break;
    case S::Landing:
        Heading=TEXT("Prépare ton atterrissage");
        Body=TEXT("Le vent et l’ascendance vont diminuer doucement. Reviens vers la zone dégagée, garde de la vitesse et arrondis près du sol. Prends ton temps : le posé doit rester doux.");
        Step=7; break;
    case S::Retrieve:
        Heading=T.bRoughLanding?TEXT("Posé un peu ferme"):TEXT("Bien posé ! Va le récupérer");
        Body=T.bRoughLanding?TEXT("Les planeurs sont fragiles : pense à arrondir doucement près du sol. Heureusement, ici c’est un simulateur, ce n’est pas grave ! "):FString();
        Body+=TEXT("Rejoins ton planeur à pied. Maj permet de courir. Une fois à côté, appuie sur ")+Shortcut(EGliderControl::Backpack)+TEXT(" pour le remettre dans le sac.");
        if(T.FlightSeconds<150.f) Body+=TEXT(" Tu pourras ensuite relancer : ton temps de vol est conservé.");
        Step=8; break;
    case S::Complete:
        Heading=TEXT("Bravo, ton premier vol est terminé !");
        Body=TEXT("Tu sais maintenant te déplacer, lire le vent, lancer le Topy et le récupérer. Retourne dans Tuto pour recommencer ou quitter la leçon et retrouver tes réglages.");
        if(T.bRoughLanding) Body+=TEXT(" Pour le prochain posé, un arrondi plus doux ménagera ton planeur.");
        Step=9; break;
    default: return;
    }
    if(T.bRecoveryNeeded)
    {
        Heading=T.bCrashed?TEXT("Oups, on réessaie ?"):TEXT("Oh non, on l’a perdu de vue !");
        Body=T.bCrashed ? TEXT("Ce vol s’arrête ici, mais ce n’est pas grave ! ")
            : TEXT("Le planeur est hors de vue depuis un moment. Pas de souci ! ");
        Body+=Shortcut(EGliderControl::Relaunch)+TEXT(" : récupère le Topy au départ et recommence. Ton temps de vol est conservé.");
    }
    if(T.Step==S::Launch) Body+=FString::Printf(TEXT(" Cap conseillé : %03d° (nord = 000°)."),FMath::RoundToInt(ValleyAir::Settings().WindFromDegrees)%360);

    // Wrap using the actual runtime font so long remapped shortcuts stay inside.
    TArray<FString> Words, Lines;
    Body.ParseIntoArrayWS(Words);
    const FSlateFontInfo BodyFont(CompassFont,FMath::Max(6,FMath::RoundToInt(12*Scale)),TEXT("Regular"));
    FString Line;
    for(const FString& Word:Words)
    {
        const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
        if(!Line.IsEmpty() && Measure->Measure(Candidate,BodyFont).X>Width-32*Scale)
        { Lines.Add(Line); Line=Word; }
        else Line=Candidate;
    }
    if(!Line.IsEmpty()) Lines.Add(Line);
    const bool Controls=T.Step==S::Controls && !T.bRecoveryNeeded;
    const bool Flight=(T.Step==S::Fly || T.Step==S::Landing || T.bRecoveryNeeded);
    const float BodyBottom=Y+(79+Lines.Num()*19)*Scale;
    const float Footer=BodyBottom+(Controls?139:Flight?41:6)*Scale;
    const float Height=Footer-Y+31*Scale;

    // Project only a real visible world point. Never mirror a target behind the
    // camera, and do not display a marker through terrain or the chalet.
    FVector Target=FVector::ZeroVector;
    FString TargetName;
    if(!T.bRecoveryNeeded)
    {
        if(T.Step==S::Walk || T.Step==S::Launch) {Target=T.Step==S::Walk?T.WalkTarget:T.LaunchPoint; TargetName=TEXT("Départ");}
        else if(T.Step==S::Windsock) {Target=T.WindsockTarget; TargetName=TEXT("Manche à air");}
        else if(T.Step==S::Landing) {Target=T.LandingTarget; TargetName=TEXT("Zone conseillée");}
        else if(T.Step==S::Retrieve) {Target=Plane.GetActorLocation(); TargetName=TEXT("Ton Topy");}
    }
    if(!TargetName.IsEmpty())
    {
        FVector Eye; FRotator View;
        PlayerOwner->GetPlayerViewPoint(Eye,View);
        const FVector Aim=Target+FVector(0,0,T.Step==S::Windsock?0:90);
        FVector2D Screen;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TutorialMarker),false,&Plane);
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Aim,ECC_Visibility,Query)
            && Hit.Distance<(Aim-Eye).Size()-150.;
        if(FVector::DotProduct(View.Vector(),Aim-Eye)>0 && !Blocked
            && PlayerOwner->ProjectWorldLocationToScreen(Aim,Screen)
            && Screen.X>40*Scale && Screen.X<Canvas->SizeX-80*Scale
            && Screen.Y>145*Scale && Screen.Y<Canvas->SizeY-165*Scale
            && !(Screen.X<X+Width+30*Scale && Screen.Y<Y+Height+30*Scale))
        {
            const float R=17*Scale;
            for(int I=0;I<40;++I)
            {
                const float A=I*UE_TWO_PI/40.,B=(I+1)*UE_TWO_PI/40.;
                DrawLine(Screen.X+R*FMath::Cos(A),Screen.Y+R*FMath::Sin(A),
                    Screen.X+R*FMath::Cos(B),Screen.Y+R*FMath::Sin(B),Gold,2*Scale);
            }
            DrawLine(Screen.X-6*Scale,Screen.Y,Screen.X+6*Scale,Screen.Y,Paper,Scale);
            DrawLine(Screen.X,Screen.Y-6*Scale,Screen.X,Screen.Y+6*Scale,Paper,Scale);
            const FString Caption=FString::Printf(TEXT("%s · %.0f m"),*TargetName,FVector::Distance(Plane.PilotPosition(),Target)*.01);
            const float LabelWidth=Measure->Measure(Caption,BodyFont).X+16*Scale;
            const float LX=FMath::Clamp(float(Screen.X)-LabelWidth*.5f,4.f,Canvas->SizeX-LabelWidth-4.f);
            DrawRect(Ink,LX,Screen.Y+23*Scale,LabelWidth,24*Scale);
            Text(Caption,LX+8*Scale,Screen.Y+25*Scale,12,Paper);
        }
        else
        {
            // A separate screen-edge direction cue is not a world marker: it
            // remains useful when the real target is below view or occluded.
            const FVector Local=View.UnrotateVector(Aim-Eye);
            FVector2D Direction(Local.Y,-Local.Z);
            FString Guidance;
            if(Local.X<0)
            {
                Direction=FVector2D(Local.Y<0?-1.:1.,0.);
                Guidance=Local.Y<0?TEXT("Derrière toi · tourne à gauche"):TEXT("Derrière toi · tourne à droite");
            }
            else if(FMath::Abs(Direction.X)>FMath::Abs(Direction.Y))
                Guidance=Direction.X<0?TEXT("Tourne à gauche"):TEXT("Tourne à droite");
            else Guidance=Direction.Y<0?TEXT("Regarde plus haut"):TEXT("Regarde plus bas");
            if(Blocked && Local.X>=0) Guidance=TEXT("Repère masqué · direction à suivre");
            if(!Direction.Normalize()) Direction=FVector2D(0.,1.);
            const FString Caption=FString::Printf(TEXT("%s · %.0f m"),*TargetName,FVector::Distance(Plane.PilotPosition(),Target)*.01);
            const float CueWidth=FMath::Min(430*Scale,FMath::Max(float(Measure->Measure(Caption,BodyFont).X),float(Measure->Measure(Guidance,BodyFont).X))+64*Scale);
            const float CX=(Canvas->SizeX-CueWidth)*.5f,CY=Canvas->SizeY-192*Scale;
            DrawRect(Ink,CX,CY,CueWidth,52*Scale);
            DrawRect(Gold,CX,CY,2*Scale,52*Scale);
            const FVector2D Centre(CX+27*Scale,CY+26*Scale);
            const FVector2D Tip=Centre+Direction*13*Scale;
            const FVector2D Side(-Direction.Y,Direction.X);
            auto Segment=[&](FVector2D A,FVector2D B)
            {DrawLine(A.X,A.Y,B.X,B.Y,Gold,2*Scale);};
            Segment(Centre-Direction*12*Scale,Tip);
            Segment(Tip,Tip-Direction*9*Scale+Side*7*Scale);
            Segment(Tip,Tip-Direction*9*Scale-Side*7*Scale);
            Text(Caption,CX+51*Scale,CY+5*Scale,12,Paper,TEXT("Bold"),CueWidth-60*Scale);
            Text(Guidance,CX+51*Scale,CY+29*Scale,10,Gold,TEXT("Regular"),CueWidth-60*Scale);
        }
    }

    DrawRect(Ink,X,Y,Width,Height);
    DrawRect(T.bRecoveryNeeded?Coral:Gold,X,Y,3*Scale,Height);
    Text(TEXT("TUTO  /  MON 1ER VOL"),X+16*Scale,Y+10*Scale,12,Gold,TEXT("BoldItalic"));
    Text(FString::Printf(TEXT("%02d / 09"),Step),X+Width-67*Scale,Y+10*Scale,11,Paper,TEXT("Bold"));
    for(int I=0;I<9;++I) DrawRect(I<Step?Gold:FLinearColor(.08f,.20f,.19f),X+(16+I*35)*Scale,Y+34*Scale,29*Scale,2*Scale);
    Text(Heading,X+16*Scale,Y+46*Scale,15,Paper,TEXT("BoldItalic"),Width-32*Scale);
    for(int I=0;I<Lines.Num();++I) Text(Lines[I],X+16*Scale,Y+(77+I*19)*Scale,12,Paper,TEXT("Regular"),Width-32*Scale);

    if(Controls)
    {
        auto Bar=[&](float DY,const FString& Name,float Value,const TCHAR* Left,const TCHAR* Right,bool CheckedLeft,bool CheckedRight)
        {
            const float BX=X+18*Scale,BY=BodyBottom+DY*Scale,BW=Width-36*Scale;
            Text(Name,BX,BY,12,Gold,TEXT("BoldItalic"));
            Text(Left,BX,BY+21*Scale,10,CheckedLeft?Valid:Soft,TEXT("Bold"));
            const float RightWidth=Measure->Measure(FString(Right),FSlateFontInfo(CompassFont,FMath::Max(6,FMath::RoundToInt(10*Scale)),TEXT("Bold"))).X;
            Text(Right,BX+BW-RightWidth,BY+21*Scale,10,CheckedRight?Valid:Soft,TEXT("Bold"));
            DrawRect(FLinearColor(.06f,.17f,.16f),BX,BY+42*Scale,BW,6*Scale);
            DrawRect(Soft,BX+BW*.5f-Scale*.5f,BY+38*Scale,Scale,14*Scale);
            DrawRect(CheckedLeft?Valid:Soft,BX,BY+40*Scale,4*Scale,10*Scale);
            DrawRect(CheckedRight?Valid:Soft,BX+BW-4*Scale,BY+40*Scale,4*Scale,10*Scale);
            const float Marker=BX+BW*(FMath::Clamp(Value,-1.f,1.f)+1.f)*.5f;
            DrawRect(Paper,Marker-3*Scale,BY+37*Scale,6*Scale,16*Scale);
        };
        Bar(2,TEXT("DIRECTION"),T.Rudder,TEXT("Gauche"),TEXT("Droite"),T.RudderLeft,T.RudderRight);
        Bar(67,TEXT("PROFONDEUR"),T.Elevator,TEXT("Cabrer"),TEXT("Piquer"),T.ElevatorUp,T.ElevatorDown);
    }
    else if(Flight)
    {
        const int Seconds=FMath::Clamp(FMath::FloorToInt(T.FlightSeconds),0,150);
        Text(FString::Printf(TEXT("TEMPS DE VOL  %d:%02d / 2:30%s"),Seconds/60,Seconds%60,T.bRecoveryNeeded?TEXT(" · EN PAUSE"):TEXT("")),
            X+16*Scale,BodyBottom+3*Scale,11,Gold,TEXT("Bold"),Width-32*Scale);
        DrawRect(FLinearColor(.08f,.20f,.19f),X+16*Scale,BodyBottom+26*Scale,Width-32*Scale,4*Scale);
        DrawRect(Gold,X+16*Scale,BodyBottom+26*Scale,(Width-32*Scale)*Seconds/150.f,4*Scale);
    }
    Text(Shortcut(EGliderControl::Menu)+TEXT(" · Menu / quitter le tutoriel"),X+16*Scale,Footer,10,Soft,TEXT("Regular"),Width-32*Scale);
}
