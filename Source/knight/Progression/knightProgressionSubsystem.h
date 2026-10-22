#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Item/knightItemDefinitions.h"
#include "Raid/knightRaidTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "knightProgressionSubsystem.generated.h"

class UknightItemDefinition;
class UknightPartItemDefinition;
class UknightSaveGame;
class UknightCraftRecipeDefinition;
class UknightWeaponItemDefinition;
class UknightMaterialItemDefinition;
class UknightSocketItemDefinition;
enum class EKnightSocketEffect : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FknightProgressionChanged);

/** Receipt for one fully validated material sale. It is display data, not another inventory. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightMaterialSaleReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Sale") FPrimaryAssetId ItemId;
	UPROPERTY(BlueprintReadOnly, Category="Sale") int32 Count = 0;
	UPROPERTY(BlueprintReadOnly, Category="Sale") int32 UnitPrice = 0;
	UPROPERTY(BlueprintReadOnly, Category="Sale") int64 TotalMoney = 0;
};

UENUM(BlueprintType)
enum class EKnightWeaponUpgradeAvailability : uint8
{
	Ready, NoSlot, NotOwned, InvalidSettings, MaxLevel, InsufficientMoney, MissingMaterial, SaveBlocked
};

USTRUCT(BlueprintType)
struct KNIGHT_API FKnightWeaponUpgradeQuote
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") EKnightWeaponUpgradeAvailability Availability = EKnightWeaponUpgradeAvailability::InvalidSettings;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") FPrimaryAssetId WeaponId;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 CurrentLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 TargetLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int64 MoneyCost = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") float BaseSuccessChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") float CurrentWeaponAttack = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") float NextWeaponAttack = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") TObjectPtr<const UknightMaterialItemDefinition> GateMaterial;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 GateMaterialNeeded = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 GateMaterialOwned = 0;
};

UENUM(BlueprintType)
enum class EKnightWeaponUpgradeOutcome : uint8 { Rejected, Success, ChanceFailed };

USTRUCT(BlueprintType)
struct KNIGHT_API FKnightWeaponUpgradeReceipt
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") EKnightWeaponUpgradeOutcome Outcome = EKnightWeaponUpgradeOutcome::Rejected;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") FPrimaryAssetId WeaponId;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 PreviousLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 NewLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int64 MoneySpent = 0;
	UPROPERTY(BlueprintReadOnly, Category="Upgrade") int32 MaterialConsumed = 0;
};

/**
 * Gameplay-facing progress API over UknightSaveSubsystem: stage clears, the item box, upgrades and the
 * equipment loadout. Raid results (clear records and rewards) are applied automatically from
 * UknightRaidSubsystem. Every change requests a save.
 */
