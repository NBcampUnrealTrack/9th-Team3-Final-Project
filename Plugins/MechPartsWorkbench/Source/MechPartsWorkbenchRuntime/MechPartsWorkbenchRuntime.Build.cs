using UnrealBuildTool;

/** Runtime data written by the workbench and read by the game. No dependency on the game module. */
public class MechPartsWorkbenchRuntime : ModuleRules
{
	public MechPartsWorkbenchRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
