#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlightRadio.h"
#include "GliderDynamics.h"
#include "LandingDamage.h"
#include "WindTrails.h"
#include "BungeeLaunch.h"
#include "LaunchInstallations.h"
#include "F3F.h"
#include "FirstFlightTutorial.h"
#include "ValleyAir.h"
#include "SlopeFlightPawn.generated.h"

class USceneComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class SFlightMenu;
class ULineBatchComponent;
class UTexture2D;
class UAudioComponent;

UCLASS()
class SIMPLANEURUE_API ASlopeFlightPawn : public APawn
{
    GENERATED_BODY()

public:
    void StartFirstFlightTutorial();
    void EndFirstFlightTutorial();
    bool IsTutorialActive() const { return Tutorial.Active(); }
    const FFirstFlightTutorial& TutorialStatus() const { return Tutorial; }
    ASlopeFlightPawn();
    virtual void BeginPlay() override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    float GetAirspeedMps() const { return AirspeedMps; }
    float GetSinkMps() const { return SinkMps; }
    float GetAltitudeM() const { return AltitudeM; }
    float GetPhysicsHz() const { return PhysicsHz; }
    double GetWeatherSeconds() const { return WeatherSeconds; }
    double GetWeatherVisualSeconds() const { return EnvironmentSeconds; }
    FVector GetLocalWind() const { return Flight.LocalWind(); }
    double GetRidgeLift() const { return RidgeLift; }
    FVector GetRelativeFlow() const { return Flight.RelativeFlowNED(); }
    bool IsFlightReady() const { return bModelReady; }
    bool IsAppearanceReady() const { return bAppearanceReady; }
    FGliderLoading Loading() const { return HangarModels[PreviewModel].Loading(); }
    FFlapTravel FlapTravel() const { return HangarModels[PreviewModel].FlapTravel(); }
    FFlapTravel DefaultFlapTravel() const { return HangarModels[PreviewModel].DefaultFlapTravel(); }
    FGliderControlGains ControlGains() const { return HangarModels[PreviewModel].ControlGains(); }
    FGliderMotor MotorSettings() const { return HangarModels[PreviewModel].MotorSettings(); }
    FGliderLoading StandardLoading() const { return HangarModels[PreviewModel].StandardLoading(); }
    FGliderLoading LimitLoading(FGliderLoading Setting) const { return HangarModels[PreviewModel].LimitLoading(Setting); }
    const TCHAR* ModelName() const;
    FString ModelNumber() const;
    FString HangarSummary() const;
    double ModelSpanCm() const;
    int32 ModelIndex() const { return PreviewModel; }
    bool CanSelectModel() const { return AvailableModels[PreviewModel]; }
    bool UsesAileronSteering() const { return AileronSteering[PreviewModel]; }
    bool HasRudder() const { return HangarModels[PreviewModel].HasRudder(); }
    bool IsControlInUse(EGliderControl Control) const;
    void BrowseModel(int32 Step);
    void ApplyModelSettings(FGliderLoading Setting, bool UseAilerons, FGliderControlGains Gains, FFlapTravel Travel, FGliderMotor Motor);
    bool IsRadioActive() const { return Radio.IsDriving(); }
    bool IsMenuOpen() const { return bMenuOpen; }
    bool HasStartedFlight() const { return bStartedFlight; }
    bool IsHeldForLaunch() const { return bHeldForLaunch; }
    bool UsesDiscusLaunch() const { return SelectedModel == 5; }
    bool IsThrowing() const { return ThrowSeconds >= 0.f; }
    bool IsWalking() const { return (!bGroundPhysics || bLandingLookReleased) && (bHeldForLaunch || bInBackpack || Flight.IsStopped()) && !IsThrowing() && !bNoclip && !bMenuOpen; }
    bool InBackpack() const { return bInBackpack; }
    bool CanWalkInFlight() const { return !bMenuOpen && !bNoclip && !bHeldForLaunch && !IsThrowing() && !Flight.IsStopped() && !bGroundPhysics; }
    const FBungeeLaunch& BungeeStatus() const { return Bungee; }
    bool BungeeSelected() const { return bBungeeSelected && !Bungee.IsWinch(); }
    bool WinchSelected() const { return bBungeeSelected && Bungee.IsWinch(); }
    float WinchPowerLimit() const { return WinchMaximumPower; }
    void SetWinchPowerLimit(float Value);
    double WinchCommand() const;
    FLinearColor BungeeColour() const;
    static FLinearColor EquipmentColour(int32 Index);
    const TArray<FLaunchInstallation>& InstalledEquipment() const { return LaunchInstallations.Entries(); }
    FString EquipmentConfirmation() const { return FPlatformTime::Seconds()<EquipmentConfirmationUntil ? EquipmentConfirmationText : FString(); }
    FString BungeeColourName() const;
    void CycleBungeeColour(int32 Step);
    double BungeeTension() const;
    FString EquipmentPrompt() const;
    FString BackpackAction() const;
    const TCHAR* FlightCameraName() const;
    bool IsNoclip() const { return bNoclip; }
    FVector ExplorationPosition() const { return NoclipPosition; }
    float ExplorationSpeed() const { return NoclipSpeed; }
    FVector PilotPosition() const;
    FString ExplorationNotice() const { return SiteNotice; }
    int32 LaunchSiteNumber() const { return SelectedSite; }
    void SwitchLaunchSite(int32 Step);
    void ExploreScene();
    void Fly();
    void OpenMenu();
    void ShowMenuPresentation(bool Hangar,bool Poster);
    float HangarZoom = 1.f;
    FVector2D MapPositionXY() const;
    FFlightRadio Radio;
    FString GetContactLabel() const;
    float GetRadioAxis(int32 Index) const { return Radio.RawAxis(Index); }
    bool HasFlaps() const { return Flight.HasFlaps(); }
    double CrowFraction() const { return Flight.CrowFraction(); }
    double FlapAngleDeg() const { return Flight.FlapAngleDeg(); }
    FString GetControlMode() const { return Radio.HasDevice() ? Radio.Status() : TEXT("Clavier"); }
    bool ShowInstruments() const {return bShowInstruments;}
    bool VarioEnabled() const { return bVarioEnabled; }
    float VarioClimb() const { return ClimbThreshold; }
    float VarioSink() const { return SinkThreshold; }
    void SetVario(bool Enabled, float Climb, float Sink);

bool TrailsVisible() const { return bShowWindTrails; }
    int32 TrailDistance=90;
    const FF3FCourse& F3FCourse() const { return F3FCircuit; }
    const FF3FRun& F3FStatus() const { return F3FRun; }
    bool F3FEnabled() const { return bF3FEnabled; }
    FString F3FPrompt() const;
    void ShowTrails(bool Visible) { bShowWindTrails=Visible; }
private:
    FFirstFlightTutorial Tutorial;
    ValleyAir::FWeatherSettings TutorialPreviousWeather, TutorialWeather;
    FGliderDynamics TutorialPreviousFlight;
    FBungeeLaunch TutorialPreviousBungee;
    int32 TutorialPreviousModel=0, TutorialPreviousPreview=0, TutorialPreviousSite=1, TutorialPreviousView=0, TutorialPreviousBungeeId=INDEX_NONE;
    FVector TutorialPreviousPilot=FVector::ZeroVector;
    FVector2D TutorialPreviousOffset=FVector2D::ZeroVector;
    float TutorialPreviousHeading=0, TutorialPreviousPitch=0, TutorialHeading=0;
    bool TutorialPreviousTrails=false, TutorialPreviousF3F=false;
    bool bTutorialProof=false;
    bool bTutorialStarting=false;
    void UpdateFirstFlightTutorial(float Dt);
    void RecoverFirstFlightTutorial();
    void TestFirstFlightTutorial(float Elapsed);
    bool bF3FProof=false;
    void TestF3F(float Elapsed);
    FF3FCourse F3FCircuit, F3FDraft;
    FVector F3FCentreCache=FVector::ZeroVector;
    FVector F3FCentreGround=FVector::ZeroVector;
    TArray<FVector> F3FCentreRing;
    FF3FRun F3FRun;
    bool bF3FEnabled=false, bF3FPlacementValid=false;
    int32 F3FPlacement=0;
    FVector F3FMarker=FVector::ZeroVector;
    UPROPERTY() TObjectPtr<ULineBatchComponent> F3FLines;
    UPROPERTY() TObjectPtr<UAudioComponent> F3FAudio;
    double F3FBeepUntil=0.;
    void UpdateF3F(float Dt);
    void SelectF3F();
    void InteractF3F();
    void F3FPhysics(const FTransform& Before,const FTransform& After,double Dt);
    void F3FLaunch();
    FLaunchInstallations LaunchInstallations;
    int32 ActiveBungeeId = INDEX_NONE;
    FString EquipmentConfirmationText;
    double EquipmentConfirmationUntil = 0.;
    void SelectNearbyBungee();
    void ActivateBungee(int32 Id);
    FBungeeLaunch Bungee;
    bool bInBackpack = false;
    bool bBungeeSelected = false;
    int32 BungeeColourIndex = 0;
    bool bPlacementValid = false;
    FVector PlacementPoint = FVector::ZeroVector;
    float InteractionSeconds = 0.f;
    bool bInteractionConsumed = false;
    bool bInteractionStarted = false;
    bool bInteractionCanPack = false;
    UPROPERTY()
    TObjectPtr<ULineBatchComponent> EquipmentLines;
    void ToggleBackpack();
    void SelectBungee();
    void SelectLaunchEquipment(ELaunchEquipment Type);
    float WinchMaximumPower = 1.f;
    bool bManualWinchPower = false;
    UPROPERTY()
    TMap<int32,TObjectPtr<class AWinchStation>> WinchStations;
    void SyncWinchStations();
    void SaveBungee();
    void LoadBungee();
    void InteractEquipment(bool Pack);
    void UpdateEquipment(float DeltaSeconds);
    void HandleEquipmentInput(bool Down, bool Pressed, bool Released, float DeltaSeconds);
    FString DeparturePlacementError() const;
    FVector EquipmentLineEnd() const;
    bool NearEquipmentEnd() const;
    bool ClearBungeeRoute(const FVector& End) const;
    FTransform BungeeHandPose() const;
    FVector BungeeForceNED();
    bool bBungeeProof = false;
    bool bWinchProof = false;
    bool bLandingProof = false;
    void TestLanding(float Elapsed);
    bool bPilotProof = false;
    void TestPilotMovement(float Elapsed);
    bool bCirqueProof = false;
    bool bWeatherProof = false;
    int32 WeatherProofLightning=0;
    int32 WeatherProofCapturedLightning=0;
    void TestWeather(float Elapsed);
    void TestCirque(float Elapsed);
    void TestBungee(float Elapsed);
    void TestWinch(float Elapsed);
    UPROPERTY()
    USceneComponent* SceneRoot;

