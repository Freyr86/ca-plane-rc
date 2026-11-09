#include "FlightMenu.h"
#include "FlightMap.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
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
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor Ink(.009f,.078f,.076f), Paper(1.f,.886f,.65f), Light(.81f,.84f,.71f), Gold(.91f,.63f,.29f);
    const FLinearColor Coral(.85f,.142f,.077f), Panel(.008f,.068f,.066f,.97f), Rule(.71f,.53f,.29f,.55f);
    const FLinearColor MutedInk(.065f,.145f,.13f), Rust(.32f,.061f,.028f), Sand(.91f,.76f,.49f);
    const FSlateBrush* Fill() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
    FText T(const FString& Value) { return FText::FromString(Value); }

    TSharedRef<STextBlock> Text(const FString& Value,int32 Size,FLinearColor Color=Ink,FName Face=TEXT("Regular"))
    {
        return SNew(STextBlock).Text(T(Value)).Font(FCoreStyle::GetDefaultFontStyle(Face,Size)).ColorAndOpacity(Color);
    }
    TSharedRef<STextBlock> Live(TFunction<FString()> Read,int32 Size,FLinearColor Color=Ink)
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
    const FButtonStyle& Style(bool Primary,bool OnDark=false)
    {
        static const auto Make=[](FLinearColor Normal,FLinearColor Hover,FLinearColor Press)
        {
            FButtonStyle S;
            S.SetNormal(FSlateColorBrush(Normal)).SetHovered(FSlateColorBrush(Hover)).SetPressed(FSlateColorBrush(Press));
            S.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
            return S;
        };
        static const FButtonStyle Main=Make(Coral,FLinearColor(.98f,.24f,.13f),FLinearColor(.71f,.11f,.065f));
        static const FButtonStyle Other=Make(FLinearColor(.05f,.083f,.087f,.91f),FLinearColor(.105f,.17f,.174f),FLinearColor(.08f,.135f,.14f));
        static const FButtonStyle Field=Make(Sand,Gold,FLinearColor(.88f,.63f,.32f));
        return Primary ? Main : OnDark ? Other : Field;
    }
    const FSpinBoxStyle& NumberFieldStyle()
    {
        static const FSpinBoxStyle Field=[]{
            auto S=FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox");
            S.SetBackgroundBrush(FSlateColorBrush(Sand)).SetHoveredBackgroundBrush(FSlateColorBrush(Gold))
                .SetActiveBackgroundBrush(FSlateColorBrush(Gold)).SetInactiveFillBrush(FSlateColorBrush(Sand))
                .SetHoveredFillBrush(FSlateColorBrush(Gold)).SetActiveFillBrush(FSlateColorBrush(Gold))
                .SetForegroundColor(Ink);
            S.ArrowsImage.TintColor=Ink;
            return S;
        }();
        return Field;
    }
    const FCheckBoxStyle& CheckStyle()
    {
        static const FCheckBoxStyle Check=[]{
            auto S=FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox");
            const FVector2f Size(18,18);
            S.SetBackgroundImage(FSlateRoundedBoxBrush(Paper,2.f,Ink,1.f,Size))
                .SetBackgroundHoveredImage(FSlateRoundedBoxBrush(Gold,2.f,Ink,1.f,Size))
                .SetBackgroundPressedImage(FSlateRoundedBoxBrush(Sand,2.f,Ink,1.f,Size));
            S.CheckedImage.TintColor=Ink;
            S.CheckedHoveredImage.TintColor=Ink;
            S.CheckedPressedImage.TintColor=Ink;
            return S;
        }();
        return Check;
    }
    // Small vector ornaments stay sharp at every window size.
    class SPrintMark : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SPrintMark) {} SLATE_ARGUMENT(FName, Mark) SLATE_ARGUMENT(FLinearColor, Tint) SLATE_END_ARGS()
        void Construct(const FArguments& A){Mark=A._Mark;Tint=A._Tint;SetVisibility(EVisibility::HitTestInvisible);}
        FVector2D ComputeDesiredSize(float) const override {return {32,32};}
        int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& D,int32 L,const FWidgetStyle&,bool) const override
        {
            const FVector2f Scale(G.GetLocalSize()/32.);
            if (Scale.X <= 0 || Scale.Y <= 0) return L;
            auto Lines=[&](TArray<FVector2f> P,float W=1.8f){for(auto& V:P)V*=Scale;
                FSlateDrawElement::MakeLines(D,L,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Tint,true,W);};
            if(Mark=="Crest"){
                TArray<FVector2f> Arch={{1,30},{1,16}};
                for(int I=0;I<=32;++I){float A=PI*I/32.f;Arch.Add({16-15*FMath::Cos(A),16-15*FMath::Sin(A)});}
                Arch.Append({{31,30},{1,30}}); Lines(Arch,1.5f);
                Lines({{2,27},{9,19},{12,22},{19,16},{30,27}},1.5f);
                Lines({{4,28},{10,24},{13,26},{20,21},{27,28}},1.f);
                // Overhead sailplane: a tapered nose projects ahead of the main
                // wing, and a separate tailplane sits at the end of the boom.
                Lines({{19,4},{18.5f,9},{27,15},{18,11},{16,17},{18.5f,19},{15,18},{12,18},{15,16},{16.5f,10},{6,8},{17.5f,8.8f},{19,4}},1.5f);
            }else if(Mark=="Fan"){
                for(int I=0;I<=10;++I){const float A=PI*I/10.f;Lines({{16,29},{16+15*FMath::Cos(A),29-23*FMath::Sin(A)}},1.3f);}
                Lines({{1,29},{31,29}});
            }else if(Mark=="Compass"){
                Lines({{16,1},{20,13},{30,16},{20,19},{16,31},{12,19},{2,16},{12,13},{16,1}});
                Lines({{16,1},{16,31}},1); Lines({{2,16},{30,16}},1);
            }else if(Mark=="Fly") Lines({{2,18},{14,14},{24,4},{21,13},{30,11},{31,13},{20,18},{15,28},{15,21},{8,24},{11,19},{2,18}});
            else if(Mark=="Hangar"){Lines({{2,14},{16,5},{30,14},{28,14},{28,27},{4,27},{4,14},{2,14}});Lines({{9,26},{9,16},{23,16},{23,26}});}
            else if(Mark=="OpenMap"){Lines({{2,8},{11,5},{21,9},{30,5},{30,25},{21,29},{11,25},{2,28},{2,8}});Lines({{11,5},{11,25}},1);Lines({{21,9},{21,29}},1);}
            else if(Mark=="Weather"){Lines({{6,24},{3,22},{3,18},{6,15},{10,15},{12,10},{16,8},{20,9},{23,14},{27,15},{29,18},{28,22},{25,24},{6,24}});Lines({{4,28},{20,28}},1);}
            else if(Mark=="Settings"){for(int I=0;I<3;++I){float Y=7+I*9,U=I==1?20:10;Lines({{3,Y},{29,Y}},1);Lines({{U,Y-3},{U,Y+3}},3);}}
            else if(Mark=="Credits"){Lines({{3,6},{10,5},{16,8},{22,5},{29,6},{29,25},{22,24},{16,27},{10,24},{3,25},{3,6}});Lines({{16,8},{16,27}},1.3f);}
            else if(Mark=="Tutorial"){Lines({{2,12},{16,5},{30,12},{16,19},{2,12}});Lines({{7,15},{7,23},{16,27},{25,23},{25,15}});Lines({{30,12},{30,24}},1.3f);}
            else {Lines({{16,2},{16,15}},2.5f);TArray<FVector2f> P;for(int I=0;I<=24;++I){float A=-.9f+4.95f*I/24;P.Add({16+11*FMath::Cos(A),17+11*FMath::Sin(A)});}Lines(P,2.2f);}
            return L;
        }
    private:
        FName Mark;
        FLinearColor Tint;
    };
    FString Dimensions(FIntPoint P) { return FString::Printf(TEXT("%d × %d"),P.X,P.Y); }
    const TCHAR* ModeNames[]={TEXT("Plein écran"),TEXT("Sans bordure"),TEXT("Fenêtré")};
    const TCHAR* QualityNames[]={TEXT("Faible"),TEXT("Moyenne"),TEXT("Élevée"),TEXT("Ultra"),TEXT("Cinéma")};

    // Simplified plan outlines in a 320 x 150 reference frame, nose up.
    // The full view and the detail share the same geometry and CG datum.
    class SCgPlan : public SLeafWidget
    {
    public:
        DECLARE_DELEGATE_OneParam(FSetOffset, int32);
        SLATE_BEGIN_ARGS(SCgPlan) {} SLATE_ARGUMENT(int32, ModelIndex) SLATE_ARGUMENT(double, SpanCm) SLATE_ATTRIBUTE(int32, OffsetMm) SLATE_EVENT(FSetOffset, OnMove) SLATE_END_ARGS()
        void Construct(const FArguments& Args)
        {
            Offset = Args._OffsetMm;
            OnMove = Args._OnMove;
            FullScale = 159.f / (Args._SpanCm * 10.);
            if (Args._ModelIndex == 8)
            {
                // Project the Nubis planform around its 90 mm simulated CG.
                Outline = {
                    {{98,34.6f},{97,37},{96.4f,47},{96.3f,58},{97,65},{99,65},{99.7f,58},{99.6f,47},{99,37}},
                    {{97,64},{97.7f,93.7f},{98.3f,93.7f},{99,64}},
                    {{98,53.6f},{82,53.5f},{64.6f,53.6f},{49.9f,54.2f},{38.7f,55.1f},{31.2f,56.1f},{26.4f,57.2f},{21.6f,59.1f},{18.5f,61.5f},{18.5f,61.7f},{26.4f,59.5f},{38.7f,59},{64.6f,59.6f},{98,60.4f}},
                    {{98,85.2f},{93.2f,85.2f},{88.9f,85.6f},{86.2f,86.4f},{84.6f,88.1f},{85,89},{88.9f,90.3f},{98,91.2f}}
                };
            }
            else if (Args._ModelIndex == 7)
            {
                Outline = {
                    {{98,18.3f},{96.4f,23},{95.6f,42},{96.1f,59},{98,69},{99.9f,59},{100.4f,42},{99.6f,23}},
                    {{96.4f,60},{97.5f,135},{98.5f,135},{99.6f,60}},
                    {{98,48.9f},{50,49.2f},{28,51},{18.5f,55.3f},{19,60},{35,63},{65,67.3f},{98,69.0f}},
                    {{98,120.8f},{89,121.4f},{81,123.6f},{81,126},{98,128.8f}}
                };
            }
            else if (Args._ModelIndex == 6)
            {
                Outline = {{{98,40.2f},{88.8f,44.5f},{29,72.2f},{18.5f,80.6f},{19,84},{45,76},{79,65},{98,63.5f}},
                           {{98,40.2f},{107.2f,44.5f},{167,72.2f},{177.5f,80.6f},{177,84},{151,76},{117,65},{98,63.5f}}};
            }
            else if (Args._ModelIndex == 5)
            {
                Outline = {
                    {{98,28},{96.8f,34},{96,47},{96.7f,60},{99.3f,60},{100,47},{99.2f,34}},
                    {{96.7f,58},{97.6f,140},{98.4f,140},{99.3f,58}},
                    {{98,49},{61,49},{39,52},{22,56},{18.5f,60},{21,62},{51,65},{98,67}},
                    {{98,124},{88,125},{82,128},{82,131},{98,133}}
                };
            }
            else if (Args._ModelIndex == 4)
            {
                Outline = {
                    {{98,35},{96,38},{95.2f,46},{95.3f,56},{96.6f,63},{99.4f,63},{100.7f,56},{100.8f,46},{100,38}},
                    {{96.6f,62},{97.6f,96},{98.4f,96},{99.4f,62}},
                    {{98,54},{57,54},{35,55},{21,57},{18.5f,60},{19,61},{34,60},{55,60},{98,61}},
                    {{98,90},{87,92},{86,94},{98,94}}
                };
            }
            else if (Args._ModelIndex == 3)
            {
                Outline = {
                    {{98,28},{95,31},{93.8f,40},{94,55},{95,65},{101,65},{102,55},{102.2f,40},{101,31}},
                    {{95,63},{97.5f,110},{98.5f,110},{101,63}},
                    {{98,52},{55,53},{22,57},{18.5f,58},{18.5f,60},{54,62},{98,65}},
                    {{98,98},{82,102},{80.3f,104},{81,106},{98,106}}
                };
            }
            else if (Args._ModelIndex == 2)
            {
                Outline = {
                    {{98,34},{97,37},{96.8f,54},{97.2f,60},{98.8f,60},{99.2f,54},{99,37}},
                    {{97.2f,59},{97.7f,106},{98.3f,106},{98.8f,59}},
                    {{98,51},{60,51.5f},{37,53},{23,55.5f},{18.5f,58},{22,60},{46,62},{98,64}},
                    {{98,101},{87,102},{83,104},{82.5f,106},{88,107},{98,107.5f}}
                };
            }
            else if (Args._ModelIndex == 1)
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
                if (!Outline.IsValidIndex(I)) break;
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
    if (auto* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Slate/Alpine/I2-Manche-a-air.png")))
        Windsock = FDeferredCleanupSlateBrush::CreateBrush(Texture);
    if(auto* Texture=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Slate/Alpine/Wordmark.png")))
    {
        Masthead=FDeferredCleanupSlateBrush::CreateBrush(Texture);
        Wordmark=*Masthead->GetSlateBrush();
        Wordmark.SetUVRegion(FBox2f({.267f,.13f},{1.f,.86f}));
    }
    auto HomeOnly=[this]{return Page==EPage::Home?EVisibility::Visible:EVisibility::Collapsed;};
    TSharedRef<SVerticalBox> PanelContent=SNew(SVerticalBox);
    PanelContent->AddSlot().AutoHeight()
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(34).HeightOverride(23)[SNew(SPrintMark).Mark("Fan").Tint(Ink)]]
        +SHorizontalBox::Slot().FillWidth(1).Padding(9,0).VAlign(VAlign_Center)[Text(TEXT("ÇA PLANE !    /    CLUB DE VOL DE PENTE"),10,Ink,TEXT("Bold"))]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(21).HeightOverride(21)[SNew(SPrintMark).Mark("Compass").Tint(Ink)]]
    ];
    PanelContent->AddSlot().AutoHeight().Padding(0,4,0,7)[Line(Ink)];
    PanelContent->AddSlot().FillHeight(1)
    [ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(Body,SBox) ] ];
    PanelContent->AddSlot().AutoHeight().Padding(0,5,0,0)[Line(Ink)];

    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this]{
            const FVector2D Size=GetCachedGeometry().GetLocalSize();
            return Size.Y>0 ? float(FMath::Clamp(FMath::Min(Size.Y/720.,Size.X/1024.),.55,1.65)) : 1.f;
        })
        [
            SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24,20,0,34)
            [ SNew(SBox).Visibility_Lambda([this]{return Page==EPage::Home?EVisibility::Collapsed:EVisibility::Visible;})
              .WidthOverride_Lambda([this]{return FOptionalSize(Page==EPage::Map?790.f:Page==EPage::Devices?680.f:398.f);})
              [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Gold).Padding(2)
               [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Paper).Padding(FMargin(13,9))[PanelContent]]] ]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24,16)
            [SNew(SBorder).Visibility_Lambda(HomeOnly).BorderImage(Fill()).BorderBackgroundColor(FLinearColor(.006f,.067f,.070f,.93f)).Padding(FMargin(15,8))
             [SNew(SVerticalBox)
              +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(79).HeightOverride(79)
                    [SNew(SImage).Image(Windsock ? Windsock->GetSlateBrush() : nullptr)]]
                +SHorizontalBox::Slot().AutoWidth().Padding(9,0,0,0)[SNew(SBox).WidthOverride(325).HeightOverride(106)[SNew(SImage).Image(&Wordmark)]]]
              +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("S I M U L A T E U R    D E    V O L    D E    P E N T E    R C"),9,Gold,TEXT("Bold"))]]]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24,12,0,22)
            [SNew(SBox).Visibility_Lambda(HomeOnly).WidthOverride(245)
             [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Gold).Padding(2)
              [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Paper).Padding(FMargin(9,7))
               [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(53).HeightOverride(30)[SNew(SPrintMark).Mark("Fan").Tint(Ink)]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Line(Ink)]
                +SVerticalBox::Slot().AutoHeight()[SAssignNew(TicketBody,SBox)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Line(Ink)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(32).HeightOverride(32)[SNew(SPrintMark).Mark("Compass").Tint(Ink)]]]]]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,24,76)
            [SNew(SBox).Visibility_Lambda(HomeOnly).WidthOverride(342)
             [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Gold).Padding(2)
              [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Panel).Padding(FMargin(12,7))
               [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Choice(TEXT("SITE DE VOL"),[this]{
                    const TCHAR* Names[]={TEXT("01  ·  Prairie du lac"),TEXT("02  ·  Chalet du versant"),TEXT("03  ·  Piste des Alpages"),TEXT("04  ·  Belvédère du cirque")};
                    return FString(Names[Plane->LaunchSiteNumber()-1]);
                },[this](int32 Step){Plane->SwitchLaunchSite(Step);})]
                +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Line(Gold)]
                +SVerticalBox::Slot().AutoHeight()[Live([this]{return FString(TEXT("MODÈLE    /    "))+Plane->ModelName();},12,Paper)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[Button(TEXT("Explore"),TEXT("Explorer le site"),[this]{Plane->ExploreScene();})]]]]]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(24,0,0,9)
            [SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Ink).Padding(FMargin(8,2))[Text(TEXT("ÇA PLANE !    /    v")+Version,10,Paper,TEXT("Bold"))]]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,28,25)
            [
                SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FLinearColor(.008f,.02f,.023f,.81f)).Padding(FMargin(15,9))
                [ SNew(SVerticalBox)
                  +SVerticalBox::Slot().AutoHeight()[Live([this]{return FString::Printf(TEXT("SITE  %02d     /     VALLÉE ALPINE"),Plane->LaunchSiteNumber());},9,Gold)]
                  +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[Live([this]{const FVector2D P=Plane->MapPositionXY();return FString::Printf(TEXT("X %+.0f m    Y %+.0f m"),P.X,P.Y);},14,Paper)] ]
            ]
        ]
    ];
    Build();
    Plane->ShowMenuPresentation(false,true);
    if (Args._ShowStartupPoster)
    {
        if (auto* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Slate/Alpine/C1-Prairie-du-lac.png")))
        {
            StartupPoster = FDeferredCleanupSlateBrush::CreateBrush(Texture);
            HomeAfterStartup = ChildSlot.DetachWidget();
            ChildSlot
            [
                SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Ink).Padding(0)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                 [SNew(SImage).Image(StartupPoster->GetSlateBrush())]]
            ];
        }
    }
}

