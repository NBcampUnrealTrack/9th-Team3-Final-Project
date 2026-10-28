#include "Progression/knightProgressionSubsystem.h"

#include "Engine/GameInstance.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightPartsComponent.h"
#include "knightInventoryLibrary.h"
#include "Quest/KnightQuestData.h"
#include "Quest/KnightProgressionIds.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidSubsystem.h"
#include "Raid/knightRaidDefinition.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const UknightPartItemDefinition* GetImplicitStarterPart(FGameplayTag Slot)
	{
		const auto Defaults = knightEquipmentStats::ReadLoadout(nullptr, UknightEquipmentSettings::LoadCatalog());
		return Defaults.FindRef(Slot);
	}

	// The default weapon already belongs to a fresh profile. Resolve it without granting
	// an item or saving merely because a screen/quote is opened. Replacement materializes
	// this same copy in ItemCounts before overriding the default-loadout slot.
	const UknightWeaponItemDefinition* GetImplicitOwnedWeapon(const UknightSaveGame* Save)
	{
		const FGameplayTag Slot = knightGameplayTags::Equipment_Slot_Weapon;
		if (!Save || Save->EquippedItems.Contains(Slot) || Save->ExplicitlyEmptySlots.Contains(Slot)) return nullptr;
		return Cast<UknightWeaponItemDefinition>(GetImplicitStarterPart(Slot));
	}

	bool RecordQuestCompletionInSave(UknightSaveGame& Save, FName QuestId)
	{
		QuestId = KnightProgressionIds::Quest(QuestId);
		if (QuestId.IsNone() || Save.CompletedQuestIds.Contains(QuestId)) return false;
		Save.CompletedQuestIds.Add(QuestId);
		return true;
	}

	bool ResolveOwnedDefinitions(const UknightSaveGame* Save, TArray<FknightItemStack>& OutItems)
	{
		OutItems.Reset();
		if (!Save) return false;
		bool bComplete = true;
		for (const TPair<FPrimaryAssetId, int32>& Entry : Save->ItemCounts)
		{
			if (Entry.Value <= 0) continue;
			const UknightItemDefinition* Item = UknightInventoryLibrary::LoadItemDefinition(Entry.Key);
			if (!Item || Item->GetPrimaryAssetId() != Entry.Key)
			{
				bComplete = false;
				UE_LOG(Logknight, Warning, TEXT("Owned item definition cannot be resolved: %s"), *Entry.Key.ToString());
				continue;
			}
			OutItems.Emplace(Item, Entry.Value);
		}
		if (const UknightWeaponItemDefinition* Starter = GetImplicitOwnedWeapon(Save))
			if (Starter->GetPrimaryAssetId().IsValid() && Save->ItemCounts.FindRef(Starter->GetPrimaryAssetId()) <= 0)
				OutItems.Emplace(Starter, 1);
		return bComplete;
	}

	template <typename TItem>
	bool ResolveOwnedCategory(const UknightSaveGame* Save, FGameplayTag ExpectedType, TArray<FknightItemStack>& OutItems)
	{
		TArray<FknightItemStack> All;
		bool bComplete = ResolveOwnedDefinitions(Save, All);
		OutItems.Reset();
		for (const FknightItemStack& Stack : All)
		{
			const bool bClassMatch = Stack.Item->IsA<TItem>();
			const bool bTypeMatch = Stack.Item->ItemType == ExpectedType;
			if (bClassMatch && bTypeMatch) OutItems.Add(Stack);
			else if (bClassMatch != bTypeMatch)
			{
				bComplete = false;
				UE_LOG(Logknight, Warning, TEXT("Owned item class/type mismatch: %s"),
					*Stack.Item->GetPrimaryAssetId().ToString());
			}
		}
		return bComplete;
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

bool UknightProgressionSubsystem::RecordQuestCompleted(FName QuestId)
{
	const UknightSaveSubsystem* const SaveSubsystem = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = SaveSubsystem && SaveSubsystem->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save || !RecordQuestCompletionInSave(*Save, QuestId)) return false;
	CommitChange();
	return true;
}

bool UknightProgressionSubsystem::IsQuestCompleted(FName QuestId) const
{
	QuestId = KnightProgressionIds::Quest(QuestId);
	const UknightSaveSubsystem* const SaveSubsystem = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = SaveSubsystem && SaveSubsystem->HasSelectedSlot() ? GetSave() : nullptr;
	return Save && !QuestId.IsNone() && Save->CompletedQuestIds.Contains(QuestId);
}