    UPROPERTY()
    USceneComponent* PlaneVisual;

    UPROPERTY()
    UProceduralMeshComponent* WingMesh;

    UPROPERTY()
    UStaticMeshComponent* Fuselage;

    UPROPERTY()
    UStaticMeshComponent* Canopy;

    UPROPERTY()
    UStaticMeshComponent* LeftTail;

    UPROPERTY()
    UStaticMeshComponent* RightTail;

    UPROPERTY()
    UStaticMeshComponent* ImportedTopy;

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> AircraftTextures;
    bool bAppearanceReady = true;

    UPROPERTY()
    USpringArmComponent* CameraBoom;

    UPROPERTY()
    UCameraComponent* Camera;

    UPROPERTY()
    TObjectPtr<UAudioComponent> WindAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> WingAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> MotorAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> ServoAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> VarioAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> RainAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> FoliageAudio;
    UPROPERTY()
    TObjectPtr<UAudioComponent> ThunderAudio;
    int32 HeardLightningSerial=0;
    struct FPendingThunder { double Due; FVector Position; float Gain; };
    TArray<FPendingThunder> PendingThunder;
    float WeatherRainMix=0.f,WeatherFoliageMix=0.f;
    bool bVarioEnabled = false;
    float ClimbThreshold = .3f, SinkThreshold = -2.f;
    float VarioPhase = 0.f, FilteredClimb = 0.f;
    double LastServoTravel = 0.;
    float AudioElapsed = 0.f;
    void PrepareFlightAudio();
    void UpdateFlightAudio(float DeltaSeconds);

