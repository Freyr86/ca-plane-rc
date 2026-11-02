#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlightRadio.h"
#include "GliderDynamics.h"
#include "WindTrails.h"
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
    int32 ModelIndex() const { return PreviewModel; }
    bool CanSelectModel() const { return AvailableModels[PreviewModel]; }
    bool UsesAileronSteering() const { return AileronSteering[PreviewModel]; }
    bool IsControlInUse(EGliderControl Control) const;
    void BrowseModel(int32 Step);
    void ApplyModelSettings(FGliderLoading Setting, bool UseAilerons, FGliderControlGains Gains, FFlapTravel Travel, FGliderMotor Motor);
    bool IsRadioActive() const { return Radio.IsDriving(); }
    bool IsMenuOpen() const { return bMenuOpen; }
    bool HasStartedFlight() const { return bStartedFlight; }
    bool IsHeldForLaunch() const { return bHeldForLaunch; }
    bool UsesDiscusLaunch() const { return SelectedModel == 5; }
    bool IsThrowing() const { return ThrowSeconds >= 0.f; }
    bool IsWalking() const { return bHeldForLaunch && !IsThrowing() && !bNoclip && !bMenuOpen; }
    const TCHAR* FlightCameraName() const;
    bool IsNoclip() const { return bNoclip; }
    FVector ExplorationPosition() const { return NoclipPosition; }
    float ExplorationSpeed() const { return NoclipSpeed; }
    FVector PilotPosition() const;
    FString ExplorationNotice() const { return SiteNotice; }
    int32 LaunchSiteNumber() const { return SelectedSite; }
    void SwitchLaunchSite();
    void ExploreScene();
    void Fly();
    void OpenMenu();
    void ShowHangarModel(bool Visible);
    FFlightRadio Radio;
    FString GetContactLabel() const;
    float GetRadioAxis(int32 Index) const { return Radio.RawAxis(Index); }
    bool HasFlaps() const { return Flight.HasFlaps(); }
    double CrowFraction() const { return Flight.CrowFraction(); }
    double FlapAngleDeg() const { return Flight.FlapAngleDeg(); }
    FString GetControlMode() const { return Radio.HasDevice() ? Radio.Status() : TEXT("Clavier"); }
    bool ShowInstruments() const {return bShowInstruments;}

bool TrailsVisible() const { return bShowWindTrails; }
    int32 TrailDistance=90;
    void ShowTrails(bool Visible) { bShowWindTrails=Visible; }
private:
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
    double LastServoTravel = 0.;
    float AudioElapsed = 0.f;
    void PrepareFlightAudio();
    void UpdateFlightAudio(float DeltaSeconds);

    FGliderDynamics Flight;
    FGliderDynamics HangarModels[7];
    bool AvailableModels[UE_ARRAY_COUNT(HangarModels)] = {};
    bool AileronSteering[UE_ARRAY_COUNT(HangarModels)] = {};
    int32 SelectedModel = 0, PreviewModel = 0;
    double WeatherSeconds = 0.;
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
    bool bHangarModel = false;
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
    void AdvanceFlight(float DeltaSeconds);
    void ReadControls(float DeltaSeconds);
    void StepPilot(float Centimetres);
    void CycleCamera();
};
