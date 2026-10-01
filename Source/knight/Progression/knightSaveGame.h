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
	 * 1: items keyed by FName. 2: items keyed by primary asset id, equipment loadout.
	 */
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame)
	TMap<FName, FknightStageRecord> StageRecords;

	/** The item box: owned count per item. */
	UPROPERTY(SaveGame)
	TMap<FPrimaryAssetId, int32> ItemCounts;

	/** Upgrade level per part or weapon. */
	UPROPERTY(SaveGame)
	TMap<FPrimaryAssetId, int32> ItemUpgradeLevels;

	/** Equipped part per Equipment.Slot tag. */
	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FPrimaryAssetId> EquippedItems;

	UPROPERTY(SaveGame)
	float TotalPlayTime = 0.f;

	/** Version 1 data, read only for migration. Names and types must not change. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> Materials;

	/** Version 1 data, read only for migration. Names and types must not change. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> UpgradeLevels;
};