void SFlightMenu::FocusFirst()
{
    if (!HomeAfterStartup && First.IsValid()) FSlateApplication::Get().SetKeyboardFocus(First,EFocusCause::SetDirectly);
}

TSharedRef<SButton> SFlightMenu::Button(const FString& Name,const FString& Label,TFunction<void()> Click,bool Primary)
{
    const bool OnDark=Page==EPage::Home;
    const FLinearColor Color=OnDark || Primary ? Paper : Ink;
    TSharedRef<SButton> B=SNew(SButton).Tag(FName(*Name)).ButtonStyle(&Style(Primary,OnDark)).ContentPadding(FMargin(12,7))
        .OnClicked_Lambda([Click]{Click();return FReply::Handled();})
        [ SNew(SHorizontalBox)
          +SHorizontalBox::Slot().FillWidth(1)[Text(Label,Primary?16:13,Color,TEXT("BoldItalic"))]
          +SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0).VAlign(VAlign_Center)[Text(Primary?TEXT("→"):TEXT("›"),14,Color)] ];
    if (!First.IsValid()) First=B;
    return B;
}

TSharedRef<SWidget> SFlightMenu::Choice(const FString& Name,TFunction<FString()> Value,TFunction<void(int32)> Change,TAttribute<bool> Enabled)
{
    const bool OnDark=Page==EPage::Home;
    auto Arrow=[this,Change,Name,Enabled,OnDark](const TCHAR* Label,int32 Step)
    {
        TSharedRef<SButton> B=SNew(SButton).Tag(FName(*(Name+(Step>0?TEXT("+"):TEXT("-"))))).ButtonStyle(&Style(false,OnDark)).ContentPadding(FMargin(10,4))
              .OnClicked_Lambda([Change,Step]{Change(Step);return FReply::Handled();})[Text(Label,15,OnDark?Gold:Ink,TEXT("Bold"))];
        if (!First.IsValid() && Enabled.Get()) First=B;
        return B;
    };
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,3)[Text(Name,10,OnDark?Light:MutedInk,TEXT("Bold"))]
        +SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox).IsEnabled(Enabled)
         +SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("‹"),-1)]
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)[Live(Value,13,OnDark?Paper:Ink)]
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
        DraftFlaps = Plane->FlapTravel();
        DraftMotor = Plane->MotorSettings();
        bAdvancedModel = false;
    }
    Plane->ShowMenuPresentation(Page==EPage::Hangar,Page==EPage::Home);
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
        const FString Title=Page==EPage::Hangar ? TEXT("Le hangar") : Page==EPage::Map ? TEXT("Carte de la vallée") : Page==EPage::Devices ? TEXT("Périphériques") : Page==EPage::Weather ? TEXT("Météo") : Page==EPage::Vario ? TEXT("Vario sonore") : Page==EPage::Settings ? TEXT("Réglages") : Page==EPage::Credits ? TEXT("Crédits") : Page==EPage::Equipment ? TEXT("Matériel de lancement") : Page==EPage::Tutorials ? TEXT("Mon 1er vol") : TEXT("Graphismes");
        Add(*List,Text(Title,27,Ink,TEXT("BoldItalic")),4);
        Add(*List,Line(Ink),10);
        if (Page==EPage::Hangar) Hangar(*List);
        if (Page==EPage::Devices) Devices(*List);
        if (Page==EPage::Graphics) Graphics(*List);
        if (Page==EPage::Weather) Weather(*List);
        if (Page==EPage::Vario) Vario(*List);
        if (Page==EPage::Map) Map(*List);
        if (Page==EPage::Tutorials) Tutorials(*List);
        if (Page==EPage::Equipment)
        {
            Add(*List,Text(TEXT("ÉLASTIQUE / TREUIL"),12,MutedInk,TEXT("Bold")),8);
            Add(*List,Choice(TEXT("COULEUR"),[this]{return Plane->BungeeColourName();},
                [this](int32 Step){Plane->CycleBungeeColour(Step);}),12);
            Add(*List,SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Ink).Padding(14)
                [SNew(SBox).HeightOverride(12)[SNew(SBorder).BorderImage(Fill())
                    .BorderBackgroundColor_Lambda([this]{return Plane->BungeeColour();})]],14);
            auto ColourHint=Text(TEXT("La couleur est conservée après rangement et au prochain démarrage."),12,MutedInk);
            ColourHint->SetAutoWrapText(true);
            Add(*List,ColourHint,16);
            auto ReleaseHint=Text(FString::Printf(TEXT("Largage automatique à %.0f° d’élévation de la ligne ou à sa détente."),FBungeeLaunch::ReleaseAngleDegrees),12,MutedInk);
            ReleaseHint->SetAutoWrapText(true);
            Add(*List,ReleaseHint,14);
            Add(*List,Choice(TEXT("PUISSANCE MAX. DU TREUIL"),[this]{return FString::Printf(TEXT("%.0f %%"),Plane->WinchPowerLimit()*100.f);},
                [this](int32 Step){Plane->SetWinchPowerLimit(Plane->WinchPowerLimit()+Step*.05f);}),14);
            auto WinchHint=Text(TEXT("F1 : treuil · F4 : élastique. Pose la machine puis le départ avec E (40 à 400 m pour le treuil). Espace démarre une traction progressive ; E largue le crochet. Sans voie affectée, le treuil fonctionne automatiquement. Pour doser la traction, affecte « Puissance du treuil » dans les périphériques."),12,MutedInk);
            WinchHint->SetAutoWrapText(true);
            Add(*List,WinchHint,14);
        }
        if (Page==EPage::Credits)
        {
            Add(*List,SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Ink).Padding(FMargin(15,15))
                [SNew(SHorizontalBox)
                 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(SBox).WidthOverride(45).HeightOverride(48)[SNew(SPrintMark).Mark("Crest").Tint(Gold)]]
                 +SHorizontalBox::Slot().FillWidth(1).Padding(14,0,0,0).VAlign(VAlign_Center)
                    [SNew(SVerticalBox)
                     +SVerticalBox::Slot().AutoHeight()[Text(TEXT("CONCEPTION DU SIMULATEUR"),10,Gold,TEXT("Bold"))]
                     +SVerticalBox::Slot().AutoHeight()[Text(TEXT("Freyr86"),24,Paper,TEXT("BoldItalic"))]]],15);
            Add(*List,Text(TEXT("Remerciements"),16,Ink,TEXT("BoldItalic")),5);
            Add(*List,Text(TEXT("VJX · CONCEPTION ET CONTRIBUTIONS"),11,MutedInk,TEXT("Bold")),8);
            Add(*List,Text(TEXT("Jochen Günzel"),18,Ink,TEXT("BoldItalic")),3);
            auto VjxDesigner=Text(TEXT("Conception du VJX et autorisation d’utiliser et d’adapter les ressources du projet dans Ça plane !."),12,MutedInk);
            VjxDesigner->SetAutoWrapText(true);
            Add(*List,VjxDesigner,10);
            Add(*List,Text(TEXT("Mario Perner"),18,Ink,TEXT("BoldItalic")),3);
            auto VjxContact=Text(TEXT("Mise en relation avec Jochen et échanges autour du projet VJX."),12,MutedInk);
            VjxContact->SetAutoWrapText(true);
            Add(*List,VjxContact,10);
            auto VjxThanks=Text(TEXT("Merci à tous les deux pour votre disponibilité, vos réponses bienveillantes et votre soutien à l’intégration du VJX dans le simulateur."),12,Ink);
            VjxThanks->SetAutoWrapText(true);
            Add(*List,VjxThanks,15);
            Add(*List,Text(TEXT("Merci de faire voler Ça plane !"),13,MutedInk),15);
        }
        if (Page==EPage::Settings)
        {
            Add(*List,Button(TEXT("Devices"),TEXT("Radio et périphériques"),[this]{Go(EPage::Devices);}),6);
            Add(*List,Button(TEXT("Graphics"),TEXT("Graphismes et affichage"),[this]{Go(EPage::Graphics);}),6);
            Add(*List,Button(TEXT("Vario"),TEXT("Vario sonore"),[this]{Go(EPage::Vario);}),6);
            Add(*List,Button(TEXT("Equipment"),TEXT("Matériel de lancement"),[this]{Go(EPage::Equipment);}),12);
        }
        if (ConfirmUntil==0.) Add(*List,Button(TEXT("Back"),Page==EPage::Map && bMapFromFlight?TEXT("Reprendre la session"):TEXT("Retour au menu"),[this]{if(Page==EPage::Map) CloseMap(); else Go(EPage::Home);}));
    }
    if(Page==EPage::Home) {Body->SetContent(SNullWidget::NullWidget);TicketBody->SetContent(List);}
    else {TicketBody->SetContent(SNullWidget::NullWidget);Body->SetContent(List);}
}

