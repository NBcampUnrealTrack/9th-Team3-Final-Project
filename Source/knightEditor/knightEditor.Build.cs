using UnrealBuildTool;

/** Editor-only tooling: data validation, asset utilities and editor extensions. */
public class knightEditor : ModuleRules
{
	public knightEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"knight"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"GameplayAbilities",
			"GameplayTags",
			// 몬스터 공격 미리보기 플러그인의 공격 선택 시뮬레이터 인터페이스(knightMonsterAttackSelectionSimulator).
			"knightMonsterAnimDebugEditor",
			"UnrealEd",
			// UI 위젯 블루프린트 트리 생성 도구(knightUIEditorLibrary).
			"UMG",
			"UMGEditor",
			"CommonUI",
			"knightUI",
			"Slate",
			"SlateCore"
		});
	}
}
