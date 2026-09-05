using UnrealBuildTool;
using System.Collections.Generic;

public class EclipseFrontEditorTarget : TargetRules
{
    public EclipseFrontEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("EclipseFront");
    }
}
