#include "System/knightTeams.h"

namespace knightTeams
{
	ETeamAttitude::Type SolveAttitude(FGenericTeamId A, FGenericTeamId B)
	{
		if (A == FGenericTeamId::NoTeam || B == FGenericTeamId::NoTeam)
		{
			return ETeamAttitude::Neutral;
		}
		if (A == B)
		{
			return ETeamAttitude::Friendly;
		}
		const FGenericTeamId Player = ToTeamId(EknightTeam::Player);
		const FGenericTeamId Ally = ToTeamId(EknightTeam::Ally);
		if ((A == Player && B == Ally) || (A == Ally && B == Player))
		{
			return ETeamAttitude::Friendly;
		}
		return ETeamAttitude::Hostile;
	}

	void RegisterAttitudeSolver()
	{
		FGenericTeamId::SetAttitudeSolver(&SolveAttitude);
	}
}
