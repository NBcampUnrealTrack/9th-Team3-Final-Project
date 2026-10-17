#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Raid/knightRaidTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightProgressionSubsystem.generated.h"

class UknightItemDefinition;
class UknightPartItemDefinition;
class UknightSaveGame;
class UknightCraftRecipeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FknightProgressionChanged);

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

	/** Validates all ingredients and stage unlocks without changing the save. */
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
};
