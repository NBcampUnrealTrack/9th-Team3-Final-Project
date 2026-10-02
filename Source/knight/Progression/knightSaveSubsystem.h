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

	/** Null when the selected slot exists but cannot be safely loaded. */
	UFUNCTION(BlueprintPure, Category="Knight|Save")
	UknightSaveGame* GetSaveGame() const { return SaveGame; }

	/** Use before gameplay starts. IDs contain letters, digits, underscores or hyphens. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	bool SelectSlot(const FString& SlotId);

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	FString GetActiveSlotId() const { return ActiveSlotId; }

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	bool DidLoadFail() const { return bLoadFailed; }

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
	bool LoadSlot(const FString& Slot);
	bool MigrateSave(UknightSaveGame& Save) const;
	void StartAsyncSave();
	void HandleAsyncSaveFinished(const FString& Slot, int32 User, bool bSuccess);

	UPROPERTY(Transient)
	TObjectPtr<UknightSaveGame> SaveGame;

	/** Holds the immutable serialized version until completion. */
	UPROPERTY(Transient)
	TObjectPtr<UknightSaveGame> SaveSnapshot;

	FString ActiveSlotId = TEXT("Default");
	FString ActiveSlotName = SlotName;
	bool bLoadFailed = false;

	bool bSaveInFlight = false;
	bool bSavePending = false;
};
