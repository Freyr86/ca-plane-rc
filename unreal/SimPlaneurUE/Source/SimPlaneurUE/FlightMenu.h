#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GliderControls.h"
#include "GliderDynamics.h"
#include "ValleyAir.h"
#include "GenericPlatform/GenericWindow.h"

class ASlopeFlightPawn;
class SBox;
class SButton;
class SVerticalBox;
class SFlightMap;
class FDeferredCleanupSlateBrush;

class SFlightMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFlightMenu) : _ShowStartupPoster(false) {}
        SLATE_ARGUMENT(ASlopeFlightPawn*, Plane)
        SLATE_ARGUMENT(bool, ShowStartupPoster)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    bool SupportsKeyboardFocus() const override { return true; }
    FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;
    FReply OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Key) override;
    FReply OnKeyUp(const FGeometry& Geometry,const FKeyEvent& Key) override;
    void Tick(const FGeometry& Geometry, double Time, float Delta) override;
    void FocusFirst();
    bool IsShowingStartupPoster() const { return HomeAfterStartup.IsValid(); }
    void UpdateStartupPoster();
    void OpenMapFromFlight();
    void CancelDisplayChange();
    int32 DraftCgMm() const { return DraftLoading.CgMm; }
    FGliderControlGains DraftControlGains() const { return DraftGains; }

private:
    enum class EPage { Home, Hangar, Devices, Graphics, Weather, Vario, Map, Settings, Credits, Equipment, Tutorials };
    TSharedPtr<SFlightMap> MapWidget;
    bool bMapFromFlight=false;
    void Map(SVerticalBox& List);
    void CloseMap();
    TWeakObjectPtr<ASlopeFlightPawn> Plane;
    TSharedPtr<SBox> Body;
    TSharedPtr<SBox> TicketBody;
    TSharedPtr<FDeferredCleanupSlateBrush> Masthead;
    TSharedPtr<FDeferredCleanupSlateBrush> Windsock;
    TSharedPtr<FDeferredCleanupSlateBrush> StartupPoster;
    TSharedPtr<SWidget> HomeAfterStartup;
    double PosterFirstFrame = 0.;
    FSlateBrush Wordmark;
    TSharedPtr<SButton> First;
    EControlGroup Group=EControlGroup::Pilot;
    EGliderControl Control=EGliderControl::Rudder;
    TArray<TSharedPtr<FString>> Groups;
    EPage Page = EPage::Home;
    FString Message, Version;
    FGliderLoading DraftLoading;
    FFlapTravel DraftFlaps;
    FGliderMotor DraftMotor;
    FGliderControlGains DraftGains;
    ValleyAir::FWeatherSettings DraftWeather;
    bool bAdvancedModel = false;
    bool bDraftAileronSteering = false;
    EWindowMode::Type Mode = EWindowMode::Windowed, PreviousMode = EWindowMode::Windowed;
    FIntPoint Size = FIntPoint::ZeroValue, PreviousSize = FIntPoint::ZeroValue;
    TArray<FIntPoint> Sizes;
    int32 Quality = -1;
    int32 GrassDistance = 50, TrailDistance = 90;
    bool ShowGrass=true, ShowTrails=true;
    bool bVsync = false;
    double ConfirmUntil = 0.;
    void Go(EPage Target);
    void Build();
    void Home(SVerticalBox& List);
    void Tutorials(SVerticalBox& List);
    void Hangar(SVerticalBox& List);
    void ModelSetup(SVerticalBox& List);
    void AdvancedModel(SVerticalBox& List);
    void Devices(SVerticalBox& List);
    void Graphics(SVerticalBox& List);
    void Weather(SVerticalBox& List);
    void Vario(SVerticalBox& List);
    void FindSizes();
    void Apply();
    void SaveGraphics();
    TSharedRef<SButton> Button(const FString& Name, const FString& Text, TFunction<void()> Click, bool Primary=false);
    TSharedRef<SWidget> Choice(const FString& Name, TFunction<FString()> Value, TFunction<void(int32)> Change,
                               TAttribute<bool> Enabled=true);
};
