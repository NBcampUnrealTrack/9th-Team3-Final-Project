using UnrealBuildTool;

/**
 * 몬스터 애니메이션 작업용 디버그 도구. DeveloperTool 모듈이라 Shipping 빌드에는 포함되지 않는다.
 * 게임 모듈에 의존하지 않고 GAS, StateTree, 리플렉션으로만 게임을 읽으므로, 게임 코드가 이 모듈에 의존해서도 안 된다.
 */
public class knightMonsterAnimDebug : ModuleRules
{
	public knightMonsterAnimDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AIModule",
			"GameplayAbilities",
			"GameplayTasks",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"knightPerfTracker"
		});

		SetupGameplayDebuggerSupport(Target);
	}
}
