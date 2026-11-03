#include "SlopeFlightPawn.h"
#include "Airfield.h"
#include "Misc/ConfigCacheIni.h"
#include "SlopeWorld.h"
#include "SceneContact.h"
#include "ValleyAir.h"
#include "Components/LineBatchComponent.h"
#include "FlightMenu.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"



namespace
{
    struct FHangarEntry
    {
        const TCHAR* Id;
        const TCHAR* Name;
        const TCHAR* Mesh;
        const TCHAR* Description;
        double SpanCm, LengthCm;
        double AssistedReleaseMps = 0.;
        double WingRisePerSpan = 7.5/59.5;
        FVector4 Wheel = FVector4(0,0,0,0); // Local centre and radius, centimetres.
        double TipSweepCm = 0., BodyLengthCm = 0.;
    };
    const FHangarEntry Aircraft[] = {
        {TEXT("Topy4522"), TEXT("Graupner Topy 4522"), TEXT("/Game/Aircraft/TopyCouleur.TopyCouleur"),TEXT("2 axes"),119.,86.},
        {TEXT("K3600"), TEXT("K3600"), TEXT("/Game/Aircraft/K3600Couleur.K3600Couleur"),TEXT("1,77 m de long · 79,1 dm²"),360.,177.},
        {TEXT("VJX2022"), TEXT("VJX F3F · 2022"), TEXT("/Game/Aircraft/VJX2022/VJX2022.VJX2022"),TEXT("F3F · vol expérimental"),293.,135.5},
        {TEXT("Fox7m"), TEXT("MDM-1 Fox · 7 m"), TEXT("/Game/Aircraft/Fox7mEffile/Fox7mEffile.Fox7mEffile"),TEXT("Voltige · lancer assisté"),700.,369.,22.,.081/3.5,FVector4(8,0,-22.437,13.5)},
        {TEXT("JS3_9m"), TEXT("Jonker JS-3 Rapture · 9 m"), TEXT("/Game/Aircraft/JS3_9m/JS3_9m.JS3_9m"),TEXT("Vol de distance"),900.,347.,21.,.18/4.5,FVector4(1,0,-26,9)},
        {TEXT("Snipe2"), TEXT("Snipe 2 · DLG"), TEXT("/Game/Aircraft/Snipe2/Snipe2.Snipe2"),TEXT("F3K · lancer disque"),149.,108.1,0.,.060/.745},
        {TEXT("LongBowMk2"), TEXT("LongBow Mk2 · Will Beavor"), TEXT("/Game/Aircraft/LongBowMk2/LongBowMk2.LongBowMk2"),TEXT("Aile volante à élevons"),215.5,59.3,0.,0.,FVector4(0,0,0,0),35.,33.},
        {TEXT("Apus4"), TEXT("FEMO APUS 4"), TEXT("/Game/Aircraft/Apus4/Apus4.Apus4"),TEXT("2 axes"),135.,99.,0.,.025},
        {TEXT("Nubis"), TEXT("Flight-Composites Nubis"), TEXT("/Game/Aircraft/Nubis/Nubis.Nubis"),TEXT("Thermique et pente · 112 dm²"),595.,221.,13.5,.372/2.975,FVector4(0,0,0,0),20.8}
    };
    constexpr double DiscusRadiusCm = 144.;
    constexpr double DiscusArc = 1.5*UE_PI;
    constexpr float DiscusDuration = .45f;
    FString LoadingSection(int32 Index) { return FString(TEXT("SimPlaneur.Loading.")) + Aircraft[Index].Id; }
    const FRotator MeadowView(-8.f,140.f,0.f);

    void AddPrism(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        const FVector& RootLeading,
        const FVector& RootTrailing,
        const FVector& TipLeading,
        const FVector& TipTrailing,
        float Thickness)
    {
        const int32 Start = Vertices.Num();
        for (float ZOffset : {-Thickness * 0.5f, Thickness * 0.5f})
        {
            Vertices.Append({
                RootLeading + FVector(0, 0, ZOffset), RootTrailing + FVector(0, 0, ZOffset),
                TipLeading + FVector(0, 0, ZOffset), TipTrailing + FVector(0, 0, ZOffset)
            });
        }
        const int32 Faces[] = {
            0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6,
            0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5,
            0, 1, 4, 1, 5, 4, 2, 6, 3, 3, 6, 7
        };
        for (int32 Index : Faces)
        {
            Triangles.Add(Start + Index);
        }
    }
}

ASlopeFlightPawn::ASlopeFlightPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    AirLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("FiletsAir"));
    AirLines->SetupAttachment(SceneRoot);
    AirLines->SetAbsolute(true, true, true);
    AirLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AirLines->SetCastShadow(false);
    PlaneVisual = CreateDefaultSubobject<USceneComponent>(TEXT("PlaneVisual"));
    PlaneVisual->SetupAttachment(SceneRoot);

    WingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FallbackWings"));
    WingMesh->SetupAttachment(PlaneVisual);
    WingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackFuselage"));
    Canopy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackCanopy"));
    LeftTail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftTail"));
    RightTail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightTail"));
    ImportedTopy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImportedTopy"));
    for (UStaticMeshComponent* Component : {Fuselage, Canopy, LeftTail, RightTail, ImportedTopy})
    {
        Component->SetupAttachment(PlaneVisual);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(SceneRoot);
    CameraBoom->TargetArmLength = 720.0f;
    CameraBoom->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 7.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 9.0f;
    CameraBoom->bDoCollisionTest = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->FieldOfView = 64.0f;

    SetActorLocation(TargetPosition);
}

FTransform ASlopeFlightPawn::MenuCameraPose() const
{
    const double T = TourTime * UE_TWO_PI / 94.;
    if (SelectedSite==3)
    {
        const FVector Look(Airfield::Point(0,400),ASlopeWorld::TerrainHeightCm(Airfield::Centre.X,Airfield::Centre.Y));
        const FVector2D Offset=Airfield::Point(-5900.+2400.*FMath::Sin(T),8100.+850.*FMath::Cos(T));
        const FVector Eye(Offset,FMath::Max(Look.Z+4400.,ASlopeWorld::TerrainHeightCm(Offset.X,Offset.Y)+2600.));
        return FTransform((Look-Eye).Rotation(),Eye);
    }
    const bool Chalet=SelectedSite==2;
    const double X=Chalet?ASlopeWorld::ChaletX:ASlopeWorld::PilotX+445;
    const double Y=Chalet?ASlopeWorld::ChaletY:ASlopeWorld::PilotY-195;
    const FVector Look(X,Y,ASlopeWorld::TerrainHeightCm(X,Y)+(Chalet?230.:140.));
    const double Angle=(Chalet?.6:-.55)+.55*FMath::Sin(T);
    const double Range=Chalet?2350.:1800.;
    FVector Eye=Look+FVector(Range*FMath::Cos(Angle),Range*FMath::Sin(Angle),Chalet?820.:410.);
    Eye.Z=FMath::Max(Eye.Z,ASlopeWorld::TerrainHeightCm(Eye.X,Eye.Y)+360.);
    FRotator Aim=(Look-Eye).Rotation();
    Aim.Pitch=FMath::Max(Aim.Pitch,-5.);
    return FTransform(Aim,Eye);
}