int64 UknightProgressionSubsystem::GetMoney() const
{
	const UknightSaveGame* const Save = GetSave();
	return Save ? Save->Money : 0;
}

bool UknightProgressionSubsystem::AddMoney(int64 Amount)
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save || Amount <= 0 || Save->Money > MAX_int64 - Amount) return false;
	Save->Money += Amount;
	CommitChange();
	return true;
}

bool UknightProgressionSubsystem::SpendMoney(int64 Amount)
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save || Amount <= 0 || Save->Money < Amount) return false;
	Save->Money -= Amount;
	CommitChange();
	return true;
}

int32 UknightProgressionSubsystem::GetUnlockedSharedSocketSlots() const
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	return Save ? Save->UnlockedSharedSocketSlots : 0;
}

const UknightSocketItemDefinition* UknightProgressionSubsystem::GetEquippedSocketItem(int32 SlotIndex) const
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save || SlotIndex < 0 || SlotIndex >= Save->UnlockedSharedSocketSlots
		|| !Save->EquippedSocketItems.IsValidIndex(SlotIndex)) return nullptr;
	const FPrimaryAssetId Id = Save->EquippedSocketItems[SlotIndex];
	if (!Id.IsValid() || Save->ItemCounts.FindRef(Id) <= 0) return nullptr;
	const UknightSocketItemDefinition* const Item = Cast<UknightSocketItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	return Item && Item->GetPrimaryAssetId() == Id && Item->ItemType == knightGameplayTags::Item_Type_Socket ? Item : nullptr;
}

int32 UknightProgressionSubsystem::GetAvailableSocketItemCount(const UknightSocketItemDefinition* Item) const
{
	const FPrimaryAssetId Id = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save || !Id.IsValid() || Item->ItemType != knightGameplayTags::Item_Type_Socket) return 0;
	int32 Used = 0;
	for (int32 Index = 0; Index < Save->UnlockedSharedSocketSlots && Index < Save->EquippedSocketItems.Num(); ++Index)
		if (Save->EquippedSocketItems[Index] == Id) ++Used;
	return FMath::Max(0, Save->ItemCounts.FindRef(Id) - Used);
}

bool UknightProgressionSubsystem::EquipSocketItem(int32 SlotIndex, const UknightSocketItemDefinition* Item)
{
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->CanStartAtomicTransaction() ? GetSave() : nullptr;
	const FPrimaryAssetId Id = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
	if (!Save || SlotIndex < 0 || SlotIndex >= Save->UnlockedSharedSocketSlots || !Item || !Id.IsValid()
		|| Item->ItemType != knightGameplayTags::Item_Type_Socket || !FMath::IsFinite(Item->EffectValue)
		|| Item->EffectValue < 0.f || Save->ItemCounts.FindRef(Id) <= 0) return false;
	const FPrimaryAssetId Previous = Save->EquippedSocketItems.IsValidIndex(SlotIndex)
		? Save->EquippedSocketItems[SlotIndex] : FPrimaryAssetId();
	if (Previous == Id) return false;
	int32 OtherUses = 0;
	for (int32 Index = 0; Index < Save->UnlockedSharedSocketSlots && Index < Save->EquippedSocketItems.Num(); ++Index)
		if (Index != SlotIndex && Save->EquippedSocketItems[Index] == Id) ++OtherUses;
	if ((!Item->bAllowDuplicateEquip && OtherUses > 0) || OtherUses >= Save->ItemCounts.FindRef(Id)) return false;
	Save->EquippedSocketItems.SetNum(Save->UnlockedSharedSocketSlots);
	Save->EquippedSocketItems[SlotIndex] = Id;
	CommitChange();
	return true;
}

bool UknightProgressionSubsystem::UnequipSocketItem(int32 SlotIndex)
{
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->CanStartAtomicTransaction() ? GetSave() : nullptr;
	if (!Save || SlotIndex < 0 || SlotIndex >= Save->UnlockedSharedSocketSlots
		|| !Save->EquippedSocketItems.IsValidIndex(SlotIndex)
		|| !Save->EquippedSocketItems[SlotIndex].IsValid()) return false;
	Save->EquippedSocketItems[SlotIndex] = FPrimaryAssetId();
	CommitChange();
	return true;
}

float UknightProgressionSubsystem::GetSocketEffectTotal(EKnightSocketEffect Effect) const
{
	float Total = 0.f;
	for (int32 Index = 0, Count = GetUnlockedSharedSocketSlots(); Index < Count; ++Index)
	{
		const UknightSocketItemDefinition* const Item = GetEquippedSocketItem(Index);
		if (Item && Item->Effect == Effect && FMath::IsFinite(Item->EffectValue) && Item->EffectValue >= 0.f)
			Total += Item->EffectValue;
	}
	return FMath::IsFinite(Total) ? Total : 0.f;
}

