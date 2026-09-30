#include "Raid/knightRaidSubsystem.h"

void UknightRaidSubsystem::RecordResult(const FknightRaidResult& Result)
{
	LastResult = Result;
	bHasResult = true;
	OnRaidResultRecorded.Broadcast(LastResult);
}

bool UknightRaidSubsystem::GetLastResult(FknightRaidResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = LastResult;
	return true;
}

void UknightRaidSubsystem::ClearLastResult()
{
	LastResult = FknightRaidResult();
	bHasResult = false;
}