void ASlopeFlightPawn::ShowMenuPresentation(bool Hangar,bool Poster)
{
    // Only the hangar previews unconfirmed aircraft. The home poster and flight
    // both display the saved selection, including after cancelling a preview.
    if (!Hangar && PreviewModel != SelectedModel)
    {
        PreviewModel = SelectedModel;
        BuildFallbackTopy();
    }
    bPosterHome = Poster && bMenuOpen;
    bHangarModel = (Hangar || Poster) && bMenuOpen;
    if (bHangarModel) PositionHangarModel();
    else PlaneVisual->SetRelativeTransform(FTransform::Identity);
    SetActorHiddenInGame(bMenuOpen && !bHangarModel);
}

void ASlopeFlightPawn::PositionHangarModel()
{
    const FTransform View = MenuCameraPose();
    if(bPosterHome)
    {
        // Frame the selected aircraft as the diagonal of the poster. Only the display
        // component is posed here; the aircraft and its simulation keep their scale.
        const double Distance=Aircraft[SelectedModel].SpanCm*1.60;
        const FVector Offset(1.,.21+.018*FMath::Sin(TourTime*.15),.012+.014*FMath::Sin(TourTime*.27));
        const FRotator Pose(-10.,127.+5.*FMath::Sin(TourTime*.16),27.+4.*FMath::Sin(TourTime*.24));
        PlaneVisual->SetWorldLocationAndRotation(View.TransformPosition(Offset*Distance),View.GetRotation()*Pose.Quaternion());
        return;
    }
    const FRotator Presentation(-18.,135.+18.*FMath::Sin(TourTime*.12),22.);
    // Common viewing distance makes wingspans directly comparable while browsing.
    PlaneVisual->SetWorldLocationAndRotation(View.TransformPosition(FVector(1300,295,-25)/HangarZoom),
        View.GetRotation()*Presentation.Quaternion());
}



void ASlopeFlightPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    if (bMenuOpen)
    {
        const FTransform View = MenuCameraPose();
        OutResult.Location = View.GetLocation();
        OutResult.Rotation = View.Rotator();
        OutResult.FOV = 64.f;
        if(bPosterHome && PosterPrint) OutResult.PostProcessSettings.AddBlendable(PosterPrint,1.f);
    }
    else if (bNoclip)
    {
        OutResult.Location = NoclipPosition;
        OutResult.Rotation = NoclipRotation;
        OutResult.FOV = 75.f;
    }
    else if (FlightView == EFlightView::FPV)
    {
        // Mount just beyond the nose so opaque canopies never block the lens.
        const double Length = Aircraft[SelectedModel].LengthCm;
        double Nose = FuselageCentre.X + .5*Length;
        if (const UStaticMesh* Mesh = ImportedTopy->GetStaticMesh())
            Nose = Mesh->GetBoundingBox().TransformBy(ImportedTopy->GetRelativeTransform()).Max.X;
        OutResult.Location = GetActorTransform().TransformPosition(FVector(Nose+2.,0.,FuselageCentre.Z+.035*Length));
        OutResult.Rotation = GetActorRotation();
        OutResult.FOV = 90.f;
    }
    else if (FlightView == EFlightView::Ground)
    {
        OutResult.Location = PilotPosition() + FVector(0,0,bInspectMeadow ? 72.f : 170.f);
        OutResult.Rotation = (GetActorLocation() - OutResult.Location).Rotation();
        // Keep the aircraft slightly above centre so the hillside stays in view.
        OutResult.Rotation.Pitch = FMath::Max(-89.f, OutResult.Rotation.Pitch - 7.f);
        if (bInspectMeadow) OutResult.Rotation = MeadowView;
        OutResult.FOV = bInspectMeadow ? 64.f : 50.f;
        if (bHeldForLaunch && !bInspectMeadow)
        {
            OutResult.Rotation = FRotator(WalkPitch,LaunchHeading,0.f);
            OutResult.FOV = 75.f;
        }
    }
    OutResult.PostProcessSettings.bOverride_AutoExposureMethod = true;
    OutResult.PostProcessSettings.AutoExposureMethod = AEM_Manual;
    OutResult.PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    OutResult.PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    OutResult.PostProcessSettings.bOverride_AutoExposureBias = true;
    OutResult.PostProcessSettings.AutoExposureBias = -12.6f;
    OutResult.PostProcessBlendWeight = 1.f;
}

