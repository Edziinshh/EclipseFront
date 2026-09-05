using UnrealBuildTool;

public class EclipseFront : ModuleRules
{
    public EclipseFront(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AIModule",
            "NavigationSystem",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });
    }
}
