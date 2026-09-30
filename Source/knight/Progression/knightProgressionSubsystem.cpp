#include "Progression/knightProgressionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"

void UknightProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UknightSaveSubsystem>();
	Collection.InitializeDependency<UknightRaidSubsystem>();
	Super::Initialize(Collection);

	if (UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
	{
		Raid->OnRaidResultRecorded.AddUniqueDynamic(this, &ThisClass::HandleRaidResult);
	}
}

void UknightProgressionSubsystem::Deinitialize()
{
	if (UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
	{
		Raid->OnRaidResultRecorded.RemoveDynamic(this, &ThisClass::HandleRaidResult);
	}
	Super::Deinitialize();
}

UknightSaveGame* UknightProgressionSubsystem::GetSave() const
{
	const UknightSaveSubsystem* const SaveSubsystem = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	return SaveSubsystem ? SaveSubsystem->GetSaveGame() : nullptr;
}

void UknightProgressionSubsystem::CommitChange()
{
	if (UknightSaveSubsystem* const SaveSubsystem = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		SaveSubsystem->RequestSave();
	}
	OnProgressionChanged.Broadcast();
}

void UknightProgressionSubsystem::HandleRaidResult(const FknightRaidResult& Result)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save)
	{
		return;
	}
	Save->TotalPlayTime += Result.ElapsedTime;
	if (Result.bSuccess && !Result.StageId.IsNone())
	{
		FknightStageRecord& Record = Save->StageRecords.FindOrAdd(Result.StageId);
		++Record.ClearCount;
		if (Record.BestClearTime <= 0.f || Result.ElapsedTime < Record.BestClearTime)
		{
			Record.BestClearTime = Result.ElapsedTime;
		}
	}
	CommitChange();
}

bool UknightProgressionSubsystem::IsStageCleared(FName StageId) const
{
	const UknightSaveGame* const Save = GetSave();
	const FknightStageRecord* const Record = Save ? Save->StageRecords.Find(StageId) : nullptr;
	return Record && Record->ClearCount > 0;
}

float UknightProgressionSubsystem::GetBestClearTime(FName StageId) const
{
	const UknightSaveGame* const Save = GetSave();
	const FknightStageRecord* const Record = Save ? Save->StageRecords.Find(StageId) : nullptr;
	return Record ? Record->BestClearTime : 0.f;
}

int32 UknightProgressionSubsystem::GetMaterialCount(FName ItemId) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Count = Save ? Save->Materials.Find(ItemId) : nullptr;
	return Count ? *Count : 0;
}

void UknightProgressionSubsystem::AddMaterial(FName ItemId, int32 Count)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || ItemId.IsNone() || Count <= 0)
	{
		return;
	}
	Save->Materials.FindOrAdd(ItemId) += Count;
	CommitChange();
}

bool UknightProgressionSubsystem::ConsumeMaterial(FName ItemId, int32 Count)
{
	UknightSaveGame* const Save = GetSave();
	int32* const Owned = Save ? Save->Materials.Find(ItemId) : nullptr;
	if (!Owned || Count <= 0 || *Owned < Count)
	{
		return false;
	}
	*Owned -= Count;
	if (*Owned == 0)
	{
		Save->Materials.Remove(ItemId);
	}
	CommitChange();
	return true;
}

int32 UknightProgressionSubsystem::GetUpgradeLevel(FName ItemId) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Level = Save ? Save->UpgradeLevels.Find(ItemId) : nullptr;
	return Level ? *Level : 0;
}

void UknightProgressionSubsystem::SetUpgradeLevel(FName ItemId, int32 Level)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || ItemId.IsNone())
	{
		return;
	}
	Save->UpgradeLevels.Add(ItemId, FMath::Max(0, Level));
	CommitChange();
}
