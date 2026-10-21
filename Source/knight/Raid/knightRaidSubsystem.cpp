#include "Raid/knightRaidSubsystem.h"
#include "knight.h"

FGuid UknightRaidSubsystem::BeginRun(FName StageId, FName QuestId, FName BossId, EKnightQuestDifficulty Difficulty,
	const FString& SlotId)
{
	CurrentRunId = FGuid::NewGuid();
	SelectedStageId = StageId;
	ActiveQuestContext = FKnightQuestRunContext();
	if (!QuestId.IsNone() && !BossId.IsNone() && !StageId.IsNone() && !SlotId.IsEmpty())
	{
		ActiveQuestContext.RunId = CurrentRunId;
		ActiveQuestContext.QuestId = QuestId;
		ActiveQuestContext.StageId = StageId;
		ActiveQuestContext.BossId = BossId;
		ActiveQuestContext.SlotId = SlotId;
		ActiveQuestContext.Difficulty = Difficulty;
		UE_LOG(Logknight, Log, TEXT("Quest run started quest=%s stage=%s boss=%s difficulty=%d run=%s"),
			*QuestId.ToString(), *StageId.ToString(), *BossId.ToString(), static_cast<int32>(Difficulty), *CurrentRunId.ToString());
	}
	return CurrentRunId;
}

void UknightRaidSubsystem::CancelRun(FGuid RunId)
{
	if (CurrentRunId == RunId && (!bHasResult || LastResult.RunId != RunId))
	{
		CurrentRunId.Invalidate();
		SelectedStageId = NAME_None;
		ActiveQuestContext = FKnightQuestRunContext();
	}
}

void UknightRaidSubsystem::EndRun()
{
	CurrentRunId.Invalidate();
	SelectedStageId = NAME_None;
	ActiveQuestContext = FKnightQuestRunContext();
}

bool UknightRaidSubsystem::GetActiveQuestRunContext(FKnightQuestRunContext& OutContext) const
{
	OutContext = ActiveQuestContext;
	return ActiveQuestContext.IsValid() && ActiveQuestContext.RunId == CurrentRunId;
}

bool UknightRaidSubsystem::RecordResult(const FknightRaidResult& Result)
{
	if (!Result.RunId.IsValid() || Result.RunId != CurrentRunId
		|| (bHasResult && LastResult.RunId == Result.RunId))
	{
		return false;
	}
	LastResult = Result;
	LastResult.QuestContext = FKnightQuestRunContext();
	if (ActiveQuestContext.IsValid() && ActiveQuestContext.RunId == Result.RunId
		&& ActiveQuestContext.StageId == Result.StageId)
	{
		LastResult.QuestContext = ActiveQuestContext;
	}
	bHasResult = true;
	// Progression may replace LastResult with the settled payout during the broadcast.
	// Keep delegate arguments independent of that storage.
	const FknightRaidResult BroadcastResult = LastResult;
	OnRaidResultRecorded.Broadcast(BroadcastResult);
	ActiveQuestContext = FKnightQuestRunContext();
	return true;
}

bool UknightRaidSubsystem::SetSettledResult(const FknightRaidResult& SettledResult)
{
	if (!bHasResult || !SettledResult.RunId.IsValid() || LastResult.RunId != SettledResult.RunId)
	{
		return false;
	}
	LastResult = SettledResult;
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
