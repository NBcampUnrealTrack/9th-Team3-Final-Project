using UnrealBuildTool;

/**
 * 개발 전용 도구 모음: 몬스터 치트와 테스트 도우미.
 * DeveloperTool 모듈이라 Shipping 빌드에는 포함되지 않으므로, 게임 코드가 이 모듈에 의존하면 안 된다.
 */
public class knightDev : ModuleRules
{
	public knightDev(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AIModule",
			"AssetRegistry",
			"CommonUI",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"knight",
			"knightUI",
			"knightInventory",
			"knightPerfTracker",
			"UMG"
		});
	}
}