void SFlightMenu::Home(SVerticalBox& List)
{
    auto Row=[this,&List](FName RowTag,const TCHAR* Name,TFunction<void()> Action,bool Primary=false)
    {
        static const FButtonStyle Ticket=FButtonStyle().SetNormal(FSlateColorBrush(Paper)).SetHovered(FSlateColorBrush(Gold)).SetPressed(FSlateColorBrush(FLinearColor(.88f,.63f,.32f)));
        const FLinearColor Color=Primary?Paper:Ink;
        TSharedRef<SButton> B=SNew(SButton).Tag(RowTag).ButtonStyle(Primary?&Style(true):&Ticket).ContentPadding(FMargin(8,9))
            .OnClicked_Lambda([Action]{Action();return FReply::Handled();})
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(27).HeightOverride(27)[SNew(SPrintMark).Mark(RowTag).Tint(Color)]]
             +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(12,0,0,0)[Text(Name,18,Color,TEXT("BoldItalic"))]];
        if(Primary)
        {
            B->SetEnabled(TAttribute<bool>::CreateLambda([this]{return Plane->IsFlightReady() && Plane->IsAppearanceReady();}));
            First=B;
        }
        Add(List,B,1);
        Add(List,Line(Ink),1);
    };
    Row("Fly",Plane->IsNoclip()?TEXT("REPRENDRE"):Plane->HasStartedFlight()?TEXT("REPRENDRE LE VOL"):TEXT("PRENDRE L’AIR"),[this]{Plane->Fly();},true);
    Row("Hangar",TEXT("HANGAR"),[this]{Go(EPage::Hangar);});
    Row("Tutorial",TEXT("TUTO"),[this]{Go(EPage::Tutorials);});
    Row("OpenMap",TEXT("CARTE"),[this]{Go(EPage::Map);});
    Row("Weather",TEXT("MÉTÉO"),[this]{Go(EPage::Weather);});
    Row("Settings",TEXT("RÉGLAGES"),[this]{Go(EPage::Settings);});
    Row("Credits",TEXT("CRÉDITS"),[this]{Go(EPage::Credits);});
    Row("Quit",TEXT("QUITTER"),[this]{if(auto* PC=Cast<APlayerController>(Plane->GetController())) UKismetSystemLibrary::QuitGame(Plane.Get(),PC,EQuitPreference::Quit,false);});
}

