// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class knight : ModuleRules
{
	public knight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"DeveloperSettings",
			"GameplayTags",
			"GameplayTasks",
			"GameplayAbilities",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"knight",
			"knight/Variant_Platforming",
			"knight/Variant_Platforming/Animation",
			"knight/Variant_Combat",
			"knight/Variant_Combat/AI",
			"knight/Variant_Combat/Animation",
			"knight/Variant_Combat/Gameplay",
			"knight/Variant_Combat/Interfaces",
			"knight/Variant_Combat/UI",
			"knight/Variant_SideScrolling",
			"knight/Variant_SideScrolling/AI",
			"knight/Variant_SideScrolling/Gameplay",
			"knight/Variant_SideScrolling/Interfaces",
			"knight/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
