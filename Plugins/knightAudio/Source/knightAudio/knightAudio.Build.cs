using UnrealBuildTool;

/** Audio services for the game. Must never depend on the game module. */
public class knightAudio : ModuleRules
{
	public knightAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AudioModulation",
			"AudioExtensions"
		});
	}
}
