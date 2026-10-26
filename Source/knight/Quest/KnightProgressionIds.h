#pragma once

#include "CoreMinimal.h"

/** Released IDs remain accepted at API/save boundaries. Asset paths and other quests are unchanged. */
namespace KnightProgressionIds
{
	inline FName Quest(FName Id)
	{
		if (Id == TEXT("Quest_Janyeol_Normal")) return TEXT("Quest_0001");
		if (Id == TEXT("Quest_Janhyang_Normal")) return TEXT("Quest_0002");
		return Id;
	}

	inline FName Stage(FName Id)
	{
		if (Id == TEXT("Stage_01")) return TEXT("Stage_0001");
		if (Id == TEXT("Stage_02")) return TEXT("Stage_0002");
		return Id;
	}
}