void ASlopeFlightPawn::BeginPlay()
{
    Super::BeginPlay();
    PosterPrint=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_MenuAlpinePrint.M_MenuAlpinePrint"));
    ValleyAir::LoadSettings();
    PrepareFlightAudio();
    GConfig->GetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowTrails"),bShowWindTrails,GGameUserSettingsIni);
    GConfig->GetInt(TEXT("SimPlaneur.Graphics"),TEXT("TrailDistance"),TrailDistance,GGameUserSettingsIni);
    TrailDistance=FMath::Clamp(TrailDistance,20,500);
    PilotSite=FVector(ASlopeWorld::PilotX,ASlopeWorld::PilotY,
        ASlopeWorld::TerrainHeightCm(ASlopeWorld::PilotX,ASlopeWorld::PilotY));
    bSmokeMode = FParse::Param(FCommandLine::Get(), TEXT("SimSmoke"));
    bContactProof = bSmokeMode && FParse::Param(FCommandLine::Get(), TEXT("ContactProof"));
    bCaptureSite = FParse::Param(FCommandLine::Get(), TEXT("CaptureSite"));
    bMeadowProof = bSmokeMode && FParse::Param(FCommandLine::Get(), TEXT("MeadowProof"));
    bMenuProof = bSmokeMode && FParse::Param(FCommandLine::Get(), TEXT("MenuProof"));
    bMenuOpen = !bSmokeMode || bMenuProof;
    bStartedFlight = !bMenuOpen;
    SetActorHiddenInGame(bMenuOpen);
    Radio.Initialize();
    static_assert(UE_ARRAY_COUNT(HangarModels) == UE_ARRAY_COUNT(Aircraft));
    for (int32 I = 0; I < UE_ARRAY_COUNT(Aircraft); ++I)
    {
        const FString Path = FPaths::ProjectContentDir() / TEXT("FlightModels") / (FString(Aircraft[I].Id) + TEXT(".json"));
        AvailableModels[I] = HangarModels[I].LoadModel(Path) &&
            (I == 0 || LoadObject<UStaticMesh>(nullptr, Aircraft[I].Mesh) != nullptr);
        if (!AvailableModels[I] || bContactProof) continue;
        FGliderLoading Setting = HangarModels[I].StandardLoading();
        const FString Section = LoadingSection(I);
        GConfig->GetInt(*Section, TEXT("MassGrams"), Setting.MassGrams, GGameUserSettingsIni);
        GConfig->GetInt(*Section, TEXT("CgMm"), Setting.CgMm, GGameUserSettingsIni);
        GConfig->GetBool(*Section, TEXT("AileronSteering"), AileronSteering[I], GGameUserSettingsIni);
        HangarModels[I].SetLoading(Setting);
        FGliderControlGains Gains;
        GConfig->GetInt(*Section,TEXT("RollGainPercent"),Gains.Roll,GGameUserSettingsIni);
        GConfig->GetInt(*Section,TEXT("PitchGainPercent"),Gains.Pitch,GGameUserSettingsIni);
        GConfig->GetInt(*Section,TEXT("YawGainPercent"),Gains.Yaw,GGameUserSettingsIni);
        HangarModels[I].SetControlGains(Gains);
        FFlapTravel Travel=HangarModels[I].DefaultFlapTravel();
        GConfig->GetInt(*Section,TEXT("FlapUpDeg"),Travel.Up,GGameUserSettingsIni);
        GConfig->GetInt(*Section,TEXT("FlapDownDeg"),Travel.Down,GGameUserSettingsIni);
        HangarModels[I].SetFlapTravel(Travel);
        FGliderMotor Motor = HangarModels[I].MotorSettings();
        GConfig->GetBool(*Section,TEXT("MotorEnabled"),Motor.Enabled,GGameUserSettingsIni);
        GConfig->GetInt(*Section,TEXT("MotorMaxWatts"),Motor.Watts,GGameUserSettingsIni);
        HangarModels[I].SetMotor(Motor);
    }
    FString Id;
    if (!bSmokeMode && GConfig->GetString(TEXT("SimPlaneur.Hangar"), TEXT("Aircraft"), Id, GGameUserSettingsIni))
        for (int32 I = 0; I < UE_ARRAY_COUNT(Aircraft); ++I)
            if (AvailableModels[I] && (Id == Aircraft[I].Id || (I == 1 && Id == TEXT("Alize")))) SelectedModel = I;
    PreviewModel = SelectedModel;
    BuildFallbackTopy();
    bModelReady = AvailableModels[SelectedModel];
    if (bModelReady)
    {
        Flight = HangarModels[SelectedModel];
        ResetFlight();
        UE_LOG(LogTemp,Display,TEXT("NATIVE_FLIGHT ready: local 6-DOF, 120 Hz, %s"),ModelName());
    }
    else UE_LOG(LogTemp,Error,TEXT("Cannot load flight model %s"),*FGliderDynamics::TopyModelPath());
}

void ASlopeFlightPawn::BuildWingSection()
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    AddPrism(Vertices, Triangles, FVector(10.5f, 0.8f, 0.0f), FVector(-8.1f, 0.8f, 0.0f), FVector(7.6f, 59.5f, 7.5f), FVector(-5.9f, 59.5f, 7.5f), 1.6f);
    AddPrism(Vertices, Triangles, FVector(10.5f, -0.8f, 0.0f), FVector(-8.1f, -0.8f, 0.0f), FVector(7.6f, -59.5f, 7.5f), FVector(-5.9f, -59.5f, 7.5f), 1.6f);
    Normals.Init(FVector::UpVector, Vertices.Num());
    UVs.Init(FVector2D::ZeroVector, Vertices.Num());
    Colors.Init(FLinearColor(0.92f, 0.90f, 0.78f), Vertices.Num());
    WingMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/VertexColorMaterial.VertexColorMaterial")))
    {
        WingMesh->SetMaterial(0, Material);
    }
}

const TCHAR* ASlopeFlightPawn::ModelName() const { return Aircraft[bPosterHome ? SelectedModel : PreviewModel].Name; }

FString ASlopeFlightPawn::ModelNumber() const
{
    int32 Position = 0, Total = 0;
    for (int32 I = 0; I < UE_ARRAY_COUNT(Aircraft); ++I)
    {
        if (!AvailableModels[I]) continue;
        ++Total;
        if (I <= PreviewModel) ++Position;
    }
    return FString::Printf(TEXT("%02d / %02d"), Position, Total);
}

double ASlopeFlightPawn::ModelSpanCm() const { return Aircraft[PreviewModel].SpanCm; }

FString ASlopeFlightPawn::HangarSummary() const
{
    const FString Metres = FString::SanitizeFloat(ModelSpanCm() / 100., 2).Replace(TEXT("."), TEXT(","));
    return FString::Printf(TEXT("%s   ·   %s m   ·   %s"), *ModelNumber(), *Metres, Aircraft[PreviewModel].Description);
}

bool ASlopeFlightPawn::IsControlInUse(EGliderControl Control) const
{
    using A = EGliderControl;
    if (Control == A::Motor || Control == A::MotorCut) return MotorSettings().Enabled;
    if (Control == A::Butterfly && (PreviewModel == 5 || PreviewModel == 7)) return false;
    if (Control == A::Flaps || Control == A::Butterfly) return HangarModels[PreviewModel].HasFlaps();
    if (Control == A::Spoilers) return HangarModels[PreviewModel].HasSpoilers();
    if (!HasRudder())
    {
        if (Control == A::Aileron || Control == A::TrimAileron) return !UsesAileronSteering();
        if (Control == A::Rudder || Control == A::TrimRudder) return UsesAileronSteering();
    }
    if (Control == A::Meadow) return false; // Replaced by mouse look while walking.
    if (Control == A::Aileron || Control == A::TrimAileron) return PreviewModel >= 2 || UsesAileronSteering();
    if (Control == A::Rudder || Control == A::TrimRudder) return PreviewModel >= 2 || !UsesAileronSteering();
    return ControlSpec(Control).OnTopy;
}

void ASlopeFlightPawn::BrowseModel(int32 Step)
{
    for (int32 I = 0; I < UE_ARRAY_COUNT(Aircraft); ++I)
    {
        PreviewModel = (PreviewModel + Step + UE_ARRAY_COUNT(Aircraft)) % UE_ARRAY_COUNT(Aircraft);
        if (AvailableModels[PreviewModel]) break;
    }
    BuildFallbackTopy();
}

