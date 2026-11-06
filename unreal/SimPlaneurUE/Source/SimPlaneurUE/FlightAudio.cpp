#include "SlopeFlightPawn.h"
#include "ValleyAir.h"
#include "ValleyLandscape.h"
#include "WeatherScene.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    constexpr float AudioInterval = .08f;
    constexpr auto WindAsset = TEXT("/Game/FlightAudio/SiteWind.SiteWind");
    constexpr auto WingAsset = TEXT("/Game/FlightAudio/WingRush.WingRush");
    constexpr auto MotorAsset = TEXT("/Game/FlightAudio/ElectricProp.ElectricProp");
    constexpr auto ServoAsset = TEXT("/Game/FlightAudio/ServoGear.ServoGear");
    constexpr auto VarioAsset = TEXT("/Game/FlightAudio/VarioTone.VarioTone");
    constexpr auto RainAsset = TEXT("/Game/FlightAudio/RainShower.RainShower");
    constexpr auto FoliageAsset = TEXT("/Game/FlightAudio/FoliageRustle.FoliageRustle");
    constexpr auto ThunderAsset = TEXT("/Game/FlightAudio/ThunderRoll.ThunderRoll");
    constexpr auto VarioSection = TEXT("SimPlaneur.Vario");

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
    FVoice VarioVoice(float Climb, float Upper, float Lower, float Dt, float& Phase)
    {
        if (Climb < Upper)
        {
            Phase = 0.f;
            if (Climb <= Lower)
                return {.14f,FMath::Max(.42f,.68f-.035f*(Lower-Climb))};
            return {};
        }
        const float Lift = FMath::Clamp((Climb-Upper)/5.f,0.f,1.f);
        const float Period = FMath::Lerp(.92f,.32f,Lift);
        const bool Beep = Phase < .48f;
        Phase = FMath::Fmod(Phase+Dt/Period,1.f);
        return {Beep ? .17f : 0.f,1.f+.60f*Lift};
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

void ASlopeFlightPawn::SetVario(bool Enabled, float Climb, float Sink)
{
    bVarioEnabled = Enabled;
    ClimbThreshold = FMath::Clamp(Climb,.1f,10.f);
    SinkThreshold = FMath::Clamp(Sink,-10.f,-.1f);
    GConfig->SetBool(VarioSection,TEXT("Enabled"),Enabled,GGameUserSettingsIni);
    GConfig->SetFloat(VarioSection,TEXT("ClimbMps"),ClimbThreshold,GGameUserSettingsIni);
    GConfig->SetFloat(VarioSection,TEXT("SinkMps"),SinkThreshold,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

void ASlopeFlightPawn::PrepareFlightAudio()
{
    GConfig->GetBool(VarioSection,TEXT("Enabled"),bVarioEnabled,GGameUserSettingsIni);
    GConfig->GetFloat(VarioSection,TEXT("ClimbMps"),ClimbThreshold,GGameUserSettingsIni);
    GConfig->GetFloat(VarioSection,TEXT("SinkMps"),SinkThreshold,GGameUserSettingsIni);
    ClimbThreshold = FMath::Clamp(ClimbThreshold,.1f,10.f);
    SinkThreshold = FMath::Clamp(SinkThreshold,-10.f,-.1f);
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
    VarioAudio = MakeSource(VarioAsset,false);
    RainAudio = MakeSource(RainAsset,false);
    FoliageAudio = MakeSource(FoliageAsset,false);
    ThunderAudio = MakeSource(ThunderAsset,true);
    if(ThunderAudio) ThunderAudio->SetAbsolute(true,false,false);
}

void ASlopeFlightPawn::UpdateFlightAudio(float DeltaSeconds)
{
    AudioElapsed += DeltaSeconds;
    if (AudioElapsed < AudioInterval) return;
    const float Dt = AudioElapsed;
    const float ServoSpeed = (Flight.TotalServoTravel()-LastServoTravel)/Dt;
    LastServoTravel = Flight.TotalServoTravel();
    AudioElapsed = 0.f;
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye,View);
    const bool Flying = bModelReady && !bHeldForLaunch && !Flight.IsStopped() && !bMenuOpen && !bNoclip;
    FVoice Vario;
    if (Flying && bVarioEnabled)
    {
        // The onboard instrument reads the aircraft's vertical speed over ground.
        FilteredClimb = FMath::Lerp(FilteredClimb,float(-Flight.SinkRate()),1.f-FMath::Exp(-Dt/.35f));
        Vario = VarioVoice(FilteredClimb,ClimbThreshold,SinkThreshold,Dt,VarioPhase);
    }
    else
    {
        FilteredClimb = 0.f;
        VarioPhase = 0.f;
    }
    MixVoice(VarioAudio,Vario,.012f);
    const bool Following = Flying && FlightView != EFlightView::Ground;
    const FVector EarNED(Eye.X*.01,Eye.Y*.01,-Eye.Z*.01);
    const float WindSpeed = Following ? Flight.Airspeed() :
        ValleyAir::Velocity(EarNED,bMenuOpen ? TourTime : WeatherSeconds).Size();
    MixVoice(WindAudio,WindVoice(WindSpeed));
    const auto Weather=ValleyAir::Settings();
    const double WeatherTime=GetWeatherVisualSeconds();
    const float Wet=float(ValleyAir::RainAt(EarNED,WeatherTime,Weather));
    const float Height=float(FMath::Max(0.,-EarNED.Z-ValleyAir::SurfaceAltitude(EarNED.X,EarNED.Y)));
    const float Cover=Valley::Woodland(Eye.X,Eye.Y);
    const float Rustle=FMath::Clamp(WindSpeed/10.f,0.f,1.f)*FMath::Exp(-Height/22.f)*(.22f+.78f*Cover);
    WeatherRainMix=FMath::FInterpTo(WeatherRainMix,Wet,Dt,2.f);
    WeatherFoliageMix=FMath::FInterpTo(WeatherFoliageMix,Rustle,Dt,1.5f);
    MixVoice(RainAudio,{WeatherRainMix*.48f,.9f+.16f*WeatherRainMix},.3f);
    MixVoice(FoliageAudio,{WeatherFoliageMix*.24f,.82f+.22f*Rustle},.3f);
    // A strike is seen immediately; sound arrives at the camera after d / 343.
    // Bounded queue also handles a new strike while an earlier roll is travelling.
    for(TActorIterator<AWeatherScene> Scene(GetWorld());Scene;++Scene)
    {
        const int32 Serial=Scene->GetLightningSerial();
        if(Serial!=HeardLightningSerial)
        {
            HeardLightningSerial=Serial;
            const FVector Strike=Scene->GetLightningPosition();
            const double Metres=FVector::Dist(Strike,Eye)*.01;
            if(Serial>0 && Weather.Sky==6 && Metres<7000. && PendingThunder.Num()<4)
                PendingThunder.Add({GetWorld()->GetTimeSeconds()+Metres/343.,Strike,float(.65/(1.+Metres/1800.))});
        }
        break;
    }
    if(Weather.Sky!=6) PendingThunder.Reset();
    for(int32 I=PendingThunder.Num()-1;I>=0;--I)
    {
        if(PendingThunder[I].Due>GetWorld()->GetTimeSeconds()) continue;
        if(ThunderAudio)
        {
            ThunderAudio->SetWorldLocation(PendingThunder[I].Position);
            ThunderAudio->SetVolumeMultiplier(PendingThunder[I].Gain);
            ThunderAudio->Play();
        }
        PendingThunder.RemoveAt(I);
    }
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
    for (const TCHAR* Path : {WindAsset,WingAsset,MotorAsset,ServoAsset,VarioAsset,RainAsset,FoliageAsset})
    {
        const auto* Loop = LoadObject<USoundWave>(nullptr,Path);
        TestTrue(Path,Loop && Loop->bLooping && Loop->NumChannels==1 && Loop->Duration>=1.9f);
    }
    const auto* Thunder=LoadObject<USoundWave>(nullptr,ThunderAsset);
    TestTrue(TEXT("Thunder is a finite one-shot"),Thunder && !Thunder->bLooping && Thunder->Duration>=5.f);
    TestTrue(TEXT("Wind rises from calm to gust"),WindVoice(0).Gain==0 && WindVoice(4).Gain<WindVoice(10).Gain);
    float Phase = 0.f;
    const FVoice Climb = VarioVoice(.3f,.3f,-2.f,.5f,Phase);
    TestTrue(TEXT("Climb threshold starts a beep"),Climb.Gain>0.f);
    TestTrue(TEXT("Climb beeps have silent gaps"),VarioVoice(.3f,.3f,-2.f,.1f,Phase).Gain==0.f);
    TestTrue(TEXT("Raised climb threshold gives silence"),VarioVoice(.5f,.7f,-2.f,.1f,Phase).Gain==0.f);
    TestTrue(TEXT("Ordinary sink is silent"),VarioVoice(-1.f,.3f,-2.f,.1f,Phase).Gain==0.f);
    const FVoice Sink = VarioVoice(-2.f,.3f,-2.f,.1f,Phase);
    TestTrue(TEXT("Sink threshold emits a lower continuous tone"),Sink.Gain>0.f && Sink.Pitch<Climb.Pitch
        && VarioVoice(-2.f,.3f,-2.f,1.f,Phase).Gain==Sink.Gain);
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
