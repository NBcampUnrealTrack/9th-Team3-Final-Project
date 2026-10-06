#include "knightAnimDebugModule.h"

#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_knightMonsterAnim.h"
#endif

DEFINE_LOG_CATEGORY(LogknightAnimDebug)

namespace knightAnimDebug
{
	static const FName CategoryName(TEXT("MonsterAnim"));
}

/** Gameplay Debugger에 "MonsterAnim" 카테고리를 등록한다. 게임에서 ' 키로 디버거를 켜면 보인다. */
class FknightMonsterAnimDebugModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
		GameplayDebugger.RegisterCategory(knightAnimDebug::CategoryName,
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_knightMonsterAnim::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGame);
		GameplayDebugger.NotifyCategoriesChanged();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
			GameplayDebugger.UnregisterCategory(knightAnimDebug::CategoryName);
			GameplayDebugger.NotifyCategoriesChanged();
		}
#endif
	}
};

IMPLEMENT_MODULE(FknightMonsterAnimDebugModule, knightMonsterAnimDebug);
