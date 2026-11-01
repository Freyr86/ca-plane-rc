#include "FlightMenu.h"
#include "Rendering/SlateRenderer.h"
#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor Ink(.011f,.026f,.032f), Paper(.94f,.95f,.88f), Light(.67f,.77f,.74f), Gold(1.f,.60f,.14f);
    const FLinearColor Panel(.009f,.023f,.028f,.94f), Rule(.18f,.25f,.25f,.55f);
    const FSlateBrush* Fill() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
    FText T(const FString& Value) { return FText::FromString(Value); }

    TSharedRef<STextBlock> Text(const FString& Value,int32 Size,FLinearColor Color=Paper,FName Face=TEXT("Regular"))
    {
        return SNew(STextBlock).Text(T(Value)).Font(FCoreStyle::GetDefaultFontStyle(Face,Size)).ColorAndOpacity(Color);
    }
    TSharedRef<STextBlock> Live(TFunction<FString()> Read,int32 Size,FLinearColor Color=Paper)
    {
        return SNew(STextBlock).Text_Lambda([Read]{return T(Read());})
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size)).ColorAndOpacity(Color);
    }
    void Add(SVerticalBox& List,TSharedRef<SWidget> Widget,float Bottom=0.f)
    {
        List.AddSlot().AutoHeight().Padding(0,0,0,Bottom)[Widget];
    }
    TSharedRef<SWidget> Line(FLinearColor Color=Rule)
    {
        return SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Color)];
    }
    const FButtonStyle& Style(bool Primary)
    {
        static const auto Make=[](FLinearColor Normal,FLinearColor Hover,FLinearColor Press)
        {
            FButtonStyle S;
            S.SetNormal(FSlateColorBrush(Normal)).SetHovered(FSlateColorBrush(Hover)).SetPressed(FSlateColorBrush(Press));
            S.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
            return S;
        };
        static const FButtonStyle Main=Make(Gold,FLinearColor(1.f,.75f,.34f),FLinearColor(.82f,.43f,.07f));
        static const FButtonStyle Other=Make(FLinearColor(.05f,.083f,.087f,.91f),FLinearColor(.105f,.17f,.174f),FLinearColor(.08f,.135f,.14f));
        return Primary ? Main : Other;
    }
    FString Dimensions(FIntPoint P) { return FString::Printf(TEXT("%d × %d"),P.X,P.Y); }
    const TCHAR* ModeNames[]={TEXT("Plein écran"),TEXT("Sans bordure"),TEXT("Fenêtré")};
    const TCHAR* QualityNames[]={TEXT("Faible"),TEXT("Moyenne"),TEXT("Élevée"),TEXT("Ultra"),TEXT("Cinéma")};

    // Simplified plan outlines in a 320 x 150 reference frame, nose up.
    // The full view and the detail share the same geometry and CG datum.
    class SCgPlan : public SLeafWidget
    {
    public:
        DECLARE_DELEGATE_OneParam(FSetOffset, int32);
        SLATE_BEGIN_ARGS(SCgPlan) {} SLATE_ARGUMENT(bool, K3600) SLATE_ATTRIBUTE(int32, OffsetMm) SLATE_EVENT(FSetOffset, OnMove) SLATE_END_ARGS()
        void Construct(const FArguments& Args)
        {
            Offset = Args._OffsetMm;
            OnMove = Args._OnMove;
            FullScale = 159.f / (Args._K3600 ? 3600.f : 1190.f);
            if (Args._K3600)
            {
                Outline = {
                    {{98,27.5f},{97,31},{96.7f,51},{97.3f,64},{98.7f,64},{99.3f,51},{99,31}},
                    {{97.3f,62},{97.7f,105.7f},{98.3f,105.7f},{98.7f,62}},
                    {{98,51.4f},{64.6f,52},{31.2f,53.8f},{18.5f,55.7f},{18.5f,57.3f},{31.2f,57.6f},{64.6f,58.7f},{98,59.6f}},
                    {{98,93},{87,93},{82,95},{82,97},{98,99}}
                };
            }
            else
            {
                Outline = {
                    {{98,13.5f},{96.6f,16},{95.3f,27},{95,40},{96,51},{100,51},{101,40},{100.7f,27},{99.4f,16}},
                    {{96,49},{97.5f,123},{98.5f,123},{100,49}},
                    {{98,47},{42,49},{28,52},{18.5f,60},{18.5f,65},{98,63}},
                    {{98,112},{78,120},{74.5f,123},{74.5f,128},{98,121}}
                };
            }
            // Reflect the left wing and tail about the fuselage centreline.
            for (int32 I : {2,3})
            {
                auto Mirrored = Outline[I];
                for (FVector2f& Point : Mirrored) Point.X = 196.f - Point.X;
                Outline.Add(MoveTemp(Mirrored));
            }
            SetCursor(EMouseCursor::Hand);
        }
        FVector2D ComputeDesiredSize(float) const override { return {320,150}; }
        int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Draw,
                      int32 Layer, const FWidgetStyle&, bool) const override
        {
            FSlateDrawElement::MakeBox(Draw,Layer,Geo.ToPaintGeometry(),Fill(),ESlateDrawEffect::None,Ink);
            const FVector2f Scale(Geo.GetLocalSize()/FVector2D(320,150));
            const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Fill());
            auto Silhouette = [&](FVector2f Origin,float Magnification,float Left,float Width)
            {
                Draw.PushClip(FSlateClippingZone(Geo.MakeChild(FVector2f(Width,150)*Scale,
                    FSlateLayoutTransform(FVector2f(Left,0)*Scale))));
                for (const auto& Polygon : Outline)
                {
                    TArray<FSlateVertex> Vertices;
                    TArray<SlateIndex> Indices;
                    for (const FVector2f Point : Polygon)
                        Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geo.GetAccumulatedRenderTransform(),
                            (Origin+(Point-FVector2f(98,56))*Magnification)*Scale,FVector2f(.5f,.5f),FColor(170,178,178)));
                    for (int32 I=1; I<Polygon.Num()-1; ++I)
                    {
                        Indices.Add(0); Indices.Add(I); Indices.Add(I+1);
                    }
                    FSlateDrawElement::MakeCustomVerts(Draw,Layer+1,Resource,Vertices,Indices,nullptr,0,0);
                }
                Draw.PopClip();
            };
            Silhouette({98,56},1.f,0,196);
            Silhouette({259,91},ZoomScale/FullScale,199,121);
            FSlateDrawElement::MakeLines(Draw,Layer+1,Geo.ToPaintGeometry(),
                {FVector2f(198,8)*Scale,FVector2f(198,142)*Scale},ESlateDrawEffect::None,Rule,true,1.f);
            auto Mark = [&](FVector2f Origin, float PixelsPerMm)
            {
                const FVector2f Active = Origin + FVector2f(0,Offset.Get()*PixelsPerMm);
                auto Ring = [&](FVector2f Centre,float Radius,FLinearColor Color,float Width)
                {
                    TArray<FVector2f> Points;
                    for (int32 I=0; I<=24; ++I)
                    {
                        const double Angle=I*UE_TWO_PI/24.;
                        Points.Add((Centre+Radius*FVector2f(FMath::Cos(Angle),FMath::Sin(Angle)))*Scale);
                    }
                    FSlateDrawElement::MakeLines(Draw,Layer+2,Geo.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width);
                };
                Ring(Origin,5.5f,Ink,3.5f);
                Ring(Origin,5.5f,Paper,1.6f);
                Ring(Active,2.4f,Ink,5.f);
                Ring(Active,1.5f,Gold,3.f);
            };
            Mark({98,56},FullScale);
            Mark({259,91},ZoomScale);
            return Layer+2;
        }
        FReply OnMouseButtonDown(const FGeometry& Geo,const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
            const FVector2D P=Geo.AbsoluteToLocal(Event.GetScreenSpacePosition())/Geo.GetLocalSize()*FVector2D(320,150);
            const bool Zoom=P.X>=199;
            const float Origin=Zoom?91.f:56.f, Scale=Zoom?ZoomScale:FullScale;
            const double At=Origin+Offset.Get()*Scale;
            GrabY=P.Y;
            GrabOffset=FMath::Abs(P.Y-At)<6. ? Offset.Get() : FMath::Clamp(FMath::RoundToInt32((P.Y-Origin)/Scale),-10,10);
            OnMove.ExecuteIfBound(GrabOffset);
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
        FReply OnMouseMove(const FGeometry& Geo,const FPointerEvent& Event) override
        {
            if (!HasMouseCapture()) return FReply::Unhandled();
            const double Y=150.*Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()).Y/Geo.GetLocalSize().Y;
            OnMove.ExecuteIfBound(FMath::Clamp(GrabOffset+FMath::RoundToInt32((Y-GrabY)/ZoomScale),-10,10));
            return FReply::Handled();
        }
        FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& Event) override
        {
            if (HasMouseCapture() && Event.GetEffectingButton()==EKeys::LeftMouseButton)
                return FReply::Handled().ReleaseMouseCapture();
            return FReply::Unhandled();
        }
    private:
        static constexpr float ZoomScale=.8f;
        float FullScale=0.;
        double GrabY=0.;
        int32 GrabOffset=0;
        TArray<TArray<FVector2f>> Outline;
        TAttribute<int32> Offset;
        FSetOffset OnMove;
    };
}

