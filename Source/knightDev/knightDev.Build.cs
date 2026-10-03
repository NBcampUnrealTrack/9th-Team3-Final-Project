using UnrealBuildTool;

/**
 * Development-only tools: monster cheats and test helpers.
 * The module is a DeveloperTool, so it is never built into Shipping; game code must not depend on it.
 */
public class knightDev : ModuleRules
{
	public knightDev(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AIModule",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"knight",
			"knightPerfTracker"
		});
	}
}
