#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "knightSaveGame.generated.h"

/** Per-stage record. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightStageRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Progression")
	int32 ClearCount = 0;

	/** Fastest clear in seconds; zero until the first clear. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Progression")
	float BestClearTime = 0.f;
};

/**
 * Game progress (one slot). Personal settings are not here: they live in UknightSharedSettings, and
 * hardware settings in UknightGameUserSettings.
 *
 * Items are stored by FPrimaryAssetId ("Item:AssetName") so saves survive asset moves and class changes.
 */
UCLASS()
class KNIGHT_API UknightSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * Bump when the layout changes, and handle the old version in UknightSaveSubsystem::MigrateSave.
	 * 1: items keyed by FName. 2: primary asset IDs and equipment. 3: explicit unlocks and applied run IDs.
	 * 5: confirmed quest IDs (older saves default to an empty set).
	 * 6: money and first-boss shared socket awards (older saves default to zero/empty).
	 */
	static constexpr int32 CurrentVersion = 6;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame)
	TMap<FName, FknightStageRecord> StageRecords;

	/** Explicit unlocks; stages without prerequisites are available by default. */
	UPROPERTY(SaveGame)
	TSet<FName> UnlockedStages;

	/** Written together with rewards, so a repeated result cannot pay twice after loading. */
	UPROPERTY(SaveGame)
	TSet<FGuid> AppliedRaidRuns;

	/** Confirmed quest completions for this save slot. Independent of stage clears and raid results. */
	UPROPERTY(SaveGame)
	TSet<FName> CompletedQuestIds;

	/** Slot currency for quest rewards, later sales and upgrades. */
	UPROPERTY(SaveGame)
	int64 Money = 0;

	/** One first-clear award per BossId across all difficulties. */
	UPROPERTY(SaveGame)
	TSet<FName> FirstDefeatedBossIds;

	UPROPERTY(SaveGame)
	int32 UnlockedSharedSocketSlots = 0;

	/** The item box: owned count per item. */
	UPROPERTY(SaveGame)
	TMap<FPrimaryAssetId, int32> ItemCounts;

	/** Upgrade level per part or weapon. */
	UPROPERTY(SaveGame)
	TMap<FPrimaryAssetId, int32> ItemUpgradeLevels;

	/** Equipped part per Equipment.Slot tag. */
	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FPrimaryAssetId> EquippedItems;

	/** Explicitly unequipped slots must not fall back to the character's starter loadout. */
	UPROPERTY(SaveGame)
	TSet<FGameplayTag> ExplicitlyEmptySlots;

	UPROPERTY(SaveGame)
	float TotalPlayTime = 0.f;

	/** Version 1 data, read only for migration. Names and types must not change. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> Materials;

	/** Version 1 data, read only for migration. Names and types must not change. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> UpgradeLevels;
};
