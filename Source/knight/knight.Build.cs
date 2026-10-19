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
			"ApplicationCore",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"DeveloperSettings",
			"GameplayTags",
			"GameplayTasks",
			"GameplayAbilities",
			"MotionWarping",
			// knightAnimNode_LegPlacement (SkeletalControlBase를 상속하는 몬스터 발 맞추기 노드).
			"AnimGraphRuntime",
			"knightInventory",
			"MechPartsWorkbenchRuntime",
			"knightAudio",
			"CommonLoadingScreen",
			// 메뉴 버튼(UCommonButtonBase). 디자인 쪽 버튼 클래스는 knightUI에 있고, 메뉴는 부모 타입으로만 받는다.
			"CommonUI",
			"UMG",
			"Slate",
			"SlateCore"
		});

		// knightPerfTracker: KNIGHT_PERF_* counters for test captures; compiled out in Shipping.
		PrivateDependencyModuleNames.AddRange(new string[] { "knightPerfTracker", "Niagara", "DLSSBlueprint", "RHI", "RenderCore" });

		// 2본 IK 풀이(AnimationCore::SolveTwoBoneIK).
		PrivateDependencyModuleNames.Add("AnimationCore");

		// 얕은 물 파동: 수면 높이 질의(UWaterBodyComponent, FWaterBodyManager).
		PrivateDependencyModuleNames.Add("Water");

		// Defines WITH_GAMEPLAY_DEBUGGER and adds the debugger modules in builds that include it.
		SetupGameplayDebuggerSupport(Target);

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
