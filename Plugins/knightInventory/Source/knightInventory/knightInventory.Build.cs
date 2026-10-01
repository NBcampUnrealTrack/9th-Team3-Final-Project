using UnrealBuildTool;

/** Generic inventory. Must never depend on the game module; the game extends it instead. */
public class knightInventory : ModuleRules
{
	public knightInventory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags"
		});
	}
}