int32 UknightProgressionSubsystem::GetSocketEffectCount(EKnightSocketEffect Effect) const
{
	int32 Total = 0;
	for (int32 Index = 0, Count = GetUnlockedSharedSocketSlots(); Index < Count; ++Index)
	{
		const UknightSocketItemDefinition* const Item = GetEquippedSocketItem(Index);
		Total += Item && Item->Effect == Effect ? 1 : 0;
	}
	return Total;
}

int32 UknightProgressionSubsystem::GetRepairKitCapacityBonus() const
{
	return FMath::Max(0, FMath::RoundToInt(GetSocketEffectTotal(EKnightSocketEffect::RepairKitCapacity)));
}

float UknightProgressionSubsystem::GetAttackStaminaCostMultiplier() const
{
	return 1.f - FMath::Clamp(GetSocketEffectTotal(EKnightSocketEffect::AttackStaminaCost), 0.f, 1.f);
}

float UknightProgressionSubsystem::GetSocketGaugeChargeMultiplier() const
{
	return 1.f + FMath::Max(0.f, GetSocketEffectTotal(EKnightSocketEffect::SocketGaugeCharge));
}

bool UknightProgressionSubsystem::HasFirstDefeatedBoss(FName BossId) const
{
	const UknightSaveGame* const Save = GetSave();
	return Save && !BossId.IsNone() && Save->FirstDefeatedBossIds.Contains(BossId);
}