void SFlightMenu::Tutorials(SVerticalBox& List)
{
    Add(List,Text(TEXT("TUTO  /  CHALET DU VERSANT"),11,MutedInk,TEXT("Bold")),8);
    auto Intro=Text(TEXT("Prends tes repères au site 2, découvre le vent puis effectue ton premier vol avec le Topy. Une consigne à la fois, à ton rythme."),13,Ink);
    // The tutorial lives in the fixed 398-unit side panel. An explicit wrap
    // width gives its ScrollBox a correct multiline desired height on first layout.
    Intro->SetWrapTextAt(340.f);
    Add(List,Intro,12);

    const bool Active=Plane->IsTutorialActive();
    if(Active)
        Add(List,Button(TEXT("TutorialResume"),TEXT("Reprendre mon 1er vol"),[this]{Plane->Fly();},true),7);
    auto Start=Button(TEXT("TutorialStart"),Active?TEXT("Recommencer le tutoriel"):TEXT("Commencer mon 1er vol"),
        [this]{Plane->StartFirstFlightTutorial();},!Active);
    Start->SetEnabled(TAttribute<bool>::CreateLambda([this]{return Plane->IsFlightReady() && Plane->IsAppearanceReady();}));
    Add(List,Start,7);
    if(Active)
        Add(List,Button(TEXT("TutorialExit"),TEXT("Quitter le tutoriel"),[this]{Plane->EndFirstFlightTutorial();Go(EPage::Home);}),7);

    Add(List,Line(Ink),11);
    Add(List,Text(TEXT("LE PARCOURS"),11,MutedInk,TEXT("Bold")),8);
    const TCHAR* Steps[]={
        TEXT("01  Rejoindre le repère à pied"),
        TEXT("02  Observer la manche à air"),
        TEXT("03  Afficher les filets d’air"),
        TEXT("04  Tester direction et profondeur"),
        TEXT("05  Sortir le Topy et le lancer"),
        TEXT("06  Tenir en l’air environ 2 min 30"),
        TEXT("07  Préparer l’approche et atterrir"),
        TEXT("08  Rejoindre et récupérer le planeur")};
    for(const TCHAR* Step:Steps) Add(List,Text(Step,12,Ink),5);

    Add(List,Line(Ink),10);
    auto Recovery=Text(TEXT("Perdu de vue ou crashé ? La commande « Récupérer » permet de réessayer sans refaire l’introduction. Les consignes affichent tes touches attribuées."),12,MutedInk);
    Recovery->SetWrapTextAt(340.f);
    Add(List,Recovery,10);
    auto Conditions=Text(TEXT("Météo douce au départ, puis vent progressivement réduit pour préparer le posé. Tes réglages personnels sont restaurés à la sortie du tutoriel."),12,MutedInk);
    Conditions->SetWrapTextAt(340.f);
    Add(List,Conditions,14);
}

