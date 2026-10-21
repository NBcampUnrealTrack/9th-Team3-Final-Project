#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "knightSaveSubsystem.generated.h"

class USaveGame;
class UknightSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightSaveFinished, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightActiveSlotChanged, const FString&, SlotId);

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
	virtual void Deinitialize() override;

	/** Null when the selected slot exists but cannot be safely loaded. */
	UFUNCTION(BlueprintPure, Category="Knight|Save")
	UknightSaveGame* GetSaveGame() const { return SaveGame; }

	/** Use before gameplay starts. IDs contain letters, digits, underscores or hyphens. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	bool SelectSlot(const FString& SlotId);

	/** Presence only; a damaged slot may still report true and must be selected to validate it. */
	UFUNCTION(BlueprintPure, Category="Knight|Save")
	bool DoesSlotExist(const FString& SlotId) const;

	/** Fixed title slots: 1 = legacy Default, 2 = Slot2, 3 = Slot3. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	bool SelectFixedSlot(int32 SlotNumber);

	/** Reads a disk snapshot without changing the active profile. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	bool GetFixedSlotInfo(int32 SlotNumber, bool& bExists, bool& bReadable, float& PlayTimeSeconds) const;

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	int32 GetActiveFixedSlotNumber() const;

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	bool IsSaveInFlight() const { return bSaveInFlight; }

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	FString GetActiveSlotId() const { return ActiveSlotId; }

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	bool HasSelectedSlot() const { return bSlotSelected && !bLoadFailed && SaveGame != nullptr; }

	UFUNCTION(BlueprintPure, Category="Knight|Save")
	bool DidLoadFail() const { return bLoadFailed; }

	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	void RequestSave();

	/** Deletes the slot and starts a fresh save. */
	UFUNCTION(BlueprintCallable, Category="Knight|Save")
	void ResetSave();

	UPROPERTY(BlueprintAssignable, Category="Knight|Save")
	FknightSaveFinished OnSaveFinished;

	/** Fired after a slot selection attempt, including an unreadable slot, so views can clear stale progress. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Save")
	FknightActiveSlotChanged OnActiveSlotChanged;

	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

#if WITH_EDITOR
	/** Test hook: the next N saves complete as failures without writing a file. */
	void DebugFailNextSaves(int32 Count) { DebugFailuresRemaining = FMath::Max(0, Count); }
#endif

private:
	bool LoadSlot(const FString& Slot);
	UknightSaveGame* ReadExistingSlot(const FString& Slot) const;
	bool MigrateSave(UknightSaveGame& Save) const;
	static bool MakeSlotName(const FString& SlotId, FString& OutSlotName);
	static FString FixedSlotId(int32 SlotNumber);
	bool TickPlayTime(float DeltaTime);
	void AccumulatePlayTime();
	bool CanCountPlayTime() const;
	void StartAsyncSave();
	void LaunchSaveAttempt();
	bool RetryFailedSave(float DeltaTime);
	void HandleAsyncSaveFinished(const FString& Slot, int32 User, bool bSuccess);

	UPROPERTY(Transient)
	TObjectPtr<UknightSaveGame> SaveGame;

	/** Holds the immutable serialized version until completion. */
	UPROPERTY(Transient)
	TObjectPtr<UknightSaveGame> SaveSnapshot;

	FString ActiveSlotId = TEXT("Default");
	FString ActiveSlotName = SlotName;
	bool bLoadFailed = false;
	bool bSlotSelected = false;
	bool bWasCountingPlayTime = false;
	double LastPlayTimeSample = 0.0;
	double SecondsSincePeriodicSave = 0.0;
	FTSTicker::FDelegateHandle PlayTimeTickerHandle;

	bool bSaveInFlight = false;
	/** Keep an unsaved settlement in memory until a retry succeeds; never switch or reset this slot. */
	bool bSaveRecoveryPending = false;
	bool bSavePending = false;
	FString InFlightSlotName;
	int32 SaveRetryCount = 0;
	FTSTicker::FDelegateHandle SaveRetryTickerHandle;
#if WITH_EDITOR
	int32 DebugFailuresRemaining = 0;
	FTimerHandle DebugFailureTimer;
	void HandleDebugSaveFailure();
#endif
};