void UknightProgressionSubsystem::HandleRaidResult(const FknightRaidResult& IncomingResult)
{
	FknightRaidResult Result = IncomingResult;
	Result.StageId = KnightProgressionIds::Stage(Result.StageId);
	Result.QuestContext.StageId = KnightProgressionIds::Stage(Result.QuestContext.StageId);
	Result.QuestContext.QuestId = KnightProgressionIds::Quest(Result.QuestContext.QuestId);
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? Saves->GetSaveGame() : nullptr;
	if (!Save || !Result.RunId.IsValid() || Save->AppliedRaidRuns.Contains(Result.RunId)) return;

	const bool bQuestRun = !Result.QuestContext.QuestId.IsNone();
	const UKnightQuestDefinition* Quest = nullptr;
	if (bQuestRun)
	{
		const FKnightQuestRunContext& Context = Result.QuestContext;
		const UKnightQuestCatalog* const Catalog = UKnightQuestSettings::LoadCatalog();
		Quest = Catalog ? Catalog->FindQuest(Context.QuestId) : nullptr;
		FText DataReason;
		if (!Context.IsValid() || Context.RunId != Result.RunId || Context.StageId != Result.StageId
			|| Context.SlotId != Saves->GetActiveSlotId() || !Quest || !Quest->bContentEnabled
			|| !Quest->HasValidConfiguration(DataReason) || KnightProgressionIds::Stage(Quest->StageId) != Context.StageId
			|| Quest->BossId != Context.BossId || Quest->Difficulty != Context.Difficulty)
		{
			UE_LOG(Logknight, Error, TEXT("Quest settlement rejected run=%s quest=%s slot=%s activeSlot=%s: %s"),
				*Result.RunId.ToString(), *Context.QuestId.ToString(), *Context.SlotId,
				*Saves->GetActiveSlotId(), *DataReason.ToString());
			return;
		}
	}
	if (Result.bSuccess && !Result.StageId.IsNone()
		&& Save->StageRecords.FindRef(Result.StageId).ClearCount == MAX_int32)
	{
		UE_LOG(Logknight, Error, TEXT("Raid settlement rejected: clear count overflow for %s."), *Result.StageId.ToString());
		return;
	}

	// Plan the whole grant before touching the save. Quest runs ignore the raid definition's legacy
	// reward list; direct-stage runs keep that existing list. Every quest line rolls independently.
	TMap<FPrimaryAssetId, int64> Counts;
	TMap<FPrimaryAssetId, const UknightItemDefinition*> Definitions;
	auto AddPlannedItem = [&Counts, &Definitions](const UknightItemDefinition* Item, int32 Count)
	{
		const FPrimaryAssetId Id = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
		if (!Id.IsValid() || Count <= 0) return false;
		int64& Total = Counts.FindOrAdd(Id);
		if (Total > MAX_int32 - Count) return false;
		Total += Count;
		Definitions.FindOrAdd(Id) = Item;
		return true;
	};
	auto RollLine = [&AddPlannedItem](const FknightRaidReward& Line)
	{
		if (Line.Chance <= 0.f || FMath::FRand() >= Line.Chance) return true;
		return AddPlannedItem(Line.Item, FMath::RandRange(Line.MinCount, Line.MaxCount));
	};
	if (Result.bSuccess)
	{
		if (Quest)
		{
			for (const FknightRaidReward& Line : Quest->ClearRewards)
			{
				if (!RollLine(Line)) return;
			}
			for (const FKnightQuestPartBreakReward& Entry : Quest->PartBreakRewards)
			{
				if (Result.BrokenParts.HasTagExact(Entry.Reward.RequiredBrokenPart) && !RollLine(Entry.Reward)) return;
			}
		}
		else
		{
			for (const FknightItemStack& Stack : Result.Rewards)
			{
				if (!AddPlannedItem(Stack.Item, Stack.Count)) return;
			}
		}
	}
	for (const TPair<FPrimaryAssetId, int64>& Entry : Counts)
	{
		const int32 Owned = Save->ItemCounts.FindRef(Entry.Key);
		if (Owned < 0 || Entry.Value > MAX_int32 - Owned)
		{
			UE_LOG(Logknight, Error, TEXT("Raid settlement rejected: item count overflow for %s."), *Entry.Key.ToString());
			return;
		}
	}
	const int64 MoneyAward = Result.bSuccess && Quest ? Quest->MoneyReward : 0;
	const int32 SocketAward = Result.bSuccess && Quest && !Save->FirstDefeatedBossIds.Contains(Quest->BossId)
		? Quest->FirstBossSocketSlots : 0;
	if (MoneyAward < 0 || Save->Money > MAX_int64 - MoneyAward
		|| SocketAward < 0 || Save->UnlockedSharedSocketSlots > MAX_int32 - SocketAward)
	{
		UE_LOG(Logknight, Error, TEXT("Raid settlement rejected: money or shared socket overflow."));
		return;
	}

	FknightRaidResult Settled = Result;
	Settled.Rewards.Reset();
	for (const TPair<FPrimaryAssetId, int64>& Entry : Counts)
	{
		Settled.Rewards.Emplace(Definitions.FindChecked(Entry.Key), static_cast<int32>(Entry.Value));
	}
	Settled.MoneyEarned = MoneyAward;
	Settled.FirstBossSocketSlotsEarned = SocketAward;
	for (const TPair<FPrimaryAssetId, int64>& Entry : Counts)
	{
		Save->ItemCounts.FindOrAdd(Entry.Key) += static_cast<int32>(Entry.Value);
	}
	Save->Money += MoneyAward;
	if (Result.bSuccess && Quest)
	{
		// Share RecordQuestCompleted's idempotent record step, but defer its save/notification
		// until the entire raid grant and RunId are in the same snapshot.
		RecordQuestCompletionInSave(*Save, Quest->QuestId);
		if (!Save->FirstDefeatedBossIds.Contains(Quest->BossId))
		{
			Save->FirstDefeatedBossIds.Add(Quest->BossId);
			Save->UnlockedSharedSocketSlots += SocketAward;
		}
	}
	if (Result.bSuccess && !Result.StageId.IsNone())
	{
		FknightStageRecord& Record = Save->StageRecords.FindOrAdd(Result.StageId);
		++Record.ClearCount;
		if (Record.BestClearTime <= 0.f || Result.ElapsedTime < Record.BestClearTime)
		{
			Record.BestClearTime = Result.ElapsedTime;
		}
	}
	Save->AppliedRaidRuns.Add(Result.RunId);
	if (UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
	{
		Raid->SetSettledResult(Settled);
	}
	UE_LOG(Logknight, Log, TEXT("Raid settled run=%s quest=%s slot=%s items=%d money=%lld sockets=%d"),
		*Result.RunId.ToString(), Quest ? *Quest->QuestId.ToString() : TEXT("None"),
		*Saves->GetActiveSlotId(), Settled.Rewards.Num(), MoneyAward, SocketAward);
	CommitChange();
}

bool UknightProgressionSubsystem::IsStageUnlocked(FName StageId) const
{
	StageId = KnightProgressionIds::Stage(StageId);
	const UknightSaveGame* const Save = GetSave();
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FKnightStageDefinition Stage;
	if (!Save || !Flow || !Flow->FindStageDefinition(StageId, Stage)) return false;
	if (Stage.RaidDefinition.IsNull()) return true; // Existing flow-test entries remain available.
	const UknightRaidDefinition* const Definition = Stage.RaidDefinition.LoadSynchronous();
	if (!Definition || (!Definition->RequiredCompletedQuestId.IsNone()
		&& !IsQuestCompleted(Definition->RequiredCompletedQuestId))) return false;
	if (Save->UnlockedStages.Contains(StageId)) return true;
	return Definition->RequiredClearedStageId.IsNone()
		|| IsStageCleared(Definition->RequiredClearedStageId);
}

bool UknightProgressionSubsystem::UnlockStage(FName StageId)
{
	StageId = KnightProgressionIds::Stage(StageId);
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
	StageId = KnightProgressionIds::Stage(StageId);
	const UknightSaveGame* const Save = GetSave();
	const FknightStageRecord* const Record = Save ? Save->StageRecords.Find(StageId) : nullptr;
	return Record && Record->ClearCount > 0;
}

float UknightProgressionSubsystem::GetBestClearTime(FName StageId) const
{
	StageId = KnightProgressionIds::Stage(StageId);
	const UknightSaveGame* const Save = GetSave();
	const FknightStageRecord* const Record = Save ? Save->StageRecords.Find(StageId) : nullptr;
	return Record ? Record->BestClearTime : 0.f;
}

int32 UknightProgressionSubsystem::GetItemCount(const UknightItemDefinition* Item) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Count = Save && Item ? Save->ItemCounts.Find(Item->GetPrimaryAssetId()) : nullptr;
	const bool bImplicitWeapon = Item && Item->IsA<UknightWeaponItemDefinition>() && Item == GetImplicitOwnedWeapon(Save);
	return FMath::Max(Count ? *Count : 0, bImplicitWeapon ? 1 : 0);
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
	if (Item->IsA<UknightSocketItemDefinition>())
	{
		int32 Equipped = 0;
		for (int32 Index = 0; Index < Save->UnlockedSharedSocketSlots && Index < Save->EquippedSocketItems.Num(); ++Index)
			if (Save->EquippedSocketItems[Index] == ItemId) ++Equipped;
		if (*Owned - Count < Equipped) return false;
	}
	*Owned -= Count;
	if (*Owned == 0)
	{
		Save->ItemCounts.Remove(ItemId);
	}
	CommitChange();
	return true;
}