void SFlightMenu::Hangar(SVerticalBox& List)
{
    Add(List,Choice(TEXT("MODÈLE"),[this]{ return FString(Plane->ModelName()); },[this](int32 Step){
        Plane->BrowseModel(Step);
        DraftLoading = Plane->Loading();
        bDraftAileronSteering = Plane->UsesAileronSteering();
        DraftGains = Plane->ControlGains();
        DraftFlaps = Plane->FlapTravel();
        DraftMotor = Plane->MotorSettings();
        Build();
        FocusFirst();
    }),6);
    Add(List,Live([this]{
        if (!Plane->IsAppearanceReady()) return FString(TEXT("Chargement du planeur…"));
        return Plane->HangarSummary();
    },12,MutedInk),12);
    if (Plane->ModelIndex()==2)
    {
        auto VjxCredit=Text(TEXT("VJX conçu par Jochen Günzel · adapté avec son autorisation. Remerciements à Mario Perner pour la mise en relation."),11,MutedInk);
        VjxCredit->SetAutoWrapText(true);
        Add(List,VjxCredit,12);
    }
    if (!Plane->CanSelectModel()) { Add(List,Text(TEXT("Modèle indisponible"),12,Rust)); return; }
    Add(List,Choice(TEXT("ZOOM DU PLANEUR"),[this]{return FString::Printf(TEXT("× %.2f"),Plane->HangarZoom);},
        [this](int32 Step){Plane->HangarZoom=FMath::Clamp(Plane->HangarZoom+Step*.25f,.5f,5.f);}),8);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)
        [Button(TEXT("TabSetup"),TEXT("Réglages"),[this]{bAdvancedModel=false;Build();FocusFirst();},!bAdvancedModel)]
        +SHorizontalBox::Slot().FillWidth(1)
        [Button(TEXT("TabAdvanced"),TEXT("Avancé"),[this]{bAdvancedModel=true;Build();FocusFirst();},bAdvancedModel)],12);
    if (bAdvancedModel) AdvancedModel(List);
    else ModelSetup(List);
    if (Plane->HasStartedFlight()) Add(List,Text(TEXT("Modifier puis appliquer prépare un nouveau départ."),11,Rust),5);
    Add(List,Button(TEXT("SelectAircraft"),TEXT("Appliquer et sélectionner"),[this]{
        Plane->ApplyModelSettings(DraftLoading,bDraftAileronSteering,DraftGains,DraftFlaps,DraftMotor); Go(EPage::Home);
    },true),5);
}

void SFlightMenu::ModelSetup(SVerticalBox& List)
{
    Add(List,Choice(TEXT("MANCHE POUR LES VIRAGES"),[this]{
        if (!Plane->HasRudder()) return FString(bDraftAileronSteering ? TEXT("Manche de direction → roulis") : TEXT("Manche d’ailerons → roulis"));
        return FString(bDraftAileronSteering ? TEXT("Ailerons → direction") : TEXT("Direction → direction"));
    },[this](int32){ bDraftAileronSteering = !bDraftAileronSteering; }),4);
    Add(List,Text(Plane->ModelIndex()==7 ? TEXT("Ailerons et profondeur intégrale · dérive fixe") : Plane->ModelIndex()==6 ? TEXT("Élevons · profondeur et roulis combinés") : Plane->ModelIndex()==0 ? TEXT("Topy · mixage en V avec la profondeur") :
        Plane->ModelIndex()>=2 ? TEXT("L’autre manche commande les ailerons") : TEXT("Commande de la dérive du K3600"),11,MutedInk),8);
    Add(List,Choice(TEXT("MASSE TOTALE"),[this]{ return FString::Printf(TEXT("%d g"), DraftLoading.MassGrams); },
        [this](int32 Step){
            const int32 I = Plane->ModelIndex();
            const int32 MassStep = I == 5 ? 1 : (I == 3 || I == 4 || I == 8) ? 100 : 5;
            DraftLoading.MassGrams += Step * MassStep;
            DraftLoading = Plane->LimitLoading(DraftLoading);
        }),6);
    Add(List,Choice(TEXT("CENTRAGE LONGITUDINAL"),[this]{ return FString::Printf(TEXT("%d mm"), DraftLoading.CgMm); },
        [this](int32 Step){ DraftLoading.CgMm += Step; DraftLoading = Plane->LimitLoading(DraftLoading); }),4);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.62f)[Text(TEXT("VUE DE DESSUS   ↑ NEZ"),10,MutedInk)]
        +SHorizontalBox::Slot().FillWidth(.38f)[Text(TEXT("ZOOM CENTRAGE"),10,MutedInk)],2);
    Add(List,SNew(SCgPlan).Tag(TEXT("CgPlan")).ModelIndex(Plane->ModelIndex()).SpanCm(Plane->ModelSpanCm())
        .OffsetMm_Lambda([this]{ return DraftLoading.CgMm-Plane->StandardLoading().CgMm; })
        .OnMove_Lambda([this](int32 Delta){ DraftLoading.CgMm=Plane->StandardLoading().CgMm+Delta; }),3);
    Add(List,Live([this]{
        const int32 Origin = Plane->StandardLoading().CgMm, Delta = DraftLoading.CgMm - Origin;
        const FString Shift = Delta ? FString::Printf(TEXT("%d mm vers %s"), FMath::Abs(Delta), Delta < 0 ? TEXT("le nez") : TEXT("la queue")) : TEXT("sur le repère d’origine");
        return FString::Printf(TEXT("Origine  %d mm   ·   %s"), Origin, *Shift);
    },12,Rust),4);
    Add(List,Text(TEXT("Glisser le point doré · cercle blanc d’origine."),11,MutedInk),6);
    if (Plane->ModelIndex()==8) Add(List,Text(TEXT("Centrage estimé depuis le bord d’attaque à l’emplanture."),11,MutedInk),4);
    if (Plane->ModelIndex()==7) Add(List,Text(TEXT("Notice FEMO · 60–70 mm du bord d’attaque à l’emplanture"),11,MutedInk),4);
    if (Plane->ModelIndex()==6) Add(List,Text(TEXT("Notice LongBow Mk2 · 214 mm depuis le nez"),11,MutedInk),4);
    if (Plane->ModelIndex()==1) Add(List,Text(TEXT("Notice K3600  ·  101–106 mm du bord d’attaque"),11,MutedInk),4);
    Add(List,Button(TEXT("DefaultCg"),TEXT("Rétablir le centrage d’origine"),[this]{ DraftLoading.CgMm = Plane->StandardLoading().CgMm; }),4);
}