void SFlightMenu::Construct(const FArguments& Args)
{
    Plane=Args._Plane;
    for (int32 I=0;I<int32(EControlGroup::Count);++I) Groups.Add(MakeShared<FString>(GroupLabel(EControlGroup(I))));
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),Version,GGameIni);
    TSharedRef<SVerticalBox> PanelContent=SNew(SVerticalBox);
    PanelContent->AddSlot().AutoHeight().Padding(32,30,32,0)
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(25).HeightOverride(3)[SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Gold)]]
        +SHorizontalBox::Slot().AutoWidth().Padding(10,0)[Text(TEXT("CLUB DE VOL DE PENTE"),10,Gold,TEXT("Bold"))]
    ];
    PanelContent->AddSlot().FillHeight(1).Padding(32,16,32,16).VAlign(VAlign_Center)
    [ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(Body,SBox) ] ];
    PanelContent->AddSlot().AutoHeight().Padding(32,0,32,23)
    [ Text(TEXT("PROTOTYPE   /   v")+Version,10,Light,TEXT("Bold")) ];

    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this]{
            const FVector2D Size=GetCachedGeometry().GetLocalSize();
            return Size.Y>0 ? float(FMath::Clamp(FMath::Min(Size.Y/720.,Size.X/1024.),.55,1.65)) : 1.f;
        })
        [
            SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Left)
            [ SNew(SBox).WidthOverride_Lambda([this]{return FOptionalSize(Page==EPage::Devices?680.f:390.f);})[SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Panel).Padding(0)[PanelContent]] ]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,28,25)
            [
                SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FLinearColor(.008f,.02f,.023f,.81f)).Padding(FMargin(15,9))
                [ SNew(SVerticalBox)
                  +SVerticalBox::Slot().AutoHeight()[Text(TEXT("SITE  01     /     VALLÉE ALPINE"),9,Gold,TEXT("Bold"))]
                  +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[Text(TEXT("La prairie du lac"),15,Paper,TEXT("Bold"))] ]
            ]
        ]
    ];
    Build();
}

void SFlightMenu::FocusFirst()
{
    if (First.IsValid()) FSlateApplication::Get().SetKeyboardFocus(First,EFocusCause::SetDirectly);
}