bool UknightProgressionSubsystem::GetMaterialSaleQuote(FPrimaryAssetId ItemId, int32 Count,
	FKnightMaterialSaleReceipt& OutReceipt) const
{
	OutReceipt = FKnightMaterialSaleReceipt();
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->CanStartAtomicTransaction() ? Saves->GetSaveGame() : nullptr;
	if (!Save || !ItemId.IsValid() || Count <= 0) return false;
	const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(
		UknightInventoryLibrary::LoadItemDefinition(ItemId));
	if (!Material || Material->GetPrimaryAssetId() != ItemId
		|| Material->ItemType != knightGameplayTags::Item_Type_Material
		|| !Material->bSellable || Material->SellPrice <= 0
		|| Save->ItemCounts.FindRef(ItemId) < Count) return false;
	const int64 Price = Material->SellPrice;
	if (Price > MAX_int64 / Count) return false;
	const int64 Total = Price * Count;
	if (Save->Money < 0 || Save->Money > MAX_int64 - Total) return false;
	OutReceipt.ItemId = ItemId;
	OutReceipt.Count = Count;
	OutReceipt.UnitPrice = Material->SellPrice;
	OutReceipt.TotalMoney = Total;
	return true;
}

bool UknightProgressionSubsystem::SellMaterial(FPrimaryAssetId ItemId, int32 Count,
	FKnightMaterialSaleReceipt& OutReceipt)
{
	if (!GetMaterialSaleQuote(ItemId, Count, OutReceipt)) return false;
	UknightSaveGame* const Save = GetSave();
	int32& Owned = Save->ItemCounts.FindChecked(ItemId);
	Owned -= Count;
	if (Owned == 0) Save->ItemCounts.Remove(ItemId);
	Save->Money += OutReceipt.TotalMoney;
	// RequestSave snapshots both changes before OnProgressionChanged refreshes the inventory.
	CommitChange();
	return true;
}

void UknightProgressionSubsystem::GetOwnedItems(TArray<FknightItemStack>& OutItems) const
{
	ResolveOwnedDefinitions(GetSave(), OutItems);
}