void SFlightMenu::AdvancedModel(SVerticalBox& List)
{
    Add(List,Text(TEXT("MOTORISATION"),12,Rust,TEXT("Bold")),5);
    Add(List,SNew(SCheckBox).Style(&CheckStyle()).Tag(TEXT("MotorEnabled"))
        .IsChecked_Lambda([this]{return DraftMotor.Enabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State){DraftMotor.Enabled = State == ECheckBoxState::Checked;})
        [Text(TEXT("Activer le moteur"),12,Ink)],6);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Puissance maximale (W)"),12,Ink)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<int32>).Style(&NumberFieldStyle()).Tag(TEXT("MotorMaxWatts"))
            .IsEnabled_Lambda([this]{return DraftMotor.Enabled;})
            .MinDesiredWidth(76).MinValue(1).MaxValue(FGliderMotor::MaxWatts).Delta(1)
            .Value_Lambda([this]{return DraftMotor.Watts;})
            .OnValueChanged_Lambda([this](int32 Watts){DraftMotor.Watts=Watts;})],5);
    Add(List,Text(TEXT("Gaz et coupure moteur à attribuer dans Périphériques."),11,MutedInk),12);
    if (Plane->IsControlInUse(EGliderControl::Flaps))
    {
        Add(List,Text(TEXT("DÉBATTEMENT DES VOLETS"),12,Rust,TEXT("Bold")),5);
        Add(List,Text(TEXT("0 % : haut     ·     50 % : neutre     ·     100 % : bas"),11,MutedInk),8);
        auto Angle = [this,&List](const TCHAR* Label, int32 FFlapTravel::*Field)
        {
            Add(List,SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label,12,Ink)]
                +SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<int32>).Style(&NumberFieldStyle()).MinDesiredWidth(76)
                    .MinValue(0).MaxValue(90).Delta(1)
                    .Value_Lambda([this,Field]{return DraftFlaps.*Field;})
                    .OnValueChanged_Lambda([this,Field](int32 Value){DraftFlaps.*Field=FMath::Clamp(Value,0,90);})],5);
        };
        Angle(TEXT("Vers le haut (°)"),&FFlapTravel::Up);
        Angle(TEXT("Vers le bas (°)"),&FFlapTravel::Down);
        Add(List,Button(TEXT("DefaultFlapTravel"),TEXT("Rétablir les débattements du modèle"),[this]{DraftFlaps=Plane->DefaultFlapTravel();}),12);
    }
    Add(List,Text(TEXT("FORCE DES COMMANDES"),12,Rust,TEXT("Bold")),5);
    Add(List,Text(TEXT("100 % référence · 0 % nul · négatif inversé"),11,MutedInk),14);
    auto Gain = [&](FName ControlTag,const TCHAR* Label,TFunction<int32()> Read,TFunction<void(int32)> Write)
    {
        Add(List,Text(Label,11,Ink),5);
        Add(List,SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,12,0)
            [SNew(SSlider).MinValue(-500.f).MaxValue(500.f).StepSize(5.f).MouseUsesStep(true)
                .Value_Lambda([Read]{return float(Read());})
                .OnValueChanged_Lambda([Write](float Value){Write(FMath::Clamp(FMath::RoundToInt32(Value),-500,500));})
                .SliderBarColor(Ink).SliderHandleColor(Coral)]
            +SHorizontalBox::Slot().AutoWidth()
            [SNew(SSpinBox<int32>).Style(&NumberFieldStyle()).Tag(ControlTag).MinDesiredWidth(76).MinValue(-500).MaxValue(500)
                .MinSliderValue(-500).MaxSliderValue(500).Delta(1)
                .Value_Lambda([Read]{return Read();})
                .OnValueChanged_Lambda([Write](int32 Value){Write(FMath::Clamp(Value,-500,500));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0,0,0)[Text(TEXT("%"),12,MutedInk)],15);
    };
    const bool Elevons = Plane->ModelIndex()==6;
    if (Plane->HasRudder()) Gain(TEXT("GainYaw"),TEXT("Direction / lacet"),[this]{return DraftGains.Yaw;},[this](int32 Value){DraftGains.Yaw=Value;});
    Gain(TEXT("GainRoll"),Elevons ? TEXT("Roulis par élevons") : Plane->ModelIndex()>=7 ? TEXT("Roulis par ailerons") : TEXT("Roulis induit"),[this]{return DraftGains.Roll;},[this](int32 Value){DraftGains.Roll=Value;});
    Gain(TEXT("GainPitch"),TEXT("Profondeur"),[this]{return DraftGains.Pitch;},[this](int32 Value){DraftGains.Pitch=Value;});
    auto Note=Text(Plane->ModelIndex()==8 ? TEXT("Les ailerons commandent le roulis. La direction permet de coordonner le virage.") : Plane->ModelIndex()==7 ? TEXT("Le stabilisateur pendulaire commande la profondeur.") : Elevons ? TEXT("Les deux élevons travaillent ensemble en profondeur et en sens opposés en roulis.")
        : TEXT("Sur ces modèles 2 axes, le manche de virage agit sur la direction et le roulis. Glisser les curseurs ou saisir un pourcentage."),11,MutedInk);
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
    Add(List,Live([&R]{return R.Status();},11,Rust),12);
    auto WalkHint=Text(TEXT("WASD / ZQSD et les flèches déplacent le pilote, même en vol. Pour piloter au clavier, attribue des touches différentes ; la radio reste indépendante."),11,MutedInk);
    WalkHint->SetAutoWrapText(true);
    Add(List,WalkHint,12);

    auto Select=[this,&R](A Next){R.CancelLearn();Control=Next;Build();};
    auto Left=SNew(SVerticalBox);
    Add(*Left,Text(TEXT("FAMILLE"),10,MutedInk,TEXT("Bold")),4);
    Add(*Left,SNew(STextComboBox).ButtonStyle(&Style(false)).ColorAndOpacity(Ink)
        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),12)).ContentPadding(FMargin(9,5))
        .OptionsSource(&Groups).InitiallySelectedItem(Groups[int32(Group)])
        .OnSelectionChanged_Lambda([this,Select](TSharedPtr<FString> Item,ESelectInfo::Type){
            const int32 Index=Groups.IndexOfByKey(Item);
            if (Index<0 || Index==int32(Group)) return;
            Group=EControlGroup(Index);
            for (int32 I=0;I<ControlCount;++I) if (A(I)!=A::Instruments && ControlSpec(A(I)).Group==Group) {Select(A(I));return;}
        }),8);
    for (int32 I=0;I<ControlCount;++I)
    {
        const A Item=A(I);
        const auto& S=ControlSpec(Item);
        if (Item!=A::Instruments && S.Group==Group) Add(*Left,Button(S.Id,S.Label,[Select,Item]{Select(Item);},Item==Control),3);
    }

    auto Right=SNew(SVerticalBox);
    Add(*Right,Text(Plane->IsControlInUse(Control)?TEXT("EN SERVICE SUR CE PLANEUR"):TEXT("NON UTILISÉ SUR CE PLANEUR"),9,Rust,TEXT("Bold")),3);
    Add(*Right,Text(Spec.Label,17,Ink,TEXT("Bold")),6);
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
                R.Clear(Control,Minus);return FReply::Handled();})[Text(TEXT("×"),14,MutedInk)];
        Clear->SetEnabled(TAttribute<bool>::CreateLambda([this,&R,Minus]{
            const auto& B=R.Binding(Control);return R.CanEdit() && !(Minus?B.Minus:B.Plus).IsNone();}));
        Add(*Right,SNew(SVerticalBox).Visibility_Lambda([this,&R,Minus]{
            return !Minus || R.SupportsMinus(Control)?EVisibility::Visible:EVisibility::Collapsed;})
            +SVerticalBox::Slot().AutoHeight()[Text(Minus?TEXT("SENS –  /  BOUTON OU TOUCHE"):TEXT("ENTRÉE PRINCIPALE  /  SENS +"),9,MutedInk,TEXT("Bold"))]
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
    if (Spec.Type==T::Switch)
        Add(*Right,SNew(SButton).ButtonStyle(&Style(false)).ContentPadding(FMargin(10,5)).IsEnabled(Available)
            .OnClicked_Lambda([this,&R]{R.CancelLearn();R.ToggleMode(Control);return FReply::Handled();})
            [Live([this,&R]{
                return R.Binding(Control).Toggle?TEXT("Mode bascule · un appui inverse l’état"):TEXT("Mode maintenu · suit l’interrupteur");
            },11)],4);
    Add(*Right,Live([this,&R]{
        const float V=R.Preview(Control);
        switch (ControlSpec(Control).Type)
        {
            case T::CenteredRange: return FString::Printf(TEXT("Course   %.0f %%   ·   neutre à 50 %%"),(1.f+V)*50.f);
            case T::Pulse: case T::Switch: return FString(V>.5f?TEXT("Entrée active"):TEXT("Entrée au repos"));
            default: return FString::Printf(TEXT("Valeur   %+.0f %%"),V*100.f);
        }
    },12,Rust),3);
    static const FSlateColorBrush MeterTrack(Ink);
    Add(*Right,SNew(SBox).HeightOverride(6)[SNew(SProgressBar).BackgroundImage(&MeterTrack).FillImage(Fill()).Percent_Lambda([this,&R]{
        const auto Type=ControlSpec(Control).Type;
        const float V=R.Preview(Control);
        return TOptional<float>((Type==T::Signed || Type==T::Trim || Type==T::CenteredRange)?(1.f+V)*.5f:V);
    }).FillColorAndOpacity(Coral)]);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.42f).Padding(0,0,16,0)[Left]
        +SHorizontalBox::Slot().FillWidth(.58f)[SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FLinearColor(.96f,.825f,.57f)).Padding(12)[Right]],8);
    auto MessageText=Live([&R]{
        if (!R.Notice.IsEmpty()) return R.Notice;
        return R.CanEdit()?FString(TEXT("Choisis une fonction, puis attribue un axe, un bouton ou une touche.")):
            FString(TEXT("Branche une radio ou sélectionne Clavier pour attribuer les touches."));
    },12,MutedInk);
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
        Add(List,Text(TEXT("Conserver cet affichage ?"),17,Ink,TEXT("Bold")),8);
        Add(List,Live([this]{return FString::Printf(TEXT("Rétablissement dans %d s"),FMath::Max(0,FMath::CeilToInt(ConfirmUntil-FPlatformTime::Seconds())));},13,MutedInk),18);
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
        Add(List,Text(Label,12,MutedInk,TEXT("Bold")),3);
        Add(List,SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)
            [SNew(SCheckBox).Style(&CheckStyle()).IsChecked_Lambda([&Visible]{return Visible?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,&Visible](ECheckBoxState State){Visible=State==ECheckBoxState::Checked; Message.Empty();})]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(SSlider).SliderBarColor(Ink).SliderHandleColor(Coral).Tag(SliderTag).MinValue(20.f).MaxValue(500.f).StepSize(1.f).MouseUsesStep(true)
                .IsEnabled_Lambda([&Visible]{return Visible;}).Value_Lambda([&Meters]{return float(Meters);})
                .OnValueChanged_Lambda([this,&Meters](float Value){Meters=FMath::Clamp(FMath::RoundToInt(Value),20,500);Message.Empty();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(10,0,0,0)
            [Live([&Meters]{return FString::Printf(TEXT("%d m"),Meters);},13)],12);
    };
    RangeRow(TEXT("DISTANCE DE L’HERBE"),TEXT("GrassRange"),ShowGrass,GrassDistance);
    RangeRow(TEXT("DISTANCE DES FILETS D’AIR"),TEXT("TrailRange"),ShowTrails,TrailDistance);
    Add(List,Live([this]{return !Message.IsEmpty()?Message:Mode==EWindowMode::WindowedFullscreen?FString(TEXT("Sans bordure : résolution du bureau.")):FString();},11,MutedInk),6);
    Add(List,Button(TEXT("Apply"),TEXT("Appliquer"),[this]{Apply();},true),5);
}

