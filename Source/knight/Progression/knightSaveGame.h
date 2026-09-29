#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
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
 * Item ids are FNames for now; the item phase replaces them with primary asset ids of item definitions,
 * bumping SaveVersion and migrating in UknightSaveSubsystem.
 */
UCLASS()
class KNIGHT_API UknightSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bump when the layout changes, and handle the old version in UknightSaveSubsystem::MigrateSave. */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame)
	TMap<FName, FknightStageRecord> StageRecords;

	/** Monster materials and other stackables by item id. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> Materials;

	/** Upgrade level by part or weapon item id. */
	UPROPERTY(SaveGame)
	TMap<FName, int32> UpgradeLevels;

	UPROPERTY(SaveGame)
	float TotalPlayTime = 0.f;
};
