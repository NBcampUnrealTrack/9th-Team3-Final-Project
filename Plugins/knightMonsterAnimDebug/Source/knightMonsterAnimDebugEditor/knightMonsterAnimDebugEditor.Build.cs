using UnrealBuildTool;

/** 몬스터 애니메이션 커버리지 보드(에디터 탭). 런타임 모듈의 설정과 노티파이 판별을 같이 쓴다. */
public class knightMonsterAnimDebugEditor : ModuleRules
{
	public knightMonsterAnimDebugEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AdvancedPreviewScene",
			"ApplicationCore",
			"AssetRegistry",
			"GameplayAbilities",
			"InputCore",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
			"knightMonsterAnimDebug"
		});
	}
}