void SFlightMenu::OpenMapFromFlight()
{
    bMapFromFlight=true;
    Go(EPage::Map);
}
void SFlightMenu::CloseMap()
{
    if (bMapFromFlight) { bMapFromFlight=false; Plane->Fly(); }
    else Go(EPage::Home);
}
void SFlightMenu::Map(SVerticalBox& List)
{
    Add(List,Text(TEXT("NORD ↑     X vers le nord   ·   Y vers l’est   ·   Distances en mètres"),12,MutedInk),6);
    Add(List,SAssignNew(MapWidget,SFlightMap).Plane(Plane.Get()),4);
    Add(List,SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(Ink).Padding(8)
        [Live([this]{return MapWidget->Coordinates();},13,Gold)],6);
    Add(List,SNew(SHorizontalBox)
        +SHorizontalBox::Slot()[Button(TEXT("MapOut"),TEXT("Zoom −"),[this]{MapWidget->Zoom(1.5,MapWidget->GetCachedGeometry().GetLocalSize()*.5);})]
        +SHorizontalBox::Slot()[Button(TEXT("MapIn"),TEXT("Zoom +"),[this]{MapWidget->Zoom(1./1.5,MapWidget->GetCachedGeometry().GetLocalSize()*.5);})]
        +SHorizontalBox::Slot()[Button(TEXT("MapCentre"),TEXT("Ma position"),[this]{MapWidget->Recenter();})],5);
    Add(List,Text(TEXT("Glisser pour déplacer la carte · Molette pour zoomer sous le pointeur"),11,MutedInk),8);
}

void SFlightMenu::Vario(SVerticalBox& List)
{
    Add(List,Choice(TEXT("SON DU VARIO"),[this]{return FString(Plane->VarioEnabled()?TEXT("Activé"):TEXT("Désactivé"));},
        [this](int32){Plane->SetVario(!Plane->VarioEnabled(),Plane->VarioClimb(),Plane->VarioSink());}),12);
    Add(List,Choice(TEXT("SEUIL DE MONTÉE"),[this]{return FString::Printf(TEXT("+%.1f m/s"),Plane->VarioClimb());},
        [this](int32 Step){Plane->SetVario(Plane->VarioEnabled(),Plane->VarioClimb()+.1f*Step,Plane->VarioSink());}),10);
    Add(List,Choice(TEXT("SEUIL DE DESCENTE"),[this]{return FString::Printf(TEXT("%.1f m/s"),Plane->VarioSink());},
        [this](int32 Step){Plane->SetVario(Plane->VarioEnabled(),Plane->VarioClimb(),Plane->VarioSink()+.1f*Step);}),12);
    auto Hint=Text(TEXT("En montée, les bips accélèrent. En descente, le son devient grave et continu. Entre les deux seuils, le vario reste silencieux."),12,MutedInk);
    Hint->SetAutoWrapText(true);
    Add(List,Hint,12);
    Add(List,Text(TEXT("Marche / arrêt en vol · ")+Plane->Radio.Shortcut(EGliderControl::Vario),12,Rust),12);
}

