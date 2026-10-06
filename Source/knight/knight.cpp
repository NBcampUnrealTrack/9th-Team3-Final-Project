// Copyright Epic Games, Inc. All Rights Reserved.

#include "knight.h"
#include "Modules/ModuleManager.h"
#include "System/knightTeams.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "Debug/knightGameplayDebuggerCategories.h"
#endif

/** Primary game module. Registers the team attitude solver and project-wide debug tooling. */
class FknightModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Player/Ally friendly, NoTeam neutral, everything else hostile. Without this the engine default
		// (same team friendly, any other team hostile) applies, so allied objectives and neutral actors read as enemies.
		knightTeams::RegisterAttitudeSolver();

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
