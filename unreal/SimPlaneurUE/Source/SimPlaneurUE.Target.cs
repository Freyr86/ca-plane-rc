using UnrealBuildTool;
using System.Collections.Generic;

public class SimPlaneurUETarget : TargetRules
{
    public SimPlaneurUETarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("SimPlaneurUE");
    }
}
