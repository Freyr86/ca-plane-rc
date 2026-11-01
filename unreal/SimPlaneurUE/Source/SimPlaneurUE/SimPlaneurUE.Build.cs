using UnrealBuildTool;

public class SimPlaneurUE : ModuleRules
{
    public SimPlaneurUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "Json",
            "ProceduralMeshComponent", "Slate", "SlateCore"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.AddRange(new[] { "dinput8.lib", "dxguid.lib" });
        }
    }
}
