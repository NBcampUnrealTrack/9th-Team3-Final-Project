#include "Monster/knightMonsterDebug.h"

#include "HAL/IConsoleManager.h"

namespace knightMonsterDebug
{
	static int32 DrawAttacks = 0;
	static FAutoConsoleVariableRef CVarDrawAttacks(
		TEXT("knight.Monster.DrawAttacks"), DrawAttacks,
		TEXT("Draw monster attack telegraphs and hit shapes (red: miss, magenta: hit). 0: off, 1: on"), ECVF_Cheat);

	bool ShouldDrawAttacks()
	{
		return DrawAttacks != 0;
	}
}