void SFlightMenu::Weather(SVerticalBox& List)
{
    Add(List,Choice(TEXT("HEURE SOLAIRE"),[this]{return FString::Printf(TEXT("%02d h %02d"),DraftWeather.SolarMinutes/60,DraftWeather.SolarMinutes%60);},
        [this](int32 Step){DraftWeather.SolarMinutes=(DraftWeather.SolarMinutes+Step*30+1440)%1440;Message.Empty();}),10);
    Add(List,Choice(TEXT("ÉTAT DU CIEL"),[this]{
        const TCHAR* Names[]={TEXT("Dégagé"),TEXT("Éclaircies"),TEXT("Couvert"),TEXT("Brumeux"),TEXT("Stratus de pente"),TEXT("Averses locales"),TEXT("Orage")};
        return FString(Names[DraftWeather.Sky]);},
        [this](int32 Step){
            DraftWeather.Sky=(DraftWeather.Sky+Step+7)%7;
            constexpr double Bases[]={900.,650.,500.,240.,300.,520.,420.};
            constexpr double Cover[]={0.,.55,.92,.65,.90,.80,.96};
            DraftWeather.CloudBaseMeters=Bases[DraftWeather.Sky];
            DraftWeather.CloudCoverage=Cover[DraftWeather.Sky];
            Message=TEXT("Ambiance préréglée ; les paramètres restent ajustables.");
        }),10);
    Add(List,Choice(TEXT("FORCE DU VENT"),[this]{return FString::Printf(TEXT("%.1f m/s · %.0f km/h"),DraftWeather.WindSpeedMps,3.6*DraftWeather.WindSpeedMps);},
        [this](int32 S){DraftWeather.WindSpeedMps=FMath::Clamp(DraftWeather.WindSpeedMps+.5*S,0.,10.);Message.Empty();}),10);
    Add(List,Choice(TEXT("VENT VENANT DU"),[this]{return FString::Printf(TEXT("%03d°"),FMath::RoundToInt(DraftWeather.WindFromDegrees)%360);},
        [this](int32 S){
            const int32 Step=S>0 ? FMath::FloorToInt(DraftWeather.WindFromDegrees/15.)+1 : FMath::CeilToInt(DraftWeather.WindFromDegrees/15.)-1;
            DraftWeather.WindFromDegrees=(Step*15+360)%360;Message.Empty();
        }),5);
    Add(List,Text(TEXT("Provenance : 0° nord · 90° est · 180° sud."),11,MutedInk),12);
    Add(List,Choice(TEXT("ACTIVITÉ THERMIQUE"),[this]{return FString::Printf(TEXT("%d %%"),FMath::RoundToInt(DraftWeather.ThermalStrength*100));},
        [this](int32 S){DraftWeather.ThermalStrength=FMath::Clamp(DraftWeather.ThermalStrength+.25*S,0.,2.);Message.Empty();}),10);
    Add(List,Choice(TEXT("TURBULENCE"),[this]{return FString::Printf(TEXT("%d %%"),FMath::RoundToInt(DraftWeather.Turbulence*100));},
        [this](int32 S){DraftWeather.Turbulence=FMath::Clamp(DraftWeather.Turbulence+.1*S,0.,1.);Message.Empty();}),12);
    Add(List,Text(TEXT("NUAGES ET PRÉCIPITATIONS"),11,MutedInk,TEXT("Bold")),8);
    Add(List,Choice(TEXT("BASE DES NUAGES"),[this]{return FString::Printf(TEXT("%.0f m d’altitude"),DraftWeather.CloudBaseMeters);},
        [this](int32 S){DraftWeather.CloudBaseMeters=FMath::Clamp(DraftWeather.CloudBaseMeters+S*50.,80.,2500.);Message.Empty();}),8);
    Add(List,Choice(TEXT("COUVERTURE NUAGEUSE"),[this]{return FString::Printf(TEXT("%.0f %%"),DraftWeather.CloudCoverage*100.);},
        [this](int32 S){DraftWeather.CloudCoverage=FMath::Clamp(DraftWeather.CloudCoverage+S*.05,0.,1.);Message.Empty();}),8);
    Add(List,SNew(SCheckBox).Style(&CheckStyle()).Tag(TEXT("LowClouds"))
        .IsChecked_Lambda([this]{return DraftWeather.LowClouds?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State){DraftWeather.LowClouds=State==ECheckBoxState::Checked;Message.Empty();})
        [Text(TEXT("Nappes basses sur les reliefs"),12,Ink)],10);
    Add(List,Choice(TEXT("INTENSITÉ DES AVERSES"),[this]{return FString::Printf(TEXT("%.0f %%"),DraftWeather.RainStrength*100.);},
        [this](int32 S){DraftWeather.RainStrength=FMath::Clamp(DraftWeather.RainStrength+S*.1,0.,1.);Message.Empty();}),8);
    auto RainHint=Text(TEXT("Pluie en modes Averses locales et Orage : les zones humides se déplacent avec le vent. La base des nuages est une altitude, pas une hauteur au-dessus du pilote."),11,MutedInk);
    RainHint->SetAutoWrapText(true);
    Add(List,RainHint,10);
    Add(List,Text(TEXT("La pente porte du côté exposé au vent.\nLes thermiques dépendent du sol et de l’altitude."),11,MutedInk),10);
    Add(List,Live([this]{return Message;},11,Rust),6);
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
    if (HomeAfterStartup && PosterFirstFrame == 0.) PosterFirstFrame = FPlatformTime::Seconds();
    if (ConfirmUntil>0. && FPlatformTime::Seconds()>=ConfirmUntil) Go(EPage::Graphics);
}

// Called during the world tick so Slate can lay out the home screen before its first paint.
void SFlightMenu::UpdateStartupPoster()
{
    if (!IsShowingStartupPoster() || PosterFirstFrame == 0.) return;
    const double VisibleSeconds = FPlatformTime::Seconds() - PosterFirstFrame;
    // Allow the home screen to open even if aircraft texture streaming stalls.
    if (VisibleSeconds < 5. || (!Plane->IsAppearanceReady() && VisibleSeconds < 15.)) return;
    ChildSlot.AttachWidget(HomeAfterStartup.ToSharedRef());
    HomeAfterStartup.Reset();
    StartupPoster.Reset();
    FocusFirst();
    UE_LOG(LogTemp, Display, TEXT("Startup poster handed off to home after %.2f s"), VisibleSeconds);
}

FReply SFlightMenu::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Key)
{
    if (HomeAfterStartup) return FReply::Handled();
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
    if (Page==EPage::Map && !Key.IsRepeat() && (Key.GetKey()==EKeys::Escape || Key.GetKey()==EKeys::F2))
    {
        CloseMap();
        return FReply::Handled();
    }
    if (Key.GetKey()==EKeys::Escape && !Key.IsRepeat())
    {
        if (ConfirmUntil>0.) Go(EPage::Graphics);
        else if (Page!=EPage::Home) Go(EPage::Home);
        else if (Plane->HasStartedFlight()) Plane->Fly();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry,Key);
}
