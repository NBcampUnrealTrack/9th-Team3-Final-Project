using UnrealBuildTool;

/** Viewport tool for placing part meshes on the mech and writing FMechPartAttachment. In development. */
public class MechPartsWorkbenchEditor : ModuleRules
{
	public MechPartsWorkbenchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"MechPartsWorkbenchRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"Slate",
			"SlateCore"
		});
	}
}