TSharedRef<SButton> SFlightMenu::Button(const FString& Name,const FString& Label,TFunction<void()> Click,bool Primary)
{
    TSharedRef<SButton> B=SNew(SButton).Tag(FName(*Name)).ButtonStyle(&Style(Primary)).ContentPadding(FMargin(14,9))
        .OnClicked_Lambda([Click]{Click();return FReply::Handled();})
        [ SNew(SHorizontalBox)
          +SHorizontalBox::Slot().FillWidth(1)[Text(Label,Primary?16:13,Primary?Ink:Paper,TEXT("Bold"))]
          +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(Primary?TEXT("→"):TEXT("›"),14,Primary?Ink:Light)] ];
    if (!First.IsValid()) First=B;
    return B;
}

TSharedRef<SWidget> SFlightMenu::Choice(const FString& Name,TFunction<FString()> Value,TFunction<void(int32)> Change,TAttribute<bool> Enabled)
{
    auto Arrow=[this,Change,Name,Enabled](const TCHAR* Label,int32 Step)
    {
        TSharedRef<SButton> B=SNew(SButton).Tag(FName(*(Name+(Step>0?TEXT("+"):TEXT("-"))))).ButtonStyle(&Style(false)).ContentPadding(FMargin(10,4))
              .OnClicked_Lambda([Change,Step]{Change(Step);return FReply::Handled();})[Text(Label,15,Gold,TEXT("Bold"))];
        if (!First.IsValid() && Enabled.Get()) First=B;
        return B;
    };
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,3)[Text(Name,10,Light,TEXT("Bold"))]
        +SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox).IsEnabled(Enabled)
         +SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("‹"),-1)]
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)[Live(Value,13)]
         +SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("›"),1)]];
}

void SFlightMenu::Go(EPage Target)
{
    CancelDisplayChange();
    if (Page==EPage::Devices) Plane->Radio.LeaveMenu();
    Page=Target;
    if (Page == EPage::Hangar)
    {
        DraftLoading = Plane->Loading();
        bDraftAileronSteering = Plane->UsesAileronSteering();
        DraftGains = Plane->ControlGains();
        bAdvancedModel = false;
    }
    Plane->ShowHangarModel(Page==EPage::Hangar);
    Message.Empty();
    if (Page==EPage::Devices) Plane->Radio.Scan();
    if (Page==EPage::Weather) DraftWeather=ValleyAir::Settings();
    if (Page==EPage::Graphics)
    {
        UGameUserSettings* G=GEngine->GetGameUserSettings();
        Mode=G->GetFullscreenMode(); Size=G->GetScreenResolution();
        Quality=G->GetOverallScalabilityLevel(); bVsync=G->IsVSyncEnabled();
        GrassDistance=ASlopeWorld::SavedGrassDistanceMeters();
        ShowGrass=true;
        GConfig->GetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowGrass"),ShowGrass,GGameUserSettingsIni);
        TrailDistance=Plane->TrailDistance;
        ShowTrails=Plane->TrailsVisible();
        FindSizes();
    }
    Build();
    FocusFirst();
}

void SFlightMenu::Build()
{
    First.Reset();
    TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    if (Page==EPage::Home) Home(*List);
    else
    {
        Add(*List,Text(TEXT("ÇA PLANE !     /     CLUB RC"),10,Gold,TEXT("Bold")),6);
        const FString Title=Page==EPage::Hangar ? TEXT("Le hangar") : Page==EPage::Devices ? TEXT("Périphériques") : Page==EPage::Weather ? TEXT("Météo") : TEXT("Graphismes");
        Add(*List,Text(Title,29,Paper,TEXT("Bold")),12);
        if (Page==EPage::Hangar) Hangar(*List);
        if (Page==EPage::Devices) Devices(*List);
        if (Page==EPage::Graphics) Graphics(*List);
        if (Page==EPage::Weather) Weather(*List);
        if (ConfirmUntil==0.) Add(*List,Button(TEXT("Back"),TEXT("Retour au menu"),[this]{Go(EPage::Home);}));
    }
    Body->SetContent(List);
}

void SFlightMenu::Home(SVerticalBox& List)
{
    Add(List,Text(TEXT("Ça plane !"),47,Paper,TEXT("BoldItalic")),2);
    Add(List,Text(TEXT("Simulateur de planeur radiocommandé"),13,Light),24);
    Add(List,Line(),14);
    Add(List,Text(TEXT("PLANEUR SÉLECTIONNÉ"),9,Gold,TEXT("Bold")),2);
    Add(List,Text(Plane->ModelName(),19,Paper,TEXT("Bold")),2);
    if (Plane->IsFlightReady())
    {
        const auto Setting = Plane->Loading();
        Add(List,Text(FString::Printf(TEXT("%d g   ·   Centrage %d mm"), Setting.MassGrams, Setting.CgMm),12,Light),4);
    }
    Add(List,Live([this]{
        if (!Plane->IsFlightReady()) return FString(TEXT("Fiche de vol introuvable ou invalide"));
        return FString(Plane->IsAppearanceReady()?TEXT("Prêt à voler"):TEXT("Chargement du planeur…"));
    },11,Light),12);
    auto FlyButton=Button(TEXT("Fly"),Plane->IsNoclip()?TEXT("Reprendre l’exploration"):(Plane->HasStartedFlight()?TEXT("Reprendre le vol"):TEXT("Prendre l’air")),[this]{Plane->Fly();},true);
    FlyButton->SetEnabled(TAttribute<bool>::CreateLambda([this]{ return Plane->IsFlightReady() && Plane->IsAppearanceReady(); }));
    Add(List,FlyButton,11);
    auto ExploreButton=Button(TEXT("Explore"),TEXT("Explorer la scène · noclip"),[this]{Plane->ExploreScene();});
    ExploreButton->SetEnabled(TAttribute<bool>::CreateLambda([this]{return Plane->IsFlightReady() && Plane->IsAppearanceReady();}));
    Add(List,ExploreButton,4);
    Add(List,Button(TEXT("Hangar"),TEXT("Choisir le planeur"),[this]{Go(EPage::Hangar);}),4);
    Add(List,Button(TEXT("Weather"),TEXT("Météo et ascendances"),[this]{Go(EPage::Weather);}),4);
    Add(List,Button(TEXT("Devices"),TEXT("Périphériques"),[this]{Go(EPage::Devices);}),4);
    Add(List,Button(TEXT("Graphics"),TEXT("Graphismes"),[this]{Go(EPage::Graphics);}),4);
    Add(List,Button(TEXT("Quit"),TEXT("Quitter"),[this]{
        if (auto* PC=Cast<APlayerController>(Plane->GetController())) UKismetSystemLibrary::QuitGame(Plane.Get(),PC,EQuitPreference::Quit,false);
    }));
}