void ASlopeFlightPawn::BuildFallbackTopy()
{
    ImportedTopy->SetTextureForceResidentFlag(false);
    AircraftTextures.Reset();
    bAppearanceReady = true;
    UStaticMesh* Imported = LoadObject<UStaticMesh>(nullptr, Aircraft[PreviewModel].Mesh);
    if (Imported && PreviewModel >= 2)
    {
        ImportedTopy->SetRelativeTransform(FTransform::Identity);
        FuselageCentre = PreviewModel == 7 ? FVector(-17.5,0.,0.) : PreviewModel == 6 ? FVector(5.5,0.,0.) : PreviewModel == 5 ? FVector(-28.05,0.,-.5) : PreviewModel == 4 ? FVector(-55.5,0.,-8.) : PreviewModel == 3 ? FVector(-59.5,0.,-5.) : FVector(-26.75,0.,-.5);
        if (PreviewModel == 8) FuselageCentre = FVector(Imported->GetBoundingBox().GetCenter().X,0.,0.);
    }
    else if (Imported)
    {
        // Keep the wing at the existing flight datum. The FBX origin lies aft of its wing.
        const FVector Dimensions = Imported->GetBoundingBox().GetSize();
        const bool K3600 = PreviewModel == 1;
        const FHangarEntry& Entry = Aircraft[PreviewModel];
        const double SpanScale = Entry.SpanCm / Dimensions.Y;
        ImportedTopy->SetRelativeRotation(FRotator(0.,180.,0.));
        ImportedTopy->SetRelativeScale3D(FVector(Entry.LengthCm / Dimensions.X, SpanScale, SpanScale));
        ImportedTopy->SetRelativeLocation(K3600 ? FVector(-25.305,0.,12.826) : FVector(-11.,0.,1.5));
        FuselageCentre = K3600 ? FVector(-24.058,0.,-5.654) : FVector(-11.,0.,-2.6);
    }
    else Imported = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Aircraft/Topy4522V0.Topy4522V0"));
    if (Imported)
    {
        UE_LOG(LogTemp, Display, TEXT("SimPlaneur: aircraft mesh %s, material %s"), *Imported->GetName(), *GetNameSafe(Imported->GetMaterial(0)));
        ImportedTopy->SetStaticMesh(Imported);
        // Load all aircraft texture levels before revealing the mesh. Hidden previews
        // otherwise receive only low-resolution mips from distance-based streaming.
        ImportedTopy->SetTextureForceResidentFlag(true);
        for (UMaterialInterface* Material : ImportedTopy->GetMaterials())
        {
            if (!Material) continue;
            TArray<UTexture*> Textures;
            Material->GetUsedTextures(Textures);
            for (UTexture* Texture : Textures)
                if (auto* Texture2D = Cast<UTexture2D>(Texture)) AircraftTextures.AddUnique(Texture2D);
        }
        bAppearanceReady = AircraftTextures.IsEmpty();
        ImportedTopy->SetVisibility(bAppearanceReady);
        WingMesh->SetVisibility(false);
        Fuselage->SetVisibility(false);
        Canopy->SetVisibility(false);
        LeftTail->SetVisibility(false);
        RightTail->SetVisibility(false);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SimPlaneur: imported Topy mesh unavailable; using fallback mesh"));
    BuildWingSection();
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cylinder || !Sphere || !Cube)
    {
        return;
    }
    Fuselage->SetStaticMesh(Cylinder);
    Fuselage->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    Fuselage->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.86f));
    Canopy->SetStaticMesh(Sphere);
    Canopy->SetRelativeLocation(FVector(27.0f, 0.0f, 3.5f));
    Canopy->SetRelativeScale3D(FVector(0.16f, 0.075f, 0.055f));
    for (UStaticMeshComponent* Tail : {LeftTail, RightTail})
    {
        Tail->SetStaticMesh(Cube);
        Tail->SetRelativeLocation(FVector(-36.0f, Tail == LeftTail ? -9.0f : 9.0f, 7.0f));
        Tail->SetRelativeScale3D(FVector(0.11f, 0.20f, 0.012f));
        Tail->SetRelativeRotation(FRotator(0.0f, 0.0f, Tail == LeftTail ? -35.0f : 35.0f));
    }
}

void ASlopeFlightPawn::StepPilot(float Centimetres)
{
    const FVector Direction = bInspectMeadow ? MeadowView.Vector() : bHeldForLaunch ? FRotator(0,LaunchHeading,0).Vector()
        : GetActorLocation() - PilotPosition();
    const FVector2D Right = FVector2D(-Direction.Y,Direction.X).GetSafeNormal();
    PilotOffset = (PilotOffset + Centimetres*Right).GetClampedToMaxSize(300.f);
}

const TCHAR* ASlopeFlightPawn::FlightCameraName() const
{
    switch (FlightView)
    {
    case EFlightView::Chase: return TEXT("Poursuite");
    case EFlightView::Wide: return TEXT("Poursuite large");
    case EFlightView::FPV: return TEXT("FPV");
    default: return TEXT("Sol");
    }
}

void ASlopeFlightPawn::CycleCamera()
{
    bInspectMeadow = false;
    switch (FlightView)
    {
    case EFlightView::Ground: FlightView = EFlightView::Chase; break;
    case EFlightView::Chase: FlightView = EFlightView::Wide; break;
    case EFlightView::Wide: FlightView = EFlightView::FPV; break;
    case EFlightView::FPV: FlightView = EFlightView::Ground; break;
    }
    const bool bWideCamera = FlightView == EFlightView::Wide;
    CameraBoom->TargetArmLength = bWideCamera ? 1500.0f : 720.0f;
    CameraBoom->SetRelativeRotation(FRotator(bWideCamera ? -24.0f : -15.0f, 0.0f, 0.0f));
}

void ASlopeFlightPawn::ReadControls(float DeltaSeconds)
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController)
    {
        return;
    }
    using A=EGliderControl;
    if (PlayerController->WasInputKeyJustPressed(EKeys::F2))
    {
        OpenMenu();
        if(Menu.IsValid()) Menu->OpenMapFromFlight();
        return;
    }
    if (Radio.Pressed(A::Menu) || PlayerController->WasInputKeyJustPressed(EKeys::Escape))
    {
        OpenMenu();
        return;
    }
    if (Radio.Pressed(A::Noclip))
    {
        ToggleNoclip();
        return;
    }
    if (bNoclip)
    {
        MoveNoclip(DeltaSeconds);
        return;
    }
    const float Rate=Radio.Command(A::LowRates)>.5f?.7f:1.f;
    const float TargetRudder=Flight.HasRudder() ? Radio.SteeringCommand(AileronSteering[SelectedModel]) : 0.f;
    const float TargetElevator=FMath::Clamp(Rate*Radio.Command(A::Elevator)+Radio.Command(A::TrimElevator),-1.f,1.f);
    Elevator = Radio.IsDriving() ? TargetElevator : FMath::FInterpConstantTo(Elevator, TargetElevator, DeltaSeconds, 3.0f);
    Rudder = Radio.IsDriving() ? TargetRudder : FMath::FInterpConstantTo(Rudder, TargetRudder, DeltaSeconds, 3.0f);
    if (Radio.Pressed(A::Relaunch))
    {
        ResetFlight();
    }
    else
    {
        if (IsWalking()) MoveWalking(DeltaSeconds);
        if (Radio.Pressed(A::HandLaunch)) BeginHandLaunch();
    }
    if (Radio.Pressed(A::Instruments)) bShowInstruments=!bShowInstruments;
    if (Radio.Pressed(A::Vario)) SetVario(!bVarioEnabled,ClimbThreshold,SinkThreshold);
    if (Radio.Pressed(A::WindTrails))
    {
        bShowWindTrails = !bShowWindTrails;
        GConfig->SetBool(TEXT("SimPlaneur.Graphics"),TEXT("ShowTrails"),bShowWindTrails,GGameUserSettingsIni);
        GConfig->Flush(false,GGameUserSettingsIni);
    }
    if (Radio.Pressed(A::Camera) && !bHeldForLaunch)
    {
        CycleCamera();
    }
    const float LeftMix = FMath::Clamp(Elevator + Rudder, -1.0f, 1.0f);
    const float RightMix = FMath::Clamp(Elevator - Rudder, -1.0f, 1.0f);
    LeftTail->SetRelativeRotation(FRotator(LeftMix * 15.0f, 0.0f, -35.0f));
    RightTail->SetRelativeRotation(FRotator(RightMix * 15.0f, 0.0f, 35.0f));
}