bool UknightProgressionSubsystem::GetOwnedMaterials(TArray<FknightItemStack>& OutMaterials) const
{
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const bool bComplete = ResolveOwnedCategory<UknightMaterialItemDefinition>(
		Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr, knightGameplayTags::Item_Type_Material, OutMaterials);
	OutMaterials.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		if (A.Item->Rarity != B.Item->Rarity) return A.Item->Rarity > B.Item->Rarity;
		return A.Item->GetPrimaryAssetId().ToString() < B.Item->GetPrimaryAssetId().ToString();
	});
	return bComplete;
}

bool UknightProgressionSubsystem::GetOwnedWeapons(TArray<FknightItemStack>& OutWeapons) const
{
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const bool bComplete = ResolveOwnedCategory<UknightWeaponItemDefinition>(
		Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr, knightGameplayTags::Item_Type_Weapon, OutWeapons);
	OutWeapons.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		return A.Item->GetPrimaryAssetId().ToString() < B.Item->GetPrimaryAssetId().ToString();
	});
	return bComplete;
}

bool UknightProgressionSubsystem::GetOwnedArmorForSlot(FGameplayTag SlotTag, TArray<FknightItemStack>& OutArmor) const
{
	OutArmor.Reset();
	if (SlotTag != knightGameplayTags::Equipment_Slot_Head && SlotTag != knightGameplayTags::Equipment_Slot_Torso
		&& SlotTag != knightGameplayTags::Equipment_Slot_Arms && SlotTag != knightGameplayTags::Equipment_Slot_Legs)
		return false;
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	TArray<FknightItemStack> All;
	bool bComplete = ResolveOwnedDefinitions(Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr, All);
	for (const FknightItemStack& Stack : All)
	{
		const UknightPartItemDefinition* Part = Cast<UknightPartItemDefinition>(Stack.Item);
		if (!Part || Part->SlotTag != SlotTag) continue;
		if (Part->ItemType == knightGameplayTags::Item_Type_Part && !Part->IsA<UknightWeaponItemDefinition>()) OutArmor.Add(Stack);
		else
		{
			bComplete = false;
			UE_LOG(Logknight, Warning, TEXT("Owned armor slot/type mismatch: %s"), *Part->GetPrimaryAssetId().ToString());
		}
	}
	OutArmor.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		return A.Item->GetPrimaryAssetId().ToString() < B.Item->GetPrimaryAssetId().ToString();
	});
	return bComplete;
}

bool UknightProgressionSubsystem::GetOwnedSockets(TArray<FknightItemStack>& OutSockets) const
{
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const bool bComplete = ResolveOwnedCategory<UknightSocketItemDefinition>(
		Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr, knightGameplayTags::Item_Type_Socket, OutSockets);
	OutSockets.Sort([](const FknightItemStack& A, const FknightItemStack& B)
	{
		return A.Item->GetPrimaryAssetId().ToString() < B.Item->GetPrimaryAssetId().ToString();
	});
	return bComplete;
}

bool UknightProgressionSubsystem::GetSocketAssignments(TArray<FKnightSocketAssignment>& OutAssignments) const
{
	OutAssignments.Reset();
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save) return false;
	bool bComplete = true;
	for (int32 Index = 0; Index < Save->UnlockedSharedSocketSlots; ++Index)
	{
		FKnightSocketAssignment& Entry = OutAssignments.AddDefaulted_GetRef();
		Entry.SlotIndex = Index;
		if (!Save->EquippedSocketItems.IsValidIndex(Index)) continue;
		Entry.ItemId = Save->EquippedSocketItems[Index];
		if (!Entry.ItemId.IsValid()) continue;
		Entry.Item = GetEquippedSocketItem(Index);
		if (!Entry.Item) bComplete = false;
	}
	return bComplete;
}

bool UknightProgressionSubsystem::GetMainEquipmentSlots(TArray<FKnightMainEquipmentSlot>& OutSlots) const
{
	OutSlots.Reset();
	const UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save) return false;
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	const knightEquipmentStats::FLoadout Loadout = knightEquipmentStats::ReadLoadout(this, Catalog);
	bool bComplete = Catalog != nullptr;
	const FGameplayTag Slots[] = { knightGameplayTags::Equipment_Slot_Weapon, knightGameplayTags::Equipment_Slot_Head,
		knightGameplayTags::Equipment_Slot_Torso, knightGameplayTags::Equipment_Slot_Arms,
		knightGameplayTags::Equipment_Slot_Legs };
	for (const FGameplayTag Slot : Slots)
	{
		FKnightMainEquipmentSlot& Entry = OutSlots.AddDefaulted_GetRef();
		Entry.SlotTag = Slot;
		Entry.Part = Loadout.FindRef(Slot);
		const FPrimaryAssetId* SavedId = Save->EquippedItems.Find(Slot);
		if (SavedId && (!Entry.Part || Entry.Part->GetPrimaryAssetId() != *SavedId))
		{
			Entry.Part = nullptr;
			bComplete = false;
		}
		if (Entry.Part && Entry.Part->SlotTag != Slot)
		{
			Entry.Part = nullptr;
			bComplete = false;
		}
	}
	return bComplete;
}

