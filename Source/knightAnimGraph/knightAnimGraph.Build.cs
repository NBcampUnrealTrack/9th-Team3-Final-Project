using UnrealBuildTool;

/**
 * Anim graph nodes for knight runtime anim nodes (the editor-side half of FAnimNode_* in the knight module).
 * UncookedOnly so the nodes also load in uncooked -game runs (Play in Standalone), not only in the editor.
 */
public class knightAnimGraph : ModuleRules
{
	public knightAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"knight"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph"
		});
	}
}
