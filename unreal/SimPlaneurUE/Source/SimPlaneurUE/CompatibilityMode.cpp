#include "CompatibilityMode.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Scalability.h"
#include "HAL/IConsoleManager.h"

bool CompatibilityMode::IsEnabled()
{
    return FParse::Param(FCommandLine::Get(),TEXT("CaPlaneLowSpec"));
}

void CompatibilityMode::Apply()
{
    if(!IsEnabled()) return;
    // Apply before spawning the scene. A successful DirectX device probe does
    // not establish stability under sustained load on older graphics cards.
    Scalability::FQualityLevels Low; Low.SetFromSingleQualityLevel(0);
    // Unreal saves the backed-up quality when temporary levels are active,
    // including when the updater saves settings before restarting the game.
    Scalability::ToggleTemporaryQualityLevels(true,Low);
    Scalability::SetQualityLevels(Low,true);
    auto Set=[](const TCHAR* Name,int32 Value)
    {
        if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(Value,ECVF_SetByCode);
    };
    Set(TEXT("t.MaxFPS"),30);
    Set(TEXT("r.VSync"),1);
    Set(TEXT("r.VolumetricCloud"),0);
    // Bound streamed textures only; render targets, geometry and driver memory
    // still consume VRAM. This is not a cap on the process's total GPU memory.
    Set(TEXT("r.Streaming.PoolSize"),400);
    Set(TEXT("r.Streaming.LimitPoolSizeToVRAM"),1);
    UE_LOG(LogTemp,Display,TEXT("COMPATIBILITY_MODE low_graphics=1 max_fps=30 vsync=1 texture_pool_mb=400 settings_preserved=1"));
}
