#include "Progression/knightProgressionSubsystem.h"

#include "Engine/GameInstance.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "knightInventoryLibrary.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"
#include "Raid/knightRaidDefinition.h"
#include "knight.h"

namespace
{
	const UknightPartItemDefinition* GetImplicitStarterPart(FGameplayTag Slot)
	{
		const auto Defaults = knightEquipmentStats::ReadLoadout(nullptr, UknightEquipmentSettings::LoadCatalog());
		return Defaults.FindRef(Slot);
	}
}

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
	if (!Save || !Result.RunId.IsValid() || Save->AppliedRaidRuns.Contains(Result.RunId))
	{
		return;
	}
	Save->AppliedRaidRuns.Add(Result.RunId);
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
		if (Result.bSuccess && Reward.Item && Reward.Count > 0 && Reward.Item->GetPrimaryAssetId().IsValid()
			&& Save->ItemCounts.FindRef(Reward.Item->GetPrimaryAssetId()) <= MAX_int32 - Reward.Count)
		{
			Save->ItemCounts.FindOrAdd(Reward.Item->GetPrimaryAssetId()) += Reward.Count;
		}
	}
	CommitChange();
}

bool UknightProgressionSubsystem::IsStageUnlocked(FName StageId) const
{
	const UknightSaveGame* const Save = GetSave();
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FKnightStageDefinition Stage;
	if (!Save || !Flow || !Flow->FindStageDefinition(StageId, Stage)) return false;
	if (Save->UnlockedStages.Contains(StageId)) return true;
	if (Stage.RaidDefinition.IsNull()) return true; // Existing flow-test entries remain available.
	const UknightRaidDefinition* const Definition = Stage.RaidDefinition.LoadSynchronous();
	return Definition && (Definition->RequiredClearedStageId.IsNone()
		|| IsStageCleared(Definition->RequiredClearedStageId));
}

bool UknightProgressionSubsystem::UnlockStage(FName StageId)
{
	UknightSaveGame* const Save = GetSave();
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FKnightStageDefinition Stage;
	if (!Save || !Flow || !Flow->FindStageDefinition(StageId, Stage)) return false;
	if (!Save->UnlockedStages.Contains(StageId))
	{
		Save->UnlockedStages.Add(StageId);
		CommitChange();
	}
	return true;
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
	if (!Save || !Item || !Item->GetPrimaryAssetId().IsValid() || Count <= 0
		|| GetItemCount(Item) > MAX_int32 - Count)
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
	TMap<FPrimaryAssetId, int32> TotalCosts;
	for (const FknightItemCost& Cost : Part->UpgradeSteps[Level].Costs)
	{
		if (!Cost.Item || !Cost.Item->GetPrimaryAssetId().IsValid() || Cost.Count <= 0)
		{
			return false;
		}
		int32& Total = TotalCosts.FindOrAdd(Cost.Item->GetPrimaryAssetId());
		if (Total > MAX_int32 - Cost.Count)
		{
			return false;
		}
		Total += Cost.Count;
	}
	for (const TPair<FPrimaryAssetId, int32>& Cost : TotalCosts)
	{
		const UknightSaveGame* const Save = GetSave();
		if (!Save || Save->ItemCounts.FindRef(Cost.Key) < Cost.Value) return false;
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

bool UknightProgressionSubsystem::IsSlotExplicitlyEmpty(FGameplayTag SlotTag) const
{
	const UknightSaveGame* const Save = GetSave();
	return Save && Save->ExplicitlyEmptySlots.Contains(SlotTag);
}

bool UknightProgressionSubsystem::CanCraftPart(const UknightCraftRecipeDefinition* Recipe) const
{
	const UknightSaveGame* const Save = GetSave();
	if (!Save || !Recipe || !Recipe->Result || !Recipe->Result->GetPrimaryAssetId().IsValid()
		|| Recipe->Costs.IsEmpty() || (!Recipe->RequiredClearedStageId.IsNone() && !IsStageCleared(Recipe->RequiredClearedStageId)))
	{
		return false;
	}
	TMap<FPrimaryAssetId, int32> Totals;
	for (const FknightItemCost& Cost : Recipe->Costs)
	{
		if (!Cost.Item || Cost.Count <= 0 || !Cost.Item->GetPrimaryAssetId().IsValid()) return false;
		int32& Total = Totals.FindOrAdd(Cost.Item->GetPrimaryAssetId());
		if (Total > MAX_int32 - Cost.Count) return false;
		Total += Cost.Count;
	}
	for (const auto& Cost : Totals)
	{
		if (Save->ItemCounts.FindRef(Cost.Key) < Cost.Value) return false;
	}
	return GetItemCount(Recipe->Result) < MAX_int32;
}

bool UknightProgressionSubsystem::CraftPart(const UknightCraftRecipeDefinition* Recipe)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || !CanCraftPart(Recipe)) return false;
	for (const FknightItemCost& Cost : Recipe->Costs)
	{
		const FPrimaryAssetId Id = Cost.Item->GetPrimaryAssetId();
		int32& Owned = Save->ItemCounts.FindChecked(Id);
		Owned -= Cost.Count;
		if (Owned == 0) Save->ItemCounts.Remove(Id);
	}
	++Save->ItemCounts.FindOrAdd(Recipe->Result->GetPrimaryAssetId());
	CommitChange();
	return true;
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
	// A starter part can be equipped without an inventory entry. Keep it when first replaced.
	if (!Save->EquippedItems.Contains(Part->SlotTag) && !Save->ExplicitlyEmptySlots.Contains(Part->SlotTag))
	{
		const UknightPartItemDefinition* Starter = GetImplicitStarterPart(Part->SlotTag);
		if (Starter)
			++Save->ItemCounts.FindOrAdd(Starter->GetPrimaryAssetId());
	}
	Save->EquippedItems.Add(Part->SlotTag, Part->GetPrimaryAssetId());
	Save->ExplicitlyEmptySlots.Remove(Part->SlotTag);
	CommitChange();
	return true;
}

void UknightProgressionSubsystem::ClearEquippedSlot(FGameplayTag SlotTag)
{
	UknightSaveGame* const Save = GetSave();
	if (!Save || !SlotTag.IsValid()) return;
	const bool bImplicitStarter = !Save->EquippedItems.Contains(SlotTag) && !Save->ExplicitlyEmptySlots.Contains(SlotTag);
	if (Save->EquippedItems.Remove(SlotTag) > 0 || !Save->ExplicitlyEmptySlots.Contains(SlotTag))
	{
		// Unequipping an implicit starter places it in the box instead of making it disappear.
		if (bImplicitStarter)
		{
			const UknightPartItemDefinition* Starter = GetImplicitStarterPart(SlotTag);
			if (Starter)
				++Save->ItemCounts.FindOrAdd(Starter->GetPrimaryAssetId());
		}
		Save->ExplicitlyEmptySlots.Add(SlotTag);
		CommitChange();
	}
}
