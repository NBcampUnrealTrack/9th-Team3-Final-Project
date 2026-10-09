using UnrealBuildTool;

/** UI layer: CommonUI layout, widget bases and ViewModels. Depends on knight, never the reverse. */
public class knightUI : ModuleRules
{
	public knightUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat module layout: includes are relative to the module root (Foundation/..., ViewModels/...).
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"FieldNotification",
			"GameplayTags",
			"GameplayAbilities",
			"UIExtension",
			"knight"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"InputCore",
			"knightInventory",
			"knightAudio",
			"CommonLoadingScreen",
			"DeveloperSettings"
		});
	}
}