void SFlightMenu::Hangar(SVerticalBox& List)
{
    Add(List,Line(),10);
    Add(List,Choice(TEXT("MODÈLE"),[this]{ return FString(Plane->ModelName()); },[this](int32 Step){
        Plane->BrowseModel(Step);
        DraftLoading = Plane->Loading();
        bDraftAileronSteering = Plane->UsesAileronSteering();
        DraftGains = Plane->ControlGains();
        Build();
        FocusFirst();
    }),6);
    Add(List,Live([this]{
        if (!Plane->IsAppearanceReady()) return FString(TEXT("Chargement du planeur…"));
        return FString(Plane->ModelIndex() == 0 ? TEXT("01 / 02   ·   1,19 m   ·   2 axes") :
            TEXT("02 / 02   ·   3,60 × 1,77 m   ·   79,1 dm²"));
    },12,Light),12);
    if (!Plane->CanSelectModel()) { Add(List,Text(TEXT("Modèle indisponible"),12,Gold)); return; }
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)
        [Button(TEXT("TabSetup"),TEXT("Réglages"),[this]{bAdvancedModel=false;Build();FocusFirst();},!bAdvancedModel)]
        +SHorizontalBox::Slot().FillWidth(1)
        [Button(TEXT("TabAdvanced"),TEXT("Avancé"),[this]{bAdvancedModel=true;Build();FocusFirst();},bAdvancedModel)],12);
    if (bAdvancedModel) AdvancedModel(List);
    else ModelSetup(List);
    if (Plane->HasStartedFlight()) Add(List,Text(TEXT("Modifier puis appliquer prépare un nouveau départ."),11,Gold),5);
    Add(List,Button(TEXT("SelectAircraft"),TEXT("Appliquer et sélectionner"),[this]{
        Plane->ApplyModelSettings(DraftLoading,bDraftAileronSteering,DraftGains); Go(EPage::Home);
    },true),5);
}

void SFlightMenu::ModelSetup(SVerticalBox& List)
{
    Add(List,Choice(TEXT("MANCHE POUR LES VIRAGES"),[this]{
        return FString(bDraftAileronSteering ? TEXT("Ailerons → direction") : TEXT("Direction → direction"));
    },[this](int32){ bDraftAileronSteering = !bDraftAileronSteering; }),4);
    Add(List,Text(Plane->ModelIndex()==0 ? TEXT("Topy · mixage en V avec la profondeur") :
        TEXT("Commande de la dérive du K3600"),11,Light),8);
    Add(List,Choice(TEXT("MASSE TOTALE"),[this]{ return FString::Printf(TEXT("%d g"), DraftLoading.MassGrams); },
        [this](int32 Step){ DraftLoading.MassGrams += 5 * Step; DraftLoading = Plane->LimitLoading(DraftLoading); }),6);
    Add(List,Choice(TEXT("CENTRAGE LONGITUDINAL"),[this]{ return FString::Printf(TEXT("%d mm"), DraftLoading.CgMm); },
        [this](int32 Step){ DraftLoading.CgMm += Step; DraftLoading = Plane->LimitLoading(DraftLoading); }),4);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.62f)[Text(TEXT("VUE DE DESSUS   ↑ NEZ"),10,Light)]
        +SHorizontalBox::Slot().FillWidth(.38f)[Text(TEXT("ZOOM CENTRAGE"),10,Light)],2);
    Add(List,SNew(SCgPlan).Tag(TEXT("CgPlan")).K3600(Plane->ModelIndex()==1)
        .OffsetMm_Lambda([this]{ return DraftLoading.CgMm-Plane->StandardLoading().CgMm; })
        .OnMove_Lambda([this](int32 Delta){ DraftLoading.CgMm=Plane->StandardLoading().CgMm+Delta; }),3);
    Add(List,Live([this]{
        const int32 Origin = Plane->StandardLoading().CgMm, Delta = DraftLoading.CgMm - Origin;
        const FString Shift = Delta ? FString::Printf(TEXT("%d mm vers %s"), FMath::Abs(Delta), Delta < 0 ? TEXT("le nez") : TEXT("la queue")) : TEXT("sur le repère d’origine");
        return FString::Printf(TEXT("Origine  %d mm   ·   %s"), Origin, *Shift);
    },12,Gold),4);
    Add(List,Text(TEXT("Glisser le point doré · cercle blanc d’origine."),11,Light),6);
    if (Plane->ModelIndex()==1) Add(List,Text(TEXT("Notice K3600  ·  101–106 mm du bord d’attaque"),11,Light),4);
    Add(List,Button(TEXT("DefaultCg"),TEXT("Rétablir le centrage d’origine"),[this]{ DraftLoading.CgMm = Plane->StandardLoading().CgMm; }),4);
}