void ASlopeFlightPawn::ToggleNoclip()
{
    if (!bNoclip)
    {
        FMinimalViewInfo View;
        CalcCamera(0.f,View);
        NoclipPosition=View.Location;
        KeepNoclipAboveGround();
        NoclipRotation=View.Rotation;
        NoclipRotation.Roll=0.f;
        SiteNotice.Reset();
        bNoclip=true;
    }
    else if (!PlacePilotFromNoclip()) return;
    // Do not let an exploration key held at exit trigger a throw or a reset.
    Radio.LeaveMenu();
    UE_LOG(LogTemp,Display,TEXT("NOCLIP active=%d"),bNoclip);
}

FVector ASlopeFlightPawn::PilotPosition() const
{
    const double X=PilotSite.X+PilotOffset.X, Y=PilotSite.Y+PilotOffset.Y;
    // Preserve the collision surface elevation at the chosen site. Small steps
    // follow the terrain slope relative to that datum.
    return FVector(X,Y,PilotSite.Z+ASlopeWorld::TerrainHeightCm(X,Y)
        -ASlopeWorld::TerrainHeightCm(PilotSite.X,PilotSite.Y));
}

FVector2D ASlopeFlightPawn::MapPositionXY() const
{
    const FVector P=bNoclip?NoclipPosition:(bHeldForLaunch?PilotPosition():GetActorLocation());
    return FVector2D(P.X*.01,P.Y*.01);
}

bool ASlopeFlightPawn::FindPilotGround(const FVector& Position, FHitResult& Hit,bool IncludeWater) const
{
    const FVector Above(Position.X,Position.Y,1000000.);
    const FVector Below(Position.X,Position.Y,-1000000.);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(NoclipSite),false,this);
    // Ignore tree canopies/trunks when looking for the actual ground below.
    for (int32 I=0;I<32;++I)
    {
        if (!GetWorld()->LineTraceSingleByChannel(Hit,Above,Below,SceneContact::Channel,Query)) break;
        const UPrimitiveComponent* Surface=Hit.GetComponent();
        if (!Surface) break;
        if (Surface->ComponentHasTag(TEXT("water"))) return IncludeWater;
        if (Surface->ComponentHasTag(TEXT("ground")) || Surface->ComponentHasTag(TEXT("rock")) || Surface->ComponentHasTag(TEXT("dock")) || Surface->ComponentHasTag(TEXT("terrace"))) return true;
        Query.AddIgnoredComponent(Surface);
    }
    return false;
}

bool ASlopeFlightPawn::PlacePilotFromNoclip()
{
    FHitResult Hit;
    if (!FindPilotGround(NoclipPosition,Hit))
    {
        SiteNotice=TEXT("Rejoins un sol ferme pour passer en mode marche.");
        return false;
    }
    PilotSite=Hit.ImpactPoint;
    PilotOffset=FVector2D::ZeroVector;
    LaunchHeading=NoclipRotation.Yaw;
    WalkPitch=FMath::Clamp(NoclipRotation.Pitch,-80.f,80.f);
    FlightView=EFlightView::Ground;
    bInspectMeadow=false;
    SiteNotice.Reset();
    ResetFlight();
    UE_LOG(LogTemp,Display,TEXT("PILOT_SITE x=%.2f y=%.2f z=%.2f heading=%.1f"),
        PilotSite.X*.01,PilotSite.Y*.01,PilotSite.Z*.01,LaunchHeading);
    return true;
}

void ASlopeFlightPawn::MoveWalking(float DeltaSeconds)
{
    if (!IsWalking()) return;
    auto* PC=CastChecked<APlayerController>(GetController());
    const float MouseX=PC->PlayerInput ? PC->PlayerInput->GetRawKeyValue(EKeys::MouseX) : 0.f;
    const float MouseY=PC->PlayerInput ? PC->PlayerInput->GetRawKeyValue(EKeys::MouseY) : 0.f;
    LaunchHeading=FRotator::NormalizeAxis(LaunchHeading+MouseX*.14f);
    WalkPitch=FMath::Clamp(WalkPitch+MouseY*.14f,-80.f,80.f);
    auto Down=[PC](FKey Key){return PC->IsInputKeyDown(Key);};
    const float Forward=float(Down(EKeys::W)||Down(EKeys::Z)||Down(EKeys::Up))-float(Down(EKeys::S)||Down(EKeys::Down));
    const float Right=FMath::Clamp(float(Down(EKeys::D)||Down(EKeys::Right))-float(Down(EKeys::A)||Down(EKeys::Q)||Down(EKeys::Left))
        +Radio.Command(EGliderControl::Sidestep),-1.f,1.f);
    const FVector Direction=FRotator(0,LaunchHeading,0).RotateVector(FVector(Forward,Right,0).GetClampedToMaxSize(1.));
    if (Direction.IsNearlyZero()) return;
    const FVector Start=PilotPosition();
    const FVector Desired=Start+Direction*(120.f*FMath::Clamp(DeltaSeconds,0.f,.05f));
    FHitResult Ground;
    if (!FindPilotGround(Desired,Ground) || Ground.ImpactNormal.Z<.55f || FMath::Abs(Ground.ImpactPoint.Z-Start.Z)>45.f) return;
    // Follow the collision terrain, with a body capsule to stop at trunks and rocks.
    FHitResult Obstacle;
    const FVector BodyHeight(0,0,95);
    if (GetWorld()->SweepSingleByChannel(Obstacle,Start+BodyHeight,Ground.ImpactPoint+BodyHeight,
        FQuat::Identity,SceneContact::Channel,FCollisionShape::MakeCapsule(24,70),
        FCollisionQueryParams(SCENE_QUERY_STAT(WalkingPilot),false,this))) return;
    PilotSite=Ground.ImpactPoint;
    PilotOffset=FVector2D::ZeroVector;
}

