#include "SlopeFlightPawn.h"
#include "ValleyAir.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"

namespace
{
    constexpr float AudioInterval = .08f;
    constexpr auto WindAsset = TEXT("/Game/FlightAudio/SiteWind.SiteWind");
    constexpr auto WingAsset = TEXT("/Game/FlightAudio/WingRush.WingRush");
    constexpr auto MotorAsset = TEXT("/Game/FlightAudio/ElectricProp.ElectricProp");
    constexpr auto ServoAsset = TEXT("/Game/FlightAudio/ServoGear.ServoGear");

    struct FVoice { float Gain = 0.f, Pitch = 1.f; };

    // Pressure falls approximately as 1/r beyond the reference distance.
    float DistanceGain(float Metres, float Reference)
    {
        return Reference / FMath::Sqrt(Reference*Reference + Metres*Metres);
    }
    FVoice WindVoice(float Speed)
    {
        const float Flow = FMath::Clamp(Speed / 14.f,0.f,1.f);
        return {.56f*Flow,.72f+.52f*Flow};
    }
    FVoice WingVoice(float Speed, float Span, float Distance, float Doppler)
    {
        const float Rush = FMath::Clamp((Speed-4.f)/24.f,0.f,1.f);
        return {.95f*Rush*DistanceGain(Distance,4.f+1.5f*Span),
            (.88f+.012f*FMath::Min(Speed,48.f))*Doppler};
    }
    FVoice MotorVoice(float Watts, float MaxWatts, float Distance, float Doppler)
    {
        const float Revs = FMath::Sqrt(FMath::Clamp(Watts/FMath::Max(1.f,MaxWatts),0.f,1.f));
        const float Size = FMath::Sqrt(FMath::Max(Watts,0.f));
        return {.80f*Revs*DistanceGain(Distance,4.f+.45f*Size),(.46f+1.25f*Revs)*Doppler};
    }
    FVoice ServoVoice(float TravelPerSecond,float Distance)
    {
        const float Motion = FMath::Clamp((TravelPerSecond-.13f)/2.5f,0.f,1.f);
        const float Near = DistanceGain(Distance,2.5f);
        return {.22f*FMath::Sqrt(Motion)*Near*Near,.82f+.24f*Motion};
    }
    void MixVoice(UAudioComponent* Source, FVoice Voice, float Fade=AudioInterval)
    {
        if (!Source) return;
        const auto State = Source->GetPlayState();
        if (Voice.Gain < .004f)
        {
            if (State == EAudioComponentPlayState::Playing || State == EAudioComponentPlayState::FadingIn)
                Source->FadeOut(Fade,0.f);
            return;
        }
        Source->SetPitchMultiplier(Voice.Pitch);
        if (State == EAudioComponentPlayState::Stopped)
            Source->FadeIn(Fade,Voice.Gain);
        else if (State != EAudioComponentPlayState::FadingOut)
            Source->AdjustVolume(Fade,Voice.Gain);
    }
}

void ASlopeFlightPawn::PrepareFlightAudio()
{
    if (!GetWorld()->AllowAudioPlayback()) return;
    auto MakeSource = [this](const TCHAR* Path, bool Spatial)
    {
        USoundWave* Loop = LoadObject<USoundWave>(nullptr,Path);
        if (!Loop) return static_cast<UAudioComponent*>(nullptr);
        auto* Source = NewObject<UAudioComponent>(this);
        Source->bAutoActivate = false;
        Source->bStopWhenOwnerDestroyed = true;
        Source->SetupAttachment(SceneRoot);
        Source->bAllowSpatialization = Spatial;
        Source->bOverrideAttenuation = true;
        Source->AttenuationOverrides.bSpatialize = Spatial;
        Source->AttenuationOverrides.bAttenuate = false; // DistanceGain handles level per source.
        Source->SetSound(Loop);
        Source->RegisterComponent();
        return Source;
    };
    WindAudio = MakeSource(WindAsset,false);
    WingAudio = MakeSource(WingAsset,true);
    MotorAudio = MakeSource(MotorAsset,true);
    ServoAudio = MakeSource(ServoAsset,true);
}