void SFlightMenu::AdvancedModel(SVerticalBox& List)
{
    Add(List,Text(TEXT("FORCE DES COMMANDES"),12,Gold,TEXT("Bold")),5);
    Add(List,Text(TEXT("100 % référence · 0 % nul · négatif inversé"),11,Light),14);
    auto Gain = [&](FName ControlTag,const TCHAR* Label,TFunction<int32()> Read,TFunction<void(int32)> Write)
    {
        Add(List,Text(Label,11,Paper),5);
        Add(List,SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,12,0)
            [SNew(SSlider).MinValue(-500.f).MaxValue(500.f).StepSize(5.f).MouseUsesStep(true)
                .Value_Lambda([Read]{return float(Read());})
                .OnValueChanged_Lambda([Write](float Value){Write(FMath::Clamp(FMath::RoundToInt32(Value),-500,500));})
                .SliderHandleColor(Gold)]
            +SHorizontalBox::Slot().AutoWidth()
            [SNew(SSpinBox<int32>).Tag(ControlTag).MinDesiredWidth(76).MinValue(-500).MaxValue(500)
                .MinSliderValue(-500).MaxSliderValue(500).Delta(1)
                .Value_Lambda([Read]{return Read();})
                .OnValueChanged_Lambda([Write](int32 Value){Write(FMath::Clamp(Value,-500,500));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0,0,0)[Text(TEXT("%"),12,Light)],15);
    };
    Gain(TEXT("GainYaw"),TEXT("Direction / lacet"),[this]{return DraftGains.Yaw;},[this](int32 Value){DraftGains.Yaw=Value;});
    Gain(TEXT("GainRoll"),TEXT("Roulis induit"),[this]{return DraftGains.Roll;},[this](int32 Value){DraftGains.Roll=Value;});
    Gain(TEXT("GainPitch"),TEXT("Profondeur"),[this]{return DraftGains.Pitch;},[this](int32 Value){DraftGains.Pitch=Value;});
    auto Note=Text(TEXT("Sur ces modèles 2 axes, le manche de virage agit sur la direction et le roulis. Glisser les curseurs ou saisir un pourcentage."),11,Light);
    Note->SetAutoWrapText(true);
    Add(List,Note,12);
    Add(List,Button(TEXT("DefaultGains"),TEXT("Rétablir les forces à 100 %"),[this]{DraftGains={};}),12);
}

void SFlightMenu::Devices(SVerticalBox& List)
{
    using A=EGliderControl;
    using T=EControlType;
    FFlightRadio& R=Plane->Radio;
    const auto& Spec=ControlSpec(Control);
    const auto Available=TAttribute<bool>::CreateLambda([&R]{return R.CanEdit();});
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)
        [Choice(TEXT("PÉRIPHÉRIQUE"),[&R]{return R.SourceLabel();},[&R](int32 S){R.CycleDevice(S);})]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
        [Button(TEXT("Refresh"),TEXT("Rechercher"),[&R]{R.Scan();R.Notice=TEXT("Recherche terminée.");})],4);
    Add(List,Live([&R]{return R.Status();},11,Gold),12);

    auto Select=[this,&R](A Next){R.CancelLearn();Control=Next;Build();};
    auto Left=SNew(SVerticalBox);
    Add(*Left,Text(TEXT("FAMILLE"),10,Light,TEXT("Bold")),4);
    Add(*Left,SNew(STextComboBox).OptionsSource(&Groups).InitiallySelectedItem(Groups[int32(Group)])
        .OnSelectionChanged_Lambda([this,Select](TSharedPtr<FString> Item,ESelectInfo::Type){
            const int32 Index=Groups.IndexOfByKey(Item);
            if (Index<0 || Index==int32(Group)) return;
            Group=EControlGroup(Index);
            for (int32 I=0;I<ControlCount;++I) if (ControlSpec(A(I)).Group==Group) {Select(A(I));return;}
        }),8);
    for (int32 I=0;I<ControlCount;++I)
    {
        const A Item=A(I);
        const auto& S=ControlSpec(Item);
        if (S.Group==Group) Add(*Left,Button(S.Id,S.Label,[Select,Item]{Select(Item);},Item==Control),3);
    }

    auto Right=SNew(SVerticalBox);
    Add(*Right,Text(Plane->IsControlInUse(Control)?TEXT("EN SERVICE SUR CE PLANEUR"):TEXT("NON UTILISÉ SUR CE PLANEUR"),9,Gold,TEXT("Bold")),3);
    Add(*Right,Text(Spec.Label,17,Paper,TEXT("Bold")),6);
    auto Slot=[this,&R,Available,Right](bool Minus)
    {
        const auto Tag=Minus?TEXT("BindMinus"):TEXT("BindPlus");
        const auto Read=[this,&R,Minus]{const auto& B=R.Binding(Control);return R.InputLabel(Minus?B.Minus:B.Plus);};
        auto Assign=Button(Tag,TEXT("Attribuer"),[this,&R,Minus]{
            R.BeginLearn(Control,Minus);
            FSlateApplication::Get().SetKeyboardFocus(SharedThis(this),EFocusCause::SetDirectly);
        });
        Assign->SetEnabled(Available);
        auto Clear=SNew(SButton).ButtonStyle(&Style(false)).ContentPadding(FMargin(7,6))
            .ToolTipText(FText::FromString(TEXT("Effacer cette affectation"))).OnClicked_Lambda([this,&R,Minus]{
                R.Clear(Control,Minus);return FReply::Handled();})[Text(TEXT("×"),14,Light)];
        Clear->SetEnabled(TAttribute<bool>::CreateLambda([this,&R,Minus]{
            const auto& B=R.Binding(Control);return R.CanEdit() && !(Minus?B.Minus:B.Plus).IsNone();}));
        Add(*Right,SNew(SVerticalBox).Visibility_Lambda([this,&R,Minus]{
            return !Minus || R.SupportsMinus(Control)?EVisibility::Visible:EVisibility::Collapsed;})
            +SVerticalBox::Slot().AutoHeight()[Text(Minus?TEXT("SENS –  /  BOUTON OU TOUCHE"):TEXT("ENTRÉE PRINCIPALE  /  SENS +"),9,Light,TEXT("Bold"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Live(Read,13)]
             +SHorizontalBox::Slot().AutoWidth()[Assign]
             +SHorizontalBox::Slot().AutoWidth().Padding(3,0,0,0)[Clear]],7);
    };
    Slot(false);Slot(true);
    auto InvertButton=SNew(SButton).Tag(TEXT("InvertControl")).ButtonStyle(&Style(false)).ContentPadding(FMargin(10,5))
        .IsEnabled_Lambda([this,&R]{return R.CanEdit() && (!R.Binding(Control).Plus.IsNone() || !R.Binding(Control).Minus.IsNone());})
        .OnClicked_Lambda([this,&R]{R.CancelLearn();R.Invert(Control);return FReply::Handled();})
        [Live([this,&R]{return R.Binding(Control).Inverted?TEXT("Sens inversé   ⇄"):TEXT("Sens normal   ⇄");},12)];
    Add(*Right,InvertButton,4);
    if (Spec.Type==T::Switch || Spec.Type==T::Phase)
        Add(*Right,SNew(SButton).ButtonStyle(&Style(false)).ContentPadding(FMargin(10,5)).IsEnabled(Available)
            .OnClicked_Lambda([this,&R]{R.CancelLearn();R.ToggleMode(Control);return FReply::Handled();})
            [Live([this,&R]{
                if (ControlSpec(Control).Type==T::Phase) return R.Binding(Control).Toggle?TEXT("Boutons : parcourir les phases"):TEXT("Boutons : vitesse / neutre / thermique");
                return R.Binding(Control).Toggle?TEXT("Mode bascule · un appui inverse l’état"):TEXT("Mode maintenu · suit l’interrupteur");
            },11)],4);
    Add(*Right,Live([this,&R]{
        const float V=R.Preview(Control);
        switch (ControlSpec(Control).Type)
        {
            case T::Phase: return FString(V<-.5f?TEXT("Phase : vitesse"):V>.5f?TEXT("Phase : thermique"):TEXT("Phase : croisière"));
            case T::Pulse: case T::Switch: return FString(V>.5f?TEXT("Entrée active"):TEXT("Entrée au repos"));
            default: return FString::Printf(TEXT("Valeur   %+.0f %%"),V*100.f);
        }
    },12,Gold),3);
    Add(*Right,SNew(SBox).HeightOverride(6)[SNew(SProgressBar).Percent_Lambda([this,&R]{
        const auto Type=ControlSpec(Control).Type;
        const float V=R.Preview(Control);
        return TOptional<float>((Type==T::Signed || Type==T::Trim || Type==T::Phase)?(1.f+V)*.5f:V);
    }).FillColorAndOpacity(Gold)]);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.42f).Padding(0,0,16,0)[Left]
        +SHorizontalBox::Slot().FillWidth(.58f)[SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FLinearColor(.018f,.042f,.045f)).Padding(12)[Right]],8);
    auto MessageText=Live([&R]{
        if (!R.Notice.IsEmpty()) return R.Notice;
        return R.CanEdit()?FString(TEXT("Choisis une fonction, puis attribue un axe, un bouton ou une touche.")):
            FString(TEXT("Branche une radio ou sélectionne Clavier pour attribuer les touches."));
    },12,Light);
    MessageText->SetAutoWrapText(true);
    Add(List,MessageText,5);
    Add(List,SNew(SHorizontalBox).Visibility_Lambda([&R]{return R.Learning()?EVisibility::Visible:EVisibility::Collapsed;})
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)[SNew(SBox).Visibility_Lambda([&R]{return R.HasConflict()?EVisibility::Visible:EVisibility::Collapsed;})
            [Button(TEXT("MoveBinding"),TEXT("Déplacer"),[&R]{R.Resolve(true);})]]
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)[SNew(SBox).Visibility_Lambda([&R]{return R.HasConflict()?EVisibility::Visible:EVisibility::Collapsed;})
            [Button(TEXT("ShareBinding"),TEXT("Partager"),[&R]{R.Resolve(false);})]]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("CancelBinding"),TEXT("Annuler"),[&R]{R.CancelLearn();})],5);
    auto Center=Button(TEXT("Center"),TEXT("Centrer les manches de pilotage"),[&R]{
        R.CancelLearn();
        R.Notice=R.CenterSticks()?TEXT("Neutres enregistrés."):TEXT("Centre les axes de pilotage affectés, puis réessaie.");
    });
    Center->SetEnabled(TAttribute<bool>::CreateLambda([&R]{return R.HasDevice() && !R.Learning();}));
    Add(List,Center,4);
}

