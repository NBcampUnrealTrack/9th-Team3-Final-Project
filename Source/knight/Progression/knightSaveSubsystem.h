#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightSaveSubsystem.generated.h"

class USaveGame;
class UknightSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightSaveFinished, bool, bSuccess);

/**
 * Loads the progress slot at startup and saves it asynchronously. Saves requested while one is in flight are
 * coalesced into a single follow-up save, so gameplay can call RequestSave freely.
 * The slot lives in Saved/SaveGames; Steam Auto-Cloud can sync that folder.
 */
UCLASS()
class KNIGHT_API UknightSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Always valid after Initialize (a fresh save when none exists). */
	UFUNCTION(BlueprintPure, Category="Knight|Save")
	UknightSaveGame* GetSaveGame() const { return SaveGame; }

	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	void RequestSave();

	/** Deletes the slot and starts a fresh save. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	void ResetSave();

	UPROPERTY(BlueprintAssignable, Category="Knight|Save")
	FknightSaveFinished OnSaveFinished;

	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

private:
	void LoadOrCreate();
	void MigrateSave(UknightSaveGame& Save) const;
	void StartAsyncSave();
	void HandleAsyncSaveFinished(const FString& Slot, int32 User, bool bSuccess);

	UPROPERTY(Transient)
	TObjectPtr<UknightSaveGame> SaveGame;

	bool bSaveInFlight = false;
	bool bSavePending = false;
};