int32 UknightProgressionSubsystem::GetUpgradeLevel(const UknightItemDefinition* Item) const
{
	const UknightSaveGame* const Save = GetSave();
	const int32* const Level = Save && Item ? Save->ItemUpgradeLevels.Find(Item->GetPrimaryAssetId()) : nullptr;
	return Level ? *Level : 0;
}

int32 UknightProgressionSubsystem::GetWeaponUpgradeFailures(const UknightWeaponItemDefinition* Weapon) const
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	return Save && Weapon ? Save->WeaponUpgradeFailures.FindRef(Weapon->GetPrimaryAssetId()) : 0;
}

void UknightProgressionSubsystem::GetWeaponUpgradeQuote(const UknightWeaponItemDefinition* Weapon,
	FKnightWeaponUpgradeQuote& OutQuote) const
{
	OutQuote = FKnightWeaponUpgradeQuote();
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? GetSave() : nullptr;
	if (!Save) { OutQuote.Availability = EKnightWeaponUpgradeAvailability::NoSlot; return; }
	if (!Weapon || Weapon->ItemType != knightGameplayTags::Item_Type_Weapon
		|| Weapon->SlotTag != knightGameplayTags::Equipment_Slot_Weapon
		|| !Weapon->GetPrimaryAssetId().IsValid()) return;
	OutQuote.WeaponId = Weapon->GetPrimaryAssetId();
	if (GetItemCount(Weapon) <= 0)
	{ OutQuote.Availability = EKnightWeaponUpgradeAvailability::NotOwned; return; }
	const int32 Level = Save->ItemUpgradeLevels.FindRef(OutQuote.WeaponId);
	if (Level < 0 || Level > 15 || Weapon->UpgradeSteps.Num() != 15) return;
	OutQuote.CurrentLevel = Level;
	OutQuote.CurrentWeaponAttack = Weapon->GetWeaponAttackAtLevel(Level);
	OutQuote.NextWeaponAttack = OutQuote.CurrentWeaponAttack;
	if (Level == 15)
	{ OutQuote.Availability = EKnightWeaponUpgradeAvailability::MaxLevel; return; }
	const FknightUpgradeStep& Step = Weapon->UpgradeSteps[Level];
	if (Step.TargetLevel != Level + 1 || Step.MoneyCost <= 0
		|| !FMath::IsFinite(Step.BaseSuccessChance) || Step.BaseSuccessChance < 0.f || Step.BaseSuccessChance > 1.f
		|| !FMath::IsFinite(Step.FailureBonusPerAttempt) || Step.FailureBonusPerAttempt < 0.f || Step.FailureBonusPerAttempt > 1.f
		|| !FMath::IsFinite(Step.AttackIncrease) || Step.AttackIncrease < 0.f
		|| ((Level == 9 || Level == 14) ? Step.Costs.Num() != 1 : !Step.Costs.IsEmpty())) return;
	OutQuote.TargetLevel = Level + 1;
	OutQuote.MoneyCost = Step.MoneyCost;
	OutQuote.BaseSuccessChance = Step.BaseSuccessChance;
	const int32 Failures = FMath::Max(0, Save->WeaponUpgradeFailures.FindRef(OutQuote.WeaponId));
	OutQuote.EffectiveSuccessChance = FMath::Clamp(
		static_cast<double>(Step.BaseSuccessChance) + static_cast<double>(Failures) * Step.FailureBonusPerAttempt, 0.0, 1.0);
	OutQuote.NextWeaponAttack = Weapon->GetWeaponAttackAtLevel(Level + 1);
	OutQuote.NextWeaponAttackIncrease = OutQuote.NextWeaponAttack - OutQuote.CurrentWeaponAttack;
	if (!Step.Costs.IsEmpty())
	{
		const FknightItemCost& Cost = Step.Costs[0];
		const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(Cost.Item.Get());
		if (!Material || Material->ItemType != knightGameplayTags::Item_Type_Material || Cost.Count != 1
			|| !Material->GetPrimaryAssetId().IsValid()) return;
		OutQuote.GateMaterial = Material;
		OutQuote.GateMaterialNeeded = Cost.Count;
		OutQuote.GateMaterialOwned = Save->ItemCounts.FindRef(Material->GetPrimaryAssetId());
	}
	if (!Saves->CanStartAtomicTransaction()) OutQuote.Availability = EKnightWeaponUpgradeAvailability::SaveBlocked;
	else if (Save->Money < Step.MoneyCost) OutQuote.Availability = EKnightWeaponUpgradeAvailability::InsufficientMoney;
	else if (OutQuote.GateMaterial && OutQuote.GateMaterialOwned < OutQuote.GateMaterialNeeded)
		OutQuote.Availability = EKnightWeaponUpgradeAvailability::MissingMaterial;
	else OutQuote.Availability = EKnightWeaponUpgradeAvailability::Ready;
}