    FGliderDynamics Flight;
    FGliderDynamics HangarModels[9];
    bool AvailableModels[UE_ARRAY_COUNT(HangarModels)] = {};
    bool AileronSteering[UE_ARRAY_COUNT(HangarModels)] = {};
    int32 SelectedModel = 0, PreviewModel = 0;
    double WeatherSeconds = 0.;
    double EnvironmentSeconds = 0.;
    double RidgeLift = 0.;
    FWindTrails WindTrails;
    UPROPERTY()
    ULineBatchComponent* AirLines;
    bool bShowWindTrails = true;
    double TimeBank = 0.;
    FTransform PreviousPose = FTransform::Identity;
    FVector TargetPosition = FVector(0.0f, 0.0f, 8000.0f);
    float AirspeedMps = 0.0f;
    float SinkMps = 0.0f;
    float AltitudeM = 80.0f;
    float Elevator = 0.0f;
    float Rudder = 0.0f;
    float StatsAccumulator = 0.0f;
    float PhysicsHz = 0.0f;
    int32 StepsThisSecond = 0;
    bool bModelReady = false;
    enum class EFlightView : uint8 { Ground, Chase, Wide, FPV };
    EFlightView FlightView = EFlightView::Ground;
    bool bShowInstruments = false;
    bool bInspectMeadow = false;
    bool bNoclip = false;
    FVector NoclipPosition = FVector::ZeroVector;
    FRotator NoclipRotation = FRotator::ZeroRotator;
    float NoclipSpeed = 30.f; // m/s, Shift multiplies by 10
    void ToggleNoclip();
    void MoveNoclip(float DeltaSeconds);
    FVector2D PilotOffset = FVector2D::ZeroVector;
    FVector PilotSite = FVector::ZeroVector;
    int32 SelectedSite=1;
    float LaunchHeading = 0.f;
    float WalkPitch = -3.f;
    void MoveWalking(float DeltaSeconds);
    void MovePilot(const FVector& Direction,float DeltaSeconds,bool Sprint);
    bool FindPilotGround(const FVector& Position, FHitResult& Hit,bool IncludeWater=false) const;
    void KeepNoclipAboveGround();
    FString SiteNotice;
    bool PlacePilotFromNoclip();
    bool bMeadowProof = false;
    int32 MeadowFrame = 0;
    bool bSmokeMode = false;
    bool bCaptureSite = false;
    bool bChaseCaptured = false;
    bool bSmokeScreenshotRequested = false;
    int32 FlightId = 0;
    FName ContactKind = NAME_None;
    bool bContactProof = false;
    int32 ProofFlight = 0;
    TSharedPtr<SFlightMenu> Menu;
    bool bMenuOpen = true;
    bool bFirstMenu = true;
    bool bHangarModel = false;
    bool bPosterHome = false;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> PosterPrint;
    FVector FuselageCentre = FVector::ZeroVector;
    FTransform MenuCameraPose() const;
    void PositionHangarModel();
    bool bStartedFlight = false;
    bool bHeldForLaunch = true;
    float ThrowSeconds = -1.f;
    FTransform HandPose(float Progress = 0.f) const;
    void BeginHandLaunch();
    bool bMenuProof = false;
    int32 MenuProofStep = 0;
    int32 MenuProofMass = 0;
    bool MenuProofTopySteering = false, MenuProofKSteering = false;
    FGliderControlGains MenuProofTopyGains;
    FVector MenuProofLocation = FVector::ZeroVector;
    FVector MenuProofCamera = FVector::ZeroVector;
    double MenuProofWeather = 0.;
    float MenuProofHeading = 0.f, MenuProofPitch = 0.f;
    void TestMenu(float Elapsed);
    double TourTime = 0.;
    float RadioScanTime = 0.f;
    float SmokeElapsed = 0.0f;