void ASlopeFlightPawn::SwitchLaunchSite(int32 Step)
{
    if (!bMenuOpen || !bModelReady) return;
    constexpr int32 Count=3;
    const int32 Next=(SelectedSite-1+Step+Count)%Count+1;
    const FVector Landing=Next==3 ? FVector(Airfield::Pilot,0) : Next==2 ? FVector(ASlopeWorld::ChaletX+430,ASlopeWorld::ChaletY,0)
                                                                         : FVector(ASlopeWorld::PilotX,ASlopeWorld::PilotY,0);
    FHitResult Ground;
    if (!FindPilotGround(Landing,Ground)) return;
    SelectedSite=Next;
    PilotSite=Ground.ImpactPoint;
    PilotOffset=FVector2D::ZeroVector;
    LaunchHeading=Next==3 ? 122.f : 0.f;
    WalkPitch=0;
    bStartedFlight=false;
    SiteNotice.Reset();
    ResetFlight();
}

void ASlopeFlightPawn::ExploreScene()
{
    Fly();
    if (!bMenuOpen && !bNoclip) ToggleNoclip();
}

void ASlopeFlightPawn::KeepNoclipAboveGround()
{
    FHitResult Hit;
    if (FindPilotGround(NoclipPosition,Hit,true))
        NoclipPosition.Z=FMath::Max(NoclipPosition.Z,Hit.ImpactPoint.Z+85.);
}

void ASlopeFlightPawn::MoveNoclip(float DeltaSeconds)
{
    auto* PC=CastChecked<APlayerController>(GetController());
    // Pixel deltas avoid applying the legacy flight-axis sensitivity a second time.
    const float MouseX=PC->PlayerInput ? PC->PlayerInput->GetRawKeyValue(EKeys::MouseX) : 0.f;
    const float MouseY=PC->PlayerInput ? PC->PlayerInput->GetRawKeyValue(EKeys::MouseY) : 0.f;
    NoclipRotation.Yaw=FRotator::NormalizeAxis(NoclipRotation.Yaw+MouseX*.14f);
    NoclipRotation.Pitch=FMath::Clamp(NoclipRotation.Pitch+MouseY*.14f,-89.f,89.f);
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) NoclipSpeed=FMath::Min(500.f,NoclipSpeed*1.5f);
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) NoclipSpeed=FMath::Max(2.f,NoclipSpeed/1.5f);
    auto Down=[PC](FKey Key){return PC->IsInputKeyDown(Key);};
    const float Forward=float(Down(EKeys::W)||Down(EKeys::Z)||Down(EKeys::Up))-float(Down(EKeys::S)||Down(EKeys::Down));
    const float Right=float(Down(EKeys::D)||Down(EKeys::Right))-float(Down(EKeys::A)||Down(EKeys::Q)||Down(EKeys::Left));
    const float Up=float(Down(EKeys::SpaceBar))-float(Down(EKeys::LeftControl)||Down(EKeys::RightControl));
    const FVector Direction=NoclipRotation.Vector()*Forward+FRotationMatrix(NoclipRotation).GetUnitAxis(EAxis::Y)*Right+FVector::UpVector*Up;
    const float Boost=(Down(EKeys::LeftShift)||Down(EKeys::RightShift))?10.f:1.f;
    const FVector Travel=Direction.GetClampedToMaxSize(1.)*(NoclipSpeed*100.f*Boost*FMath::Clamp(DeltaSeconds,0.f,.1f));
    if (Travel.IsNearlyZero()) return;
    // Follow narrow ridges even when boost carries the camera across several terrain cells.
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(Travel.Size2D()/250.));
    for (int32 I=0; I<Steps; ++I)
    {
        NoclipPosition+=Travel/Steps;
        KeepNoclipAboveGround();
    }
}

void ASlopeFlightPawn::OpenMenu()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !GEngine || !GEngine->GameViewport || Menu.IsValid()) return;
    bMenuOpen = true;
    Elevator = Rudder = 0.f;
    SetActorHiddenInGame(true);
    Menu = SNew(SFlightMenu).Plane(this)
        .ShowStartupPoster(bFirstMenu && !FParse::Param(FCommandLine::Get(), TEXT("NoSplash")));
    bFirstMenu = false;
    GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 20);
    PC->bShowMouseCursor = true;
    FInputModeUIOnly Input;
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Input.SetWidgetToFocus(Menu.ToSharedRef());
    PC->SetInputMode(Input);
    Menu->FocusFirst();
}