EKnightWeaponUpgradeOutcome UknightProgressionSubsystem::TryUpgradeWeapon(
	const UknightWeaponItemDefinition* Weapon, FKnightWeaponUpgradeReceipt& OutReceipt)
{
	OutReceipt = FKnightWeaponUpgradeReceipt();
	FKnightWeaponUpgradeQuote Quote;
	GetWeaponUpgradeQuote(Weapon, Quote);
	if (Quote.Availability != EKnightWeaponUpgradeAvailability::Ready) return OutReceipt.Outcome;
	UknightSaveGame* const Save = GetSave();
	const int32 Failures = Save->WeaponUpgradeFailures.FindRef(Quote.WeaponId);
	bool bSuccess = FMath::FRand() < Quote.EffectiveSuccessChance;
#if WITH_EDITOR
	if (DebugNextWeaponUpgradeOutcome >= 0)
	{
		bSuccess = DebugNextWeaponUpgradeOutcome == 1;
		DebugNextWeaponUpgradeOutcome = -1;
	}
#endif
	Save->Money -= Quote.MoneyCost;
	OutReceipt.WeaponId = Quote.WeaponId;
	OutReceipt.PreviousLevel = Quote.CurrentLevel;
	OutReceipt.NewLevel = Quote.CurrentLevel;
	OutReceipt.MoneySpent = Quote.MoneyCost;
	if (bSuccess)
	{
		Save->ItemUpgradeLevels.Add(Quote.WeaponId, Quote.TargetLevel);
		Save->WeaponUpgradeFailures.Remove(Quote.WeaponId);
		OutReceipt.NewLevel = Quote.TargetLevel;
		OutReceipt.Outcome = EKnightWeaponUpgradeOutcome::Success;
		if (Quote.GateMaterial)
		{
			const FPrimaryAssetId MaterialId = Quote.GateMaterial->GetPrimaryAssetId();
			int32& Owned = Save->ItemCounts.FindChecked(MaterialId);
			Owned -= Quote.GateMaterialNeeded;
			if (Owned == 0) Save->ItemCounts.Remove(MaterialId);
			OutReceipt.MaterialConsumed = Quote.GateMaterialNeeded;
		}
		if (APlayerController* const PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			if (APawn* const Pawn = PC->GetPawn())
				if (UknightPartsComponent* const Parts = Pawn->FindComponentByClass<UknightPartsComponent>())
					Parts->RefreshWeaponUpgrade(Weapon, Quote.TargetLevel);
	}
	else
	{
		Save->WeaponUpgradeFailures.Add(Quote.WeaponId, Failures < MAX_int32 ? Failures + 1 : Failures);
		OutReceipt.Outcome = EKnightWeaponUpgradeOutcome::ChanceFailed;
	}
	CommitChange();
	return OutReceipt.Outcome;
}

bool UknightProgressionSubsystem::CanUpgradePart(const UknightPartItemDefinition* Part) const
{
	if (!Part || Part->IsA<UknightWeaponItemDefinition>() || GetItemCount(Part) <= 0)
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
		|| (!Recipe->RequiredClearedStageId.IsNone() && !IsStageCleared(Recipe->RequiredClearedStageId)))
	{
		return false;
	}
	// Equipment is unique per save slot. A second craft must never spend materials.
	if (GetItemCount(Recipe->Result) > 0) return false;
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
	return true;
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
		{
			int32& Count = Save->ItemCounts.FindOrAdd(Starter->GetPrimaryAssetId());
			Count = FMath::Max(Count, 1);
		}
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
			{
				int32& Count = Save->ItemCounts.FindOrAdd(Starter->GetPrimaryAssetId());
				Count = FMath::Max(Count, 1);
			}
		}
		Save->ExplicitlyEmptySlots.Add(SlotTag);
		CommitChange();
	}
}
