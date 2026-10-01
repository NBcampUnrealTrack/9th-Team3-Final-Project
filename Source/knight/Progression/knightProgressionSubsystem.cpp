#include "Progression/knightProgressionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Item/knightItemDefinitions.h"
#include "knightInventoryLibrary.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"
#include "knight.h"

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
	for (const FknightItemStack& Reward : Result.Rewards)
	{
		if (Reward.Item && Reward.Count > 0)
		{
			Save->ItemCounts.FindOrAdd(Reward.Item->GetPrimaryAssetId()) += Reward.Count;
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

int32 UknightProgressionSubsystem::GetItemCount(const UknightItemDefinition* Item) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Count = Save && Item ? Save->ItemCounts.Find(Item->GetPrimaryAssetId()) : nullptr;
	return Count ? *Count : 0;
}

void UknightProgressionSubsystem::AddItem(const UknightItemDefinition* Item, int32 Count)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || !Item || Count <= 0)
	{
		return;
	}
	Save->ItemCounts.FindOrAdd(Item->GetPrimaryAssetId()) += Count;
	CommitChange();
}

bool UknightProgressionSubsystem::ConsumeItem(const UknightItemDefinition* Item, int32 Count)
{
	UknightSaveGame* const Save = GetSave();
	const FPrimaryAssetId ItemId = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
	int32* const Owned = Save ? Save->ItemCounts.Find(ItemId) : nullptr;
	if (!Owned || Count <= 0 || *Owned < Count)
	{
		return false;
	}
	*Owned -= Count;
	if (*Owned == 0)
	{
		Save->ItemCounts.Remove(ItemId);
	}
	CommitChange();
	return true;
}

void UknightProgressionSubsystem::GetOwnedItems(TArray<FknightItemStack>& OutItems) const
{
	OutItems.Reset();
	const UknightSaveGame* const Save = GetSave();
	if (!Save)
	{
		return;
	}
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save->ItemCounts)
	{
		if (const UknightItemDefinition* const Item = UknightInventoryLibrary::LoadItemDefinition(Entry.Key))
		{
			OutItems.Emplace(Item, Entry.Value);
		}
	}
}

int32 UknightProgressionSubsystem::GetUpgradeLevel(const UknightItemDefinition* Item) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Level = Save && Item ? Save->ItemUpgradeLevels.Find(Item->GetPrimaryAssetId()) : nullptr;
	return Level ? *Level : 0;
}

bool UknightProgressionSubsystem::CanUpgradePart(const UknightPartItemDefinition* Part) const
{
	if (!Part || GetItemCount(Part) <= 0)
	{
		return false;
	}
	const int32 Level = GetUpgradeLevel(Part);
	if (!Part->UpgradeSteps.IsValidIndex(Level))
	{
		return false;
	}
	for (const FknightItemCost& Cost : Part->UpgradeSteps[Level].Costs)
	{
		if (!Cost.Item || GetItemCount(Cost.Item) < Cost.Count)
		{
			return false;
		}
	}
	return true;
}

bool UknightProgressionSubsystem::UpgradePart(const UknightPartItemDefinition* Part)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || !CanUpgradePart(Part))
	{
		return false;
	}
	const int32 Level = GetUpgradeLevel(Part);
	// Every cost was checked above, so these removals cannot fail part-way.
	for (const FknightItemCost& Cost : Part->UpgradeSteps[Level].Costs)
	{
		const FPrimaryAssetId CostId = Cost.Item->GetPrimaryAssetId();
		int32& Owned = Save->ItemCounts.FindChecked(CostId);
		Owned -= Cost.Count;
		if (Owned == 0)
		{
			Save->ItemCounts.Remove(CostId);
		}
	}
	Save->ItemUpgradeLevels.Add(Part->GetPrimaryAssetId(), Level + 1);
	UE_LOG(Logknight, Log, TEXT("Upgraded %s to level %d."), *Part->GetName(), Level + 1);
	CommitChange();
	return true;
}

const UknightPartItemDefinition* UknightProgressionSubsystem::GetEquippedPart(FGameplayTag SlotTag) const
{
	const UknightSaveGame* const Save = GetSave();
	const FPrimaryAssetId* const ItemId = Save ? Save->EquippedItems.Find(SlotTag) : nullptr;
	return ItemId ? Cast<UknightPartItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(*ItemId)) : nullptr;
}

bool UknightProgressionSubsystem::SetEquippedPart(const UknightPartItemDefinition* Part)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || !Part || !Part->SlotTag.IsValid())
	{
		return false;
	}
	if (GetItemCount(Part) <= 0)
	{
		UE_LOG(Logknight, Warning, TEXT("Cannot equip %s: not owned."), *Part->GetName());
		return false;
	}
	Save->EquippedItems.Add(Part->SlotTag, Part->GetPrimaryAssetId());
	CommitChange();
	return true;
}

void UknightProgressionSubsystem::ClearEquippedSlot(FGameplayTag SlotTag)
{
	UknightSaveGame* const Save = GetSave();
	if (Save && Save->EquippedItems.Remove(SlotTag) > 0)
	{
		CommitChange();
	}
}