void SFlightMenu::FindSizes()
{
    Sizes.Reset();
    UGameUserSettings* G=GEngine->GetGameUserSettings();
    const FIntPoint Desktop=G->GetDesktopResolution();
    if (Mode==EWindowMode::WindowedFullscreen) { Size=Desktop; Sizes.Add(Size); return; }
    if (Mode==EWindowMode::Fullscreen) UKismetSystemLibrary::GetSupportedFullscreenResolutions(Sizes);
    else UKismetSystemLibrary::GetConvenientWindowedResolutions(Sizes);
    if (Mode==EWindowMode::Windowed || Sizes.IsEmpty())
    {
        for (FIntPoint P : {FIntPoint(1024,576),FIntPoint(1280,720),FIntPoint(1280,800),FIntPoint(1600,900),FIntPoint(1920,1080),Desktop})
            if (P.X<=Desktop.X && P.Y<=Desktop.Y) Sizes.AddUnique(P);
    }
    Sizes.RemoveAll([](FIntPoint P){return P.X<800 || P.Y<500;});
    if (Mode==EWindowMode::Windowed && Size.X>=800 && Size.Y>=500 && Size.X<=Desktop.X && Size.Y<=Desktop.Y) Sizes.AddUnique(Size);
    if (Sizes.IsEmpty()) Sizes.Add(Desktop);
    Sizes.Sort([](FIntPoint A,FIntPoint B){return A.X==B.X ? A.Y<B.Y : A.X<B.X;});
    if (!Sizes.Contains(Size)) Size=Sizes.Last();
}

