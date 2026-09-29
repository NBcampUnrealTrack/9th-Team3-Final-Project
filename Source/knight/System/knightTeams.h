#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "knightTeams.generated.h"

/** Team IDs used with IGenericTeamAgentInterface. Attitudes come from knightTeams::SolveAttitude. */
UENUM(BlueprintType)
enum class EknightTeam : uint8
{
	/** Engine default team ID. Never assign it; it exists so a default-initialized value is detectable. */
	Unassigned = 0 UMETA(Hidden),
	Player = 1,
	Monster = 2,
	/** Protected objectives and future allied NPCs. Friendly to Player, hostile to Monster. */
	Ally = 3,
	Neutral = 255 UMETA(DisplayName="Neutral (NoTeam)")
};

namespace knightTeams
{
	inline FGenericTeamId ToTeamId(EknightTeam Team)
	{
		return FGenericTeamId(static_cast<uint8>(Team));
	}

	/** Same team or Player/Ally: friendly. NoTeam on either side: neutral. Otherwise hostile. */
	KNIGHT_API ETeamAttitude::Type SolveAttitude(FGenericTeamId A, FGenericTeamId B);

	/** Installs SolveAttitude as the engine attitude solver. Called once at startup. */
	KNIGHT_API void RegisterAttitudeSolver();
}
