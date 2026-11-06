#include "SlopeFlightPawn.h"
#include "SlopeWorld.h"
#include "WeatherScene.h"
#include "Components/AudioComponent.h"
#include "ValleyAir.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "FlightMenu.h"
#include "Widgets/Input/SButton.h"

void ASlopeFlightPawn::TestWeather(float Elapsed)
{
#if !UE_BUILD_SHIPPING
    const bool StormOnly=FParse::Param(FCommandLine::Get(),TEXT("WeatherStormProof"));
    if(StormOnly && MenuProofStep<10) MenuProofStep=10;
    if(StormOnly) Elapsed+=70.f;
    auto Shot=[](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false,false); };
    AWeatherScene* Scene=nullptr;
    for(TActorIterator<AWeatherScene> It(GetWorld());It;++It) { Scene=*It; break; }
    if(Scene && Scene->GetLightningSerial()>WeatherProofLightning)
    {
        WeatherProofLightning=Scene->GetLightningSerial();
    }
    if(Scene && WeatherProofLightning>WeatherProofCapturedLightning && Scene->GetLightningAge()>=.14 && Scene->IsLightningVisible())
    {
        Shot(TEXT("weather-06-lightning.png"));
        WeatherProofCapturedLightning=WeatherProofLightning;
        UE_LOG(LogTemp,Display,TEXT("WEATHER_PROOF flash_captured age=%.3f visible=1"),Scene->GetLightningAge());
    }
    if(!bAppearanceReady || Elapsed<5.f+MenuProofStep*7.f) return;
    if(StormOnly && MenuProofStep>12)
    {
        UE_LOG(LogTemp,Display,TEXT("WEATHER_PROOF complete storm_only=1"));
        FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
    auto View=[this](FVector Eye,FVector Target)
    {
        bMenuOpen=false; bNoclip=true; bHeldForLaunch=true; bInBackpack=true;
        bShowWindTrails=false; PlaneVisual->SetHiddenInGame(true,true);
        NoclipPosition=Eye; NoclipRotation=(Target-Eye).Rotation();
        if(auto* PC=Cast<APlayerController>(GetController())) if(auto* HUD=PC->GetHUD()) HUD->bShowHUD=false;
    };
    auto W=ValleyAir::Settings();
    bool OK=Scene && Scene->GetFogPoolSize()==27;
    switch(MenuProofStep)
    {
    case 0:
        W=ValleyAir::FWeatherSettings{};
        W.WindSpeedMps=7.; W.WindFromDegrees=70.;
        View({-3500,-9000,13000},{22000,18000,17000});
        break;
    case 1: Shot(TEXT("weather-01-cumulus.png")); break;
    case 2:
        W.Sky=4; W.CloudBaseMeters=300; W.CloudCoverage=.82;
        View({240000,120000,17000},{298000,216000,30500}); break;
    case 3: Shot(TEXT("weather-02-stratus.png")); break;
    case 4:
        W.Sky=5; W.CloudBaseMeters=520; W.CloudCoverage=.8; W.RainStrength=1.;
        View({-75000,-48000,23000},{15000,8000,15000}); break;
    case 5: Shot(TEXT("weather-03-distant-rain.png")); break;
    case 6:
    {
        const auto C=ValleyAir::WeatherCells(GetWeatherVisualSeconds(),W)[0].Centre;
        const FVector Ground(C.X*100,C.Y*100,ASlopeWorld::TerrainHeightCm(C.X*100,C.Y*100));
        View(Ground+FVector(0,0,2600),Ground+FVector(16000,14000,4000)); break;
    }
    case 7:
        OK &= Scene && Scene->GetLocalRain()>.7 && WeatherRainMix>.5f;
        Shot(TEXT("weather-04-inside-rain.png")); break;
    case 8:
        View({-180000,60000,40000},{20000,8000,26000}); break;
    case 9:
        OK &= Scene && Scene->GetLocalRain()<.02 && WeatherRainMix<.05f;
        Shot(TEXT("weather-05-dry-zone.png")); break;
    case 10:
        W.Sky=6; W.CloudBaseMeters=420; W.CloudCoverage=.96;
        View({-20000,-16000,15500},{15000,8000,25000}); break;
    case 11: break;
    case 12:
        OK &= WeatherProofLightning>0 && WeatherProofCapturedLightning>0;
        OK &= ThunderAudio && ThunderAudio->IsPlaying();
        UE_LOG(LogTemp,Display,TEXT("WEATHER_PROOF thunder_playing=%d"),ThunderAudio && ThunderAudio->IsPlaying());
        Shot(TEXT("weather-07-storm.png")); break;
    case 13:
        W.Sky=0; W.WindSpeedMps=0;
        View({6400,-1800,ASlopeWorld::TerrainHeightCm(6400,-1800)+190.},{8000,0,ASlopeWorld::TerrainHeightCm(6400,-1800)+100.}); break;
    case 14:
        OK &= Scene && Scene->GetLocalRain()==0;
        Shot(TEXT("weather-08-calm.png")); break;
    case 15: W.WindSpeedMps=10; W.Turbulence=.6; break;
    case 16: Shot(TEXT("weather-09-windy-grass.png")); break;
    case 17:
    {
        OpenMenu();
        TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> FindWeather;
        FindWeather=[&](TSharedRef<SWidget> Widget)->TSharedPtr<SWidget>
        {
            if(Widget->GetTag()==TEXT("Weather")) return Widget;
            auto* Children=Widget->GetChildren();
            for(int32 I=0;Children && I<Children->Num();++I)
                if(auto Found=FindWeather(Children->GetChildAt(I))) return Found;
            return nullptr;
        };
        auto Button=Menu.IsValid()?FindWeather(Menu.ToSharedRef()):nullptr;
        OK &= Button.IsValid() && Button->GetType()==TEXT("SButton");
        if(OK) StaticCastSharedPtr<SButton>(Button)->SimulateClick();
        break;
    }
    case 18:
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/weather-10-menu.png"),true,false,false); break;
    default:
        UE_LOG(LogTemp,Display,TEXT("WEATHER_PROOF complete"));
        FPlatformMisc::RequestExitWithStatus(false,0); return;
    }
    ValleyAir::SetSettings(W);
    UE_LOG(LogTemp,Display,TEXT("WEATHER_PROOF step=%d pass=%d local_rain=%.3f rain_mix=%.3f lightning=%d"),MenuProofStep,OK,Scene?Scene->GetLocalRain():-1,WeatherRainMix,WeatherProofLightning);
    if(!OK) FPlatformMisc::RequestExitWithStatus(false,3);
    ++MenuProofStep;
#endif
}
