using UnrealBuildTool;

/** UI layer: CommonUI layout, widget bases and ViewModels. Depends on knight, never the reverse. */
public class knightUI : ModuleRules
{
	public knightUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"knight"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore"
		});
	}
}