UCLASS()
class KNIGHT_API UknightProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Stages
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	bool IsStageCleared(FName StageId) const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	bool IsStageUnlocked(FName StageId) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool UnlockStage(FName StageId);

	/** Call only after the quest system has confirmed completion. False for empty IDs, no selected save, or duplicates. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Quest")
	bool RecordQuestCompleted(FName QuestId);

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Quest")
	bool IsQuestCompleted(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Rewards")
	int64 GetMoney() const;

	/** Credit positive currency, with overflow protection. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Rewards")
	bool AddMoney(int64 Amount);

	/** Spend only when the full amount is available. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Rewards")
	bool SpendMoney(int64 Amount);

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Rewards")
	int32 GetUnlockedSharedSocketSlots() const;

	/** Null for an empty or locked slot. Uses saved item IDs; never grants or consumes inventory. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Socket")
	const UknightSocketItemDefinition* GetEquippedSocketItem(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	int32 GetAvailableSocketItemCount(const UknightSocketItemDefinition* Item) const;

	/** Replaces one slot atomically after checking unlock, ownership, duplicates and save readiness. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Socket")
	bool EquipSocketItem(int32 SlotIndex, const UknightSocketItemDefinition* Item);

	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Socket")
	bool UnequipSocketItem(int32 SlotIndex);

	/** Values are always recomputed from the equipped IDs; no accumulated modifier state is saved. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	float GetSocketEffectTotal(EKnightSocketEffect Effect) const;

	/** (2026-10-22) 이 효과를 가진 소켓을 몇 칸에 끼웠는지. 충전 보조(퍼펙트 가드 필요 횟수 -1)처럼 값 대신 개수로 쓰는 효과용. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	int32 GetSocketEffectCount(EKnightSocketEffect Effect) const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	int32 GetRepairKitCapacityBonus() const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	float GetAttackStaminaCostMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Socket")
	float GetSocketGaugeChargeMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression|Rewards")
	bool HasFirstDefeatedBoss(FName BossId) const;

	/** Zero if never cleared. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	float GetBestClearTime(FName StageId) const;

	// Item box
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	int32 GetItemCount(const UknightItemDefinition* Item) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	void AddItem(const UknightItemDefinition* Item, int32 Count);

	/** Removes Count if available. Returns false and changes nothing otherwise. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool ConsumeItem(const UknightItemDefinition* Item, int32 Count);

	/** Quotes a sale from the selected slot and the current item definition; never changes the save. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Sale")
	bool GetMaterialSaleQuote(FPrimaryAssetId ItemId, int32 Count, FKnightMaterialSaleReceipt& OutReceipt) const;

	/** Decrements one material and credits money in one save snapshot. Refuses overlapping requests. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Sale")
	bool SellMaterial(FPrimaryAssetId ItemId, int32 Count, FKnightMaterialSaleReceipt& OutReceipt);

	/** Every owned item with its definition loaded. Items whose asset no longer exists are skipped. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	void GetOwnedItems(TArray<FknightItemStack>& OutItems) const;

	// Upgrades
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	int32 GetUpgradeLevel(const UknightItemDefinition* Item) const;

	/** True if the part is owned, below its max level, and every material for the next step is in the box. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	bool CanUpgradePart(const UknightPartItemDefinition* Part) const;

	/** Spends the next step's materials and raises the level. Returns false and changes nothing if it cannot. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool UpgradePart(const UknightPartItemDefinition* Part);

	/** Read-only next-step quote. Failure correction is intentionally absent from player-facing data. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Weapon Upgrade")
	void GetWeaponUpgradeQuote(const UknightWeaponItemDefinition* Weapon, FKnightWeaponUpgradeQuote& OutQuote) const;

	/** One roll and one save snapshot; rejected requests consume neither money nor RNG. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression|Weapon Upgrade")
	EKnightWeaponUpgradeOutcome TryUpgradeWeapon(const UknightWeaponItemDefinition* Weapon, FKnightWeaponUpgradeReceipt& OutReceipt);

	/** Internal progression state, never shown as an effective chance in the maintenance UI. */
	int32 GetWeaponUpgradeFailures(const UknightWeaponItemDefinition* Weapon) const;

#if WITH_EDITOR
	void SetDebugNextWeaponUpgradeOutcome(int32 Outcome) { DebugNextWeaponUpgradeOutcome = FMath::Clamp(Outcome, -1, 1); }
#endif

	/** Validates ownership, ingredients and stage unlocks without changing the save. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	bool CanCraftPart(const UknightCraftRecipeDefinition* Recipe) const;

	/** Atomic single-save transaction. Never equips the result. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool CraftPart(const UknightCraftRecipeDefinition* Recipe);

	// Loadout
	/** Equipped part for a slot, or null. Loads the definition if needed. */
	const UknightPartItemDefinition* GetEquippedPart(FGameplayTag SlotTag) const;

	bool IsSlotExplicitlyEmpty(FGameplayTag SlotTag) const;

	/** Equips an owned part into its slot. Returns false if it is not owned. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool SetEquippedPart(const UknightPartItemDefinition* Part);

	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	void ClearEquippedSlot(UPARAM(meta=(Categories="Equipment.Slot")) FGameplayTag SlotTag);

	UPROPERTY(BlueprintAssignable, Category="Knight|Progression")
	FknightProgressionChanged OnProgressionChanged;

private:
	UFUNCTION()
	void HandleRaidResult(const FknightRaidResult& Result);

	UknightSaveGame* GetSave() const;
	void CommitChange();
#if WITH_EDITOR
	int32 DebugNextWeaponUpgradeOutcome = -1;
#endif
};