void ASlopeFlightPawn::Fly()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!bModelReady || !bAppearanceReady || !PC || !Menu.IsValid()) return;
    GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    bMenuOpen = false;
    Radio.LeaveMenu();
    bStartedFlight = true;
    ShowMenuPresentation(false,false);
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void ASlopeFlightPawn::ApplyModelSettings(FGliderLoading Setting, bool UseAilerons, FGliderControlGains Gains, FFlapTravel Travel, FGliderMotor Motor)
{
    if (!CanSelectModel() || !bMenuOpen) return;
    Setting = LimitLoading(Setting);
    if (SelectedModel == PreviewModel && Setting == Flight.Loading() && UseAilerons == AileronSteering[SelectedModel]
        && Gains == Flight.ControlGains() && Travel == Flight.FlapTravel() && Motor == Flight.MotorSettings()) return;
    SelectedModel = PreviewModel;
    AileronSteering[SelectedModel] = UseAilerons;
    HangarModels[SelectedModel].SetLoading(Setting);
    HangarModels[SelectedModel].SetControlGains(Gains);
    HangarModels[SelectedModel].SetFlapTravel(Travel);
    HangarModels[SelectedModel].SetMotor(Motor);
    Flight = HangarModels[SelectedModel];
    Gains = Flight.ControlGains();
    bModelReady = true;
    const FString Section = LoadingSection(SelectedModel);
    GConfig->SetBool(*Section,TEXT("MotorEnabled"),Flight.MotorSettings().Enabled,GGameUserSettingsIni);
    GConfig->SetInt(*Section,TEXT("MotorMaxWatts"),Flight.MotorSettings().Watts,GGameUserSettingsIni);
    GConfig->SetInt(*Section, TEXT("MassGrams"), Setting.MassGrams, GGameUserSettingsIni);
    GConfig->SetInt(*Section, TEXT("CgMm"), Setting.CgMm, GGameUserSettingsIni);
    if (Flight.HasFlaps())
    {
        GConfig->SetInt(*Section,TEXT("FlapUpDeg"),Flight.FlapTravel().Up,GGameUserSettingsIni);
        GConfig->SetInt(*Section,TEXT("FlapDownDeg"),Flight.FlapTravel().Down,GGameUserSettingsIni);
    }
    GConfig->SetBool(*Section, TEXT("AileronSteering"), UseAilerons, GGameUserSettingsIni);
    GConfig->SetInt(*Section,TEXT("RollGainPercent"),Gains.Roll,GGameUserSettingsIni);
    GConfig->SetInt(*Section,TEXT("PitchGainPercent"),Gains.Pitch,GGameUserSettingsIni);
    GConfig->SetInt(*Section,TEXT("YawGainPercent"),Gains.Yaw,GGameUserSettingsIni);
    GConfig->SetString(TEXT("SimPlaneur.Hangar"), TEXT("Aircraft"), Aircraft[SelectedModel].Id, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    bStartedFlight = false;
    ResetFlight();
}

void ASlopeFlightPawn::ResetFlight()
{
    bNoclip=false;
    Flight.Reset();
    // Historical smoke/contact captures keep their airborne fixture. Interactive
    // sessions and the menu proof always prepare a manual hand launch.
    bHeldForLaunch = !bSmokeMode || bMenuProof;
    ThrowSeconds = -1.f;
    Elevator = Rudder = 0.f;
    if (bHeldForLaunch)
    {
        FlightView = EFlightView::Ground;
        bInspectMeadow = false;
        Flight.StopAt(HandPose());
    }
    // Relaunching the aircraft must not restart the life cycle of the air mass.
    RidgeLift = ValleyAir::SampleGlider(Flight, WeatherSeconds);
    WindTrails.Clear(*AirLines);
    ++FlightId;
    TimeBank=0.;
    PreviousPose=Flight.WorldPose();
    TargetPosition=PreviousPose.GetLocation();
    SetActorTransform(PreviousPose);
    ContactKind=NAME_None;
    AirspeedMps=Flight.Airspeed();
    AltitudeM=Flight.Altitude();
    SinkMps=Flight.SinkRate();
}

FTransform ASlopeFlightPawn::HandPose(float Progress) const
{
    if (UsesDiscusLaunch())
    {
        // The left wing peg stays in the right hand throughout a rising arc.
        // The tangent at the end points along the heading selected while walking.
        const double T = FMath::Clamp(double(Progress),0.,1.);
        const double Angle = UE_HALF_PI + DiscusArc*T*T;
        const double Blend = T*T*(3.-2.*T);
        const FVector Forward = FRotator(55.*Blend,LaunchHeading,0.).Vector();
        const FVector Right = FRotator(0.,LaunchHeading+90.,0.).Vector();
        const FVector Radial = Forward*FMath::Sin(Angle) + Right*FMath::Cos(Angle);
        const FVector Tangent = Forward*FMath::Cos(Angle) - Right*FMath::Sin(Angle);
        // Step through the turn so the glider crosses the pilot's sightline
        // at release. The step eases to rest, preserving the release tangent.
        const FVector Ahead = FRotator(0.,LaunchHeading,0.).Vector();
        const FVector Step = (Ahead-Right)*(DiscusRadiusCm*Blend);
        return FTransform(Tangent.Rotation(),PilotPosition()+FVector(0,0,145.)+DiscusRadiusCm*Radial+Step);
    }
    // Right-handed, shoulder-height throw along the selected site's heading.
    // A quadratic stroke reaches 8 m/s at release after 0.30 s.
    const double Stroke=120.*FMath::Square(FMath::Clamp(Progress,0.f,1.f));
    const FVector Position=PilotPosition()+FRotator(0,LaunchHeading,0).RotateVector(FVector(45.+Stroke,45.,145.+Stroke*.1));
    return FTransform(FRotator(6.f,LaunchHeading,0.f),Position);
}

void ASlopeFlightPawn::BeginHandLaunch()
{
    if (!bModelReady || !bAppearanceReady || bMenuOpen || !bHeldForLaunch || IsThrowing()) return;
    bInspectMeadow = false;
    ThrowSeconds = 0.f;
}

void ASlopeFlightPawn::AdvanceFlight(float DeltaSeconds)
{
    if (!bModelReady || bMenuOpen || bNoclip) return;
    const double Aileron = SelectedModel >= 2 ? Radio.SteeringCommand(!AileronSteering[SelectedModel]) : 0.;
    if (bHeldForLaunch)
    {
        const float Dt=FMath::Clamp(DeltaSeconds,0.f,.05f);
        Flight.AdvanceFlaps(Radio.Command(EGliderControl::Flaps),Dt,IsControlInUse(EGliderControl::Butterfly) ? Radio.Command(EGliderControl::Butterfly) : 0.);
        Flight.AdvanceSpoilers(Radio.Command(EGliderControl::Spoilers),Dt);
        Flight.AdvanceServos(Elevator,-Rudder,Aileron,Dt);
        WeatherSeconds+=Dt;
        if (IsThrowing()) ThrowSeconds+=Dt;
        const float Duration = UsesDiscusLaunch() ? DiscusDuration : .30f;
        const FTransform Pose=HandPose(IsThrowing() ? ThrowSeconds/Duration : 0.f);
        Flight.StopAt(Pose);
        RidgeLift=ValleyAir::SampleGlider(Flight,WeatherSeconds);
        PreviousPose=Pose;
        TargetPosition=Pose.GetLocation();
        SetActorTransform(Pose);
        TimeBank=0.;
        AirspeedMps=0.f; SinkMps=0.f; AltitudeM=Flight.Altitude();
        if (ThrowSeconds>=Duration)
        {
            FVector Velocity = FRotator(0,LaunchHeading,0).RotateVector(FVector((SelectedModel == 2 || SelectedModel == 7) ? 12. : 8.,0.,.8));
            const double ReleaseAirspeed = Aircraft[SelectedModel].AssistedReleaseMps;
            if (ReleaseAirspeed > 0.)
            {
                const FVector Wind = Flight.LocalWind(); // NED to Unreal world axes.
                Velocity = FRotator(0,LaunchHeading,0).Vector()*ReleaseAirspeed + FVector(Wind.X,Wind.Y,-Wind.Z);
            }
            if (UsesDiscusLaunch())
                Velocity = Pose.GetRotation().GetForwardVector()*(.01*DiscusRadiusCm*2.*DiscusArc/DiscusDuration);
            Flight.LaunchFrom(Pose,Velocity);
            bHeldForLaunch=false;
            ThrowSeconds=-1.f;
            AirspeedMps=Flight.Airspeed(); SinkMps=Flight.SinkRate();
            UE_LOG(LogTemp,Display,TEXT("LAUNCH released: %s, ground speed %.2f m/s"),ModelName(),Velocity.Size());
        }
        return;
    }
    // Bound catch-up after an OS stall. Sweep every simulated step, even when
    // the renderer needs several steps per frame.
    TimeBank+=FMath::Clamp(double(DeltaSeconds),0.,.25);
    while (TimeBank>=FGliderDynamics::TimeStep)
    {
        WeatherSeconds += FGliderDynamics::TimeStep;
        TimeBank-=FGliderDynamics::TimeStep;
        if (Flight.IsStopped()) continue;
        PreviousPose=Flight.WorldPose();
        Flight.AdvanceFlaps(Radio.Command(EGliderControl::Flaps),FGliderDynamics::TimeStep,IsControlInUse(EGliderControl::Butterfly) ? Radio.Command(EGliderControl::Butterfly) : 0.);
        Flight.AdvanceSpoilers(Radio.Command(EGliderControl::Spoilers),FGliderDynamics::TimeStep);
        const double Throttle = Radio.Command(EGliderControl::MotorCut) > .5f ? 0. : Radio.Command(EGliderControl::Motor);
        Flight.Step(Elevator,-Rudder,Aileron,Throttle);
        RidgeLift = ValleyAir::SampleGlider(Flight, WeatherSeconds);
        ++StepsThisSecond;
        FTransform Contact;
        const FHangarEntry& Entry = Aircraft[SelectedModel];
        const FVector2D SizeScale((Entry.BodyLengthCm > 0. ? Entry.BodyLengthCm : Entry.LengthCm)/Aircraft[0].LengthCm, Entry.SpanCm/Aircraft[0].SpanCm);
        if (SceneContact::SweepTopy(GetWorld(),this,PreviousPose,Flight.WorldPose(),Contact,ContactKind,
            FuselageCentre,SizeScale,Entry.WingRisePerSpan,Entry.Wheel,Entry.TipSweepCm))
        {
            Flight.StopAt(Contact);
            PreviousPose=Contact;
            TimeBank=0.;
            UE_LOG(LogTemp,Display,TEXT("NATIVE_FLIGHT %d hit %s"),FlightId,*ContactKind.ToString());
            break;
        }
    }
    const FTransform Current=Flight.WorldPose();
    if (Flight.IsStopped()) Flight.SetWind(ValleyAir::Velocity(Flight.PositionNED(), WeatherSeconds, &RidgeLift));
    TargetPosition=Current.GetLocation();
    AirspeedMps=Flight.Airspeed(); SinkMps=Flight.SinkRate(); AltitudeM=Flight.Altitude();
    FTransform Display;
    Display.Blend(PreviousPose,Current,TimeBank/FGliderDynamics::TimeStep);
    SetActorTransform(Display);
}

void ASlopeFlightPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bAppearanceReady)
    {
        bAppearanceReady = !AircraftTextures.ContainsByPredicate([](UTexture2D* Texture)
        {
            return Texture->HasPendingInitOrStreaming() || !Texture->IsFullyStreamedIn();
        });
        if (bAppearanceReady)
        {
            ImportedTopy->SetVisibility(true);
            UE_LOG(LogTemp,Display,TEXT("AIRCRAFT_TEXTURES ready: %s, %d textures"),ModelName(),AircraftTextures.Num());
        }
    }
    Radio.Poll(Cast<APlayerController>(GetController()),bMenuOpen);
    if (!Radio.EditingKeyboard() && !Radio.HasDevice() && (RadioScanTime += DeltaSeconds) >= 3.f)
    {
        RadioScanTime = 0.f;
        Radio.Scan();
    }
    if (bMenuOpen)
    {
        TourTime += DeltaSeconds;
        if (!Menu.IsValid()) OpenMenu();
        else Menu->UpdateStartupPoster();
    }
    else if (!bContactProof) ReadControls(DeltaSeconds);
    AdvanceFlight(DeltaSeconds);
    UpdateFlightAudio(DeltaSeconds);
    if (bModelReady && !bMenuOpen && !bNoclip && bShowWindTrails)
    {
        FVector Eye; FRotator View;
        GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(Eye,View);
        Eye*=.01;
        WindTrails.Update(*AirLines, Flight.PositionNED(), -Flight.RelativeFlowNED().GetSafeNormal(), WeatherSeconds,FVector(Eye.X,Eye.Y,-Eye.Z),TrailDistance);
    }
    else
        WindTrails.Clear(*AirLines);
    if (bHangarModel) PositionHangarModel();
    StatsAccumulator += DeltaSeconds;
    if (StatsAccumulator >= 1.0f)
    {
        PhysicsHz = StepsThisSecond / StatsAccumulator;
        StatsAccumulator = 0.0f;
        StepsThisSecond = 0;
    }
    if (bSmokeMode || bCaptureSite)
    {
        SmokeElapsed += DeltaSeconds;
        if (bMenuProof) { TestMenu(SmokeElapsed); return; }
        if (bContactProof && Flight.IsStopped() && ContactKind==TEXT("tree") && ProofFlight==0)
        {
            ProofFlight=FlightId;
            if (!SceneContact::CheckSite(GetWorld())) FPlatformMisc::RequestExitWithStatus(false,2);
            UE_LOG(LogTemp,Display,TEXT("CONTACT_PROOF wing contact acknowledged, speed %.1f"),AirspeedMps);
            ResetFlight();
        }
        if (bContactProof && ProofFlight>0 && FlightId>ProofFlight && ContactKind.IsNone() && AirspeedMps>1.f)
        {
            UE_LOG(LogTemp,Display,TEXT("CONTACT_PROOF reset acknowledged, airborne flight %d"),FlightId);
            bContactProof=false;
        }
        if (!bSmokeScreenshotRequested && SmokeElapsed >= (bCaptureSite ? 9.f : 25.f))
        {
            bSmokeScreenshotRequested = true;
            const FString ScreenshotPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), bCaptureSite ? TEXT("alpine-flight.png") : TEXT("simplaneur-unreal-smoke.png"));
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false);
            UE_LOG(LogTemp, Display, TEXT("SimPlaneur: smoke screenshot requested at %s"), *ScreenshotPath);
        }
        if (bMeadowProof && SmokeElapsed >= 12.f)
        {
            bInspectMeadow = true;
            // Sample a two-second sidestep through the normal ground camera.
            if (MeadowFrame < 25 && SmokeElapsed >= 14.f+MeadowFrame*.12f && !FScreenshotRequest::IsScreenshotRequested())
            {
                const FString Name = FString::Printf(TEXT("meadow-%02d.png"),MeadowFrame);
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots"),Name),true,false,false);
                ++MeadowFrame;
            }
            if (SmokeElapsed >= 14.f && SmokeElapsed < 16.9f) StepPilot(DeltaSeconds*38.f);
        }
        if (bSmokeMode && bCaptureSite && !bMeadowProof && !bChaseCaptured && SmokeElapsed >= 12.f)
        {
            if (FlightView == EFlightView::Ground) CycleCamera();
            if (SmokeElapsed >= 16.f)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/valley-chase.png")),true,false,false);
                bChaseCaptured = true;
            }
        }
        if (bSmokeMode && SmokeElapsed >= 29.0f)
        {
            FPlatformMisc::RequestExitWithStatus(false,bContactProof ? 2 : 0);
        }
    }
}

FString ASlopeFlightPawn::GetContactLabel() const
{
    if (ContactKind.IsNone()) return {};
    if (ContactKind==TEXT("tree")) return TEXT("Arbre touché");
    if (ContactKind==TEXT("bench")) return TEXT("Contact banc");
    if (ContactKind==TEXT("mast")) return TEXT("Contact mât");
    if (ContactKind==TEXT("rock")) return TEXT("Contact rocher");
    if (ContactKind==TEXT("water")) return TEXT("Contact eau");
    return TEXT("Au sol");
}

void ASlopeFlightPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ImportedTopy->SetTextureForceResidentFlag(false);
    if (Menu.IsValid() && GEngine && GEngine->GameViewport)
    {
        Menu->CancelDisplayChange();
        GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
        Menu.Reset();
    }
    Super::EndPlay(EndPlayReason);
}