void SFlightMenu::Graphics(SVerticalBox& List)
{
    if (ConfirmUntil>0.)
    {
        Add(List,Text(TEXT("Conserver cet affichage ?"),17,Paper,TEXT("Bold")),8);
        Add(List,Live([this]{return FString::Printf(TEXT("Rétablissement dans %d s"),FMath::Max(0,FMath::CeilToInt(ConfirmUntil-FPlatformTime::Seconds())));},13,Light),18);
        Add(List,Button(TEXT("Confirm"),TEXT("Conserver"),[this]{SaveGraphics();},true),5);
        Add(List,Button(TEXT("Revert"),TEXT("Rétablir"),[this]{Go(EPage::Graphics);}));
        return;
    }
    Add(List,Choice(TEXT("MODE D’AFFICHAGE"),[this]{return FString(ModeNames[Mode]);},
        [this](int32 S){Mode=EWindowMode::Type((int(Mode)+S+3)%3);FindSizes();Message.Empty();}),10);
    Add(List,Choice(TEXT("RÉSOLUTION"),[this]{return Dimensions(Size);},
        [this](int32 S){Size=Sizes[(Sizes.IndexOfByKey(Size)+S+Sizes.Num())%Sizes.Num()];Message.Empty();},
        TAttribute<bool>::CreateLambda([this]{return Mode!=EWindowMode::WindowedFullscreen && Sizes.Num()>1;})),10);
    Add(List,Choice(TEXT("QUALITÉ GRAPHIQUE"),[this]{return Quality<0?FString(TEXT("Personnalisée")):FString(QualityNames[Quality]);},
        [this](int32 S){Quality=Quality<0?2:(Quality+S+5)%5;Message.Empty();}),10);
    Add(List,Choice(TEXT("SYNCHRONISATION VERTICALE"),[this]{return FString(bVsync?TEXT("Activée"):TEXT("Désactivée"));},
        [this](int32){bVsync=!bVsync;Message.Empty();}),12);
    auto RangeRow=[this,&List](const TCHAR* Label,FName SliderTag,bool& Visible,int32& Meters)
    {
        Add(List,Text(Label,12,Light,TEXT("Bold")),3);
        Add(List,SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)
            [SNew(SCheckBox).IsChecked_Lambda([&Visible]{return Visible?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,&Visible](ECheckBoxState State){Visible=State==ECheckBoxState::Checked; Message.Empty();})]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(SSlider).Tag(SliderTag).MinValue(20.f).MaxValue(500.f).StepSize(1.f).MouseUsesStep(true)
                .IsEnabled_Lambda([&Visible]{return Visible;}).Value_Lambda([&Meters]{return float(Meters);})
                .OnValueChanged_Lambda([this,&Meters](float Value){Meters=FMath::Clamp(FMath::RoundToInt(Value),20,500);Message.Empty();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(10,0,0,0)
            [Live([&Meters]{return FString::Printf(TEXT("%d m"),Meters);},13)],12);
    };
    RangeRow(TEXT("DISTANCE DE L’HERBE"),TEXT("GrassRange"),ShowGrass,GrassDistance);
    RangeRow(TEXT("DISTANCE DES FILETS D’AIR"),TEXT("TrailRange"),ShowTrails,TrailDistance);
    Add(List,Live([this]{return !Message.IsEmpty()?Message:Mode==EWindowMode::WindowedFullscreen?FString(TEXT("Sans bordure : résolution du bureau.")):FString();},11,Light),6);
    Add(List,Button(TEXT("Apply"),TEXT("Appliquer"),[this]{Apply();},true),5);
}

void SFlightMenu::Weather(SVerticalBox& List)
{
    Add(List,Choice(TEXT("HEURE SOLAIRE"),[this]{return FString::Printf(TEXT("%02d h %02d"),DraftWeather.SolarMinutes/60,DraftWeather.SolarMinutes%60);},
        [this](int32 Step){DraftWeather.SolarMinutes=(DraftWeather.SolarMinutes+Step*30+1440)%1440;Message.Empty();}),10);
    Add(List,Choice(TEXT("ÉTAT DU CIEL"),[this]{
        const TCHAR* Names[]={TEXT("Dégagé"),TEXT("Éclaircies"),TEXT("Couvert"),TEXT("Brumeux")};
        return FString(Names[DraftWeather.Sky]);},
        [this](int32 Step){DraftWeather.Sky=(DraftWeather.Sky+Step+4)%4;Message.Empty();}),10);
    Add(List,Choice(TEXT("FORCE DU VENT"),[this]{return FString::Printf(TEXT("%.1f m/s · %.0f km/h"),DraftWeather.WindSpeedMps,3.6*DraftWeather.WindSpeedMps);},
        [this](int32 S){DraftWeather.WindSpeedMps=FMath::Clamp(DraftWeather.WindSpeedMps+.5*S,0.,10.);Message.Empty();}),10);
    Add(List,Choice(TEXT("VENT VENANT DU"),[this]{return FString::Printf(TEXT("%03d°"),FMath::RoundToInt(DraftWeather.WindFromDegrees)%360);},
        [this](int32 S){
            const int32 Step=S>0 ? FMath::FloorToInt(DraftWeather.WindFromDegrees/15.)+1 : FMath::CeilToInt(DraftWeather.WindFromDegrees/15.)-1;
            DraftWeather.WindFromDegrees=(Step*15+360)%360;Message.Empty();
        }),5);
    Add(List,Text(TEXT("Provenance : 0° nord · 90° est · 180° sud."),11,Light),12);
    Add(List,Choice(TEXT("ACTIVITÉ THERMIQUE"),[this]{return FString::Printf(TEXT("%d %%"),FMath::RoundToInt(DraftWeather.ThermalStrength*100));},
        [this](int32 S){DraftWeather.ThermalStrength=FMath::Clamp(DraftWeather.ThermalStrength+.25*S,0.,2.);Message.Empty();}),10);
    Add(List,Choice(TEXT("TURBULENCE"),[this]{return FString::Printf(TEXT("%d %%"),FMath::RoundToInt(DraftWeather.Turbulence*100));},
        [this](int32 S){DraftWeather.Turbulence=FMath::Clamp(DraftWeather.Turbulence+.1*S,0.,1.);Message.Empty();}),12);
    Add(List,Text(TEXT("La pente porte du côté exposé au vent.\nLes thermiques dépendent du sol et de l’altitude."),11,Light),10);
    Add(List,Live([this]{return Message;},11,Gold),6);
    Add(List,Button(TEXT("DefaultWeather"),TEXT("Rétablir la météo de référence"),[this]{
        DraftWeather=ValleyAir::FWeatherSettings{};Message=TEXT("Valeurs de référence prêtes à appliquer.");
    }),5);
    Add(List,Button(TEXT("ApplyWeather"),TEXT("Appliquer la météo"),[this]{
        ValleyAir::SetSettings(DraftWeather);
        ValleyAir::SaveSettings();
        DraftWeather=ValleyAir::Settings();
        Message=TEXT("Météo appliquée et enregistrée.");
    },true),5);
}

void SFlightMenu::Apply()
{
    UGameUserSettings* G=GEngine->GetGameUserSettings();
    PreviousSize=G->GetScreenResolution();
    PreviousMode=G->GetFullscreenMode();
    if (Mode==PreviousMode && (Size==PreviousSize || Mode==EWindowMode::WindowedFullscreen)) { SaveGraphics(); return; }
    G->SetFullscreenMode(Mode);
    G->SetScreenResolution(Size);
    G->ApplyResolutionSettings(false);
    ConfirmUntil=FPlatformTime::Seconds()+15.;
    Build(); FocusFirst();
}

void SFlightMenu::SaveGraphics()
{
    UGameUserSettings* G=GEngine->GetGameUserSettings();
    ConfirmUntil=0.;
    if (Quality>=0 && Quality!=G->GetOverallScalabilityLevel()) G->SetOverallScalabilityLevel(Quality);
    G->SetVSyncEnabled(bVsync);
    G->ApplyNonResolutionSettings();
    G->ConfirmVideoMode();
    G->SaveSettings();
    Plane->ShowTrails(ShowTrails);
    Plane->TrailDistance=TrailDistance;
    GConfig->SetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowTrails"),ShowTrails,GGameUserSettingsIni);
    GConfig->SetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowGrass"),ShowGrass,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("SimPlaneur.Graphics"),TEXT("TrailDistance"),TrailDistance,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("SimPlaneur.Graphics"),TEXT("GrassDistanceMeters"),GrassDistance,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    for (TActorIterator<ASlopeWorld> World(Plane->GetWorld()); World; ++World)
        World->SetGrassDistanceMeters(GrassDistance,ShowGrass);
    Message=TEXT("Réglages enregistrés.");
    Build(); FocusFirst();
}

void SFlightMenu::CancelDisplayChange()
{
    if (!ConfirmUntil || !GEngine) return;
    ConfirmUntil=0.;
    UGameUserSettings* G=GEngine->GetGameUserSettings();
    G->SetFullscreenMode(PreviousMode);
    G->SetScreenResolution(PreviousSize);
    G->ApplyResolutionSettings(false);
}

void SFlightMenu::Tick(const FGeometry& Geometry,double Time,float Delta)
{
    SCompoundWidget::Tick(Geometry,Time,Delta);
    if (ConfirmUntil>0. && FPlatformTime::Seconds()>=ConfirmUntil) Go(EPage::Graphics);
}

FReply SFlightMenu::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Key)
{
    if (Page==EPage::Devices)
    {
        auto& R=Plane->Radio;
        if (Key.GetKey()==EKeys::Escape && R.Learning()) {R.CancelLearn();return FReply::Handled();}
        if (Key.GetKey()!=EKeys::Escape && !Key.GetKey().IsGamepadKey())
        {
            const bool Capture=R.EditingKeyboard() && R.Learning() && !R.HasConflict();
            if (!Key.IsRepeat()) R.MenuKey(Key.GetKey().GetFName(),true);
            if (Capture) return FReply::Handled();
        }
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry,Key);
}

FReply SFlightMenu::OnKeyUp(const FGeometry& Geometry,const FKeyEvent& Key)
{
    if (Page==EPage::Devices) Plane->Radio.MenuKey(Key.GetKey().GetFName(),false);
    return SCompoundWidget::OnKeyUp(Geometry,Key);
}

FReply SFlightMenu::OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Key)
{
    if (Key.GetKey()==EKeys::Escape && !Key.IsRepeat())
    {
        if (ConfirmUntil>0.) Go(EPage::Graphics);
        else if (Page!=EPage::Home) Go(EPage::Home);
        else if (Plane->HasStartedFlight()) Plane->Fly();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry,Key);
}