void ASlopeFlightPawn::UpdateFlightAudio(float DeltaSeconds)
{
    AudioElapsed += DeltaSeconds;
    if (AudioElapsed < AudioInterval) return;
    const float ServoSpeed = (Flight.TotalServoTravel()-LastServoTravel)/AudioElapsed;
    LastServoTravel = Flight.TotalServoTravel();
    AudioElapsed = 0.f;
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC || (!WindAudio && !WingAudio && !MotorAudio && !ServoAudio)) return;
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye,View);
    const bool Flying = bModelReady && !bHeldForLaunch && !Flight.IsStopped() && !bMenuOpen && !bNoclip;
    const bool Following = Flying && FlightView != EFlightView::Ground;
    const FVector EarNED(Eye.X*.01,Eye.Y*.01,-Eye.Z*.01);
    const float WindSpeed = Following ? Flight.Airspeed() :
        ValleyAir::Velocity(EarNED,bMenuOpen ? TourTime : WeatherSeconds).Size();
    MixVoice(WindAudio,WindVoice(WindSpeed));
    const FVector ToEar = EarNED-Flight.PositionNED();
    const float Distance = ToEar.Size();
    const bool CanMoveServos = bModelReady && !bMenuOpen && !bNoclip && (bHeldForLaunch || Flying);
    MixVoice(ServoAudio,CanMoveServos ? ServoVoice(ServoSpeed,Distance) : FVoice{},.025f);
    FVoice Wings, Motor;
    if (Flying)
    {
        const FVector Velocity = Flight.LocalWind()-Flight.RelativeFlowNED();
        const float Closing = Following ? 0.f : FVector::DotProduct(Velocity,ToEar.GetSafeNormal());
        const float Doppler = 343.f/(343.f-FMath::Clamp(Closing,-70.f,70.f));
        const float Span = 2.f*Flight.RightTipOffsetNED().Size();
        Wings = WingVoice(Flight.Airspeed(),Span,Distance,Doppler);
        Motor = MotorVoice(Flight.MotorPowerWatts(),Flight.MotorSettings().Watts,Distance,Doppler);
    }
    MixVoice(WingAudio,Wings);
    MixVoice(MotorAudio,Motor);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightAudioTest,"SimPlaneur.Audio.FlightMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFlightAudioTest::RunTest(const FString&)
{
    for (const TCHAR* Path : {WindAsset,WingAsset,MotorAsset,ServoAsset})
    {
        const auto* Loop = LoadObject<USoundWave>(nullptr,Path);
        TestTrue(Path,Loop && Loop->bLooping && Loop->NumChannels==1 && Loop->Duration>=1.9f);
    }
    TestTrue(TEXT("Wind rises from calm to gust"),WindVoice(0).Gain==0 && WindVoice(4).Gain<WindVoice(10).Gain);
    const FVoice Slow=WingVoice(8,3,8,1), Fast=WingVoice(24,3,8,1);
    TestTrue(TEXT("Fast pass is louder and higher"),Slow.Gain<Fast.Gain && Slow.Pitch<Fast.Pitch);
    TestTrue(TEXT("Wings recede"),WingVoice(24,3,100,1).Gain<.2f*Fast.Gain);
    TestTrue(TEXT("Motor cut silences prop"),MotorVoice(0,400,8,1).Gain==0);
    const FVoice Half=MotorVoice(200,400,8,1), Full=MotorVoice(400,400,8,1);
    TestTrue(TEXT("RPM follows throttle"),Half.Gain<Full.Gain && Half.Pitch<Full.Pitch);
    TestTrue(TEXT("Motor recedes"),MotorVoice(400,400,120,1).Gain<.2f*Full.Gain);
    FGliderDynamics Glider;
    if (!TestTrue(TEXT("Load Topy servos"),Glider.LoadModel(Glider.TopyModelPath()))) return false;
    Glider.AdvanceServos(1.,0.,0.,1.);
    const double Stroke = Glider.TotalServoTravel();
    Glider.AdvanceServos(1.,0.,0.,1.);
    TestTrue(TEXT("A moving servo sounds at the pilot"),ServoVoice(Stroke,1).Gain>.1f);
    TestEqual(TEXT("Held deflection adds no servo travel"),Glider.TotalServoTravel(),Stroke);
    TestTrue(TEXT("Radio jitter is quiet"),ServoVoice(.08f,1).Gain==0);
    TestTrue(TEXT("Servo fades beyond the launch area"),ServoVoice(3,50).Gain<.004f);
    return true;
}
#endif