    void BuildFallbackTopy();
    void BuildWingSection();
    void ResetFlight();
    void StopFlightAt(const FTransform& Contact,bool PreserveView=false);
    void LookAtAircraftFromPilot(const FTransform& Pose);
public:
    int32 DamageLevel() const { return ImpactDamage; }
    EAircraftResistance ResistanceMode() const { return Resistance; }
    void SetResistanceMode(EAircraftResistance Mode);
    bool IsSettling() const { return bGroundPhysics; }
private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> LandingBody;
    int32 ImpactDamage=0;
    EAircraftResistance Resistance=EAircraftResistance::Realistic;
    bool bGroundPhysics=false, bLandingPaused=false;
    bool bLandingLookReleased=false;
    bool CanWalkAfterContact() const { return bGroundPhysics && !bMenuOpen && !bNoclip && !bHeldForLaunch && !IsThrowing(); }
    FVector LandingVelocity=FVector::ZeroVector, LandingAngular=FVector::ZeroVector;
    void BeginGroundPhysics(const FTransform& Pose,const FHitResult& Hit,double Length,double Span,double Rise,double Sweep,FVector4 Wheel);
    void UpdateGroundPhysics(float Dt);
    void ClearGroundPhysics();
    UFUNCTION() void LandingHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
    void AdvanceFlight(float DeltaSeconds);
    void ReadControls(float DeltaSeconds);
    void StepPilot(float Centimetres);
    void CycleCamera();
};
