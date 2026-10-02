#include "Raid/knightRaidSubsystem.h"

FGuid UknightRaidSubsystem::BeginRun(FName StageId)
{
	CurrentRunId = FGuid::NewGuid();
	SelectedStageId = StageId;
	return CurrentRunId;
}

void UknightRaidSubsystem::CancelRun(FGuid RunId)
{
	if (CurrentRunId == RunId && (!bHasResult || LastResult.RunId != RunId))
	{
		CurrentRunId.Invalidate();
		SelectedStageId = NAME_None;
	}
}

bool UknightRaidSubsystem::RecordResult(const FknightRaidResult& Result)
{
	if (!Result.RunId.IsValid() || Result.RunId != CurrentRunId
		|| (bHasResult && LastResult.RunId == Result.RunId))
	{
		return false;
	}
	LastResult = Result;
	bHasResult = true;
	OnRaidResultRecorded.Broadcast(LastResult);
	return true;
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
