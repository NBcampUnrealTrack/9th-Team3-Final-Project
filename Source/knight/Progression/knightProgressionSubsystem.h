#pragma once

#include "CoreMinimal.h"
#include "Raid/knightRaidTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightProgressionSubsystem.generated.h"

class UknightSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FknightProgressionChanged);

/**
 * Gameplay-facing progress API over UknightSaveSubsystem: stage clears, materials and upgrade levels.
 * Raid results are recorded automatically from UknightRaidSubsystem. Every change requests a save.
 */
UCLASS()
class KNIGHT_API UknightProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	bool IsStageCleared(FName StageId) const;

	/** Zero if never cleared. */
	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	float GetBestClearTime(FName StageId) const;

	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	int32 GetMaterialCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	void AddMaterial(FName ItemId, int32 Count);

	/** Removes Count if available. Returns false and changes nothing otherwise. */
	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	bool ConsumeMaterial(FName ItemId, int32 Count);

	UFUNCTION(BlueprintPure, Category="Knight|Progression")
	int32 GetUpgradeLevel(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Progression")
	void SetUpgradeLevel(FName ItemId, int32 Level);

	UPROPERTY(BlueprintAssignable, Category="Knight|Progression")
	FknightProgressionChanged OnProgressionChanged;

private:
	UFUNCTION()
	void HandleRaidResult(const FknightRaidResult& Result);

	UknightSaveGame* GetSave() const;
	void CommitChange();
};
