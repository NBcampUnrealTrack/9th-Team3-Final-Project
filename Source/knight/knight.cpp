// Copyright Epic Games, Inc. All Rights Reserved.

#include "knight.h"
#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "Debug/knightGameplayDebuggerCategories.h"
#endif

/** Primary game module. Registers project-wide debug tooling. */
class FknightModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		knightGameplayDebugger::RegisterCategories();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		knightGameplayDebugger::UnregisterCategories();
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FknightModule, knight, "knight" );

DEFINE_LOG_CATEGORY(Logknight)
DEFINE_LOG_CATEGORY(LogknightAbility)
DEFINE_LOG_CATEGORY(LogknightCombat)
DEFINE_LOG_CATEGORY(LogknightAI)
DEFINE_LOG_CATEGORY(LogknightCamera)
DEFINE_LOG_CATEGORY(LogknightData)
