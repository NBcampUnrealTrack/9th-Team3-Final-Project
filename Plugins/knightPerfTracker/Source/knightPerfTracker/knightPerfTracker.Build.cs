using UnrealBuildTool;

/** Performance capture for tests and before/after comparisons. Must never depend on the game module. */
public class knightPerfTracker : ModuleRules
{
	public knightPerfTracker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"RenderCore",
			"RHI",
			"Json"
		});
	}
}
