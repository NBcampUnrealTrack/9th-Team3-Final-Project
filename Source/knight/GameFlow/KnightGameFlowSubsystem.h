#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightFlowTypes.h"
#include "KnightStageCatalog.h"
#include "KnightGameFlowSubsystem.generated.h"

class UWorld;
class AKnightMenuGameMode;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKnightFlowStateChanged, EKnightFlowState, PreviousState, EKnightFlowState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKnightFlowTravelRejected, EKnightFlowState, RequestedState, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKnightFlowTravelFailed, EKnightFlowState, RequestedState, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnightFlowTravelPendingChanged, bool, bIsPending);

/** Owns menu travel requests for one GameInstance. Menu pages are not flow states. */
UCLASS()
class KNIGHT_API UKnightGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Knight|Flow")
	EKnightFlowState GetCurrentFlowState() const { return CurrentFlowState; }

	UFUNCTION(BlueprintPure, Category="Knight|Flow")
	bool IsTravelPending() const { return bTravelPending; }

	/** Only a confirmed Gameplay arrival has an active stage. Direct test-map play has none. */
	UFUNCTION(BlueprintPure, Category="Knight|Flow")
	FName GetActiveStageId() const { return ActiveStageId; }

	/** Returns copies of valid-ID definitions. Invalid and duplicate IDs are omitted. */
	UFUNCTION(BlueprintCallable, Category="Knight|Stages")
	bool GetStageDefinitions(TArray<FKnightStageDefinition>& OutStages) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Stages")
	bool FindStageDefinition(FName StageId, FKnightStageDefinition& OutStage) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Stages")
	bool CanLaunchStage(FName StageId, FText& OutReason) const;

	/** True means the request was accepted, not that the destination was entered. */
	UFUNCTION(BlueprintCallable, Category="Knight|Flow")
	bool RequestOpenMainMenu();

	/** True means the request was accepted, not that the destination was entered. */
	UFUNCTION(BlueprintCallable, Category="Knight|Flow")
	bool RequestOpenTitle();

	/** True means the stage travel request was accepted, not that Gameplay was entered. */
	UFUNCTION(BlueprintCallable, Category="Knight|Flow")
	bool RequestStartStage(FName StageId);

#if WITH_EDITOR
	/** Test hook consumed by the next asynchronous raid-definition completion. */
	void DebugFailNextRaidLoad() { bDebugFailNextRaidLoad = true; }
#endif

	/** The startup logos play once per game session; returning to the title later skips them. Returns true the first time. */
	bool ConsumeStartupIntro() { return !bStartupIntroPlayed && (bStartupIntroPlayed = true); }

	/** Native GameMode initialization hook; call from a Gameplay GameMode's InitGame. */
	void RegisterEnteredGameplay(UWorld* EnteredWorld);

	UPROPERTY(BlueprintAssignable, Category="Knight|Flow")
	FKnightFlowStateChanged OnFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Knight|Flow")
	FKnightFlowTravelRejected OnTravelRejected;

	UPROPERTY(BlueprintAssignable, Category="Knight|Flow")
	FKnightFlowTravelFailed OnTravelFailed;

	UPROPERTY(BlueprintAssignable, Category="Knight|Flow")
	FKnightFlowTravelPendingChanged OnTravelPendingChanged;

private:
	friend class AKnightMenuGameMode;

	/** Called only by the active menu GameMode during InitGame, before controllers initialize. */
	void RegisterEnteredMenu(EKnightFlowState MenuState, UWorld* EnteredWorld);
	bool RequestOpenMenu(EKnightFlowState Destination);
	bool BeginTravel(EKnightFlowState Destination, const FString& MapPackage, UWorld* SourceWorld, FName StageId = NAME_None);
	bool ResolveMapPackage(const TSoftObjectPtr<UWorld>& Map, FString& OutPackage) const;
	const UKnightStageCatalog* GetStageCatalog() const;
	bool RejectRequest(EKnightFlowState Destination, const FText& Reason);
	void SetCurrentFlowState(EKnightFlowState NewState);
	void SetTravelPending(bool bNewPending);
	void ConfirmEnteredWorld(EKnightFlowState NewState);
	void ResetPendingTravel();
	void HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleRaidDefinitionLoaded();

	EKnightFlowState CurrentFlowState = EKnightFlowState::Uninitialized;
	EKnightFlowState PendingDestination = EKnightFlowState::Uninitialized;
	bool bTravelPending = false;
	FString PendingMapPackage;
	FName PendingStageId;
	FName ActiveStageId;
	FSoftObjectPath PendingRaidDefinitionPath;
	TSharedPtr<FStreamableHandle> PendingLoadHandle;
	FGuid PendingRunId;
	TWeakObjectPtr<UWorld> PendingSourceWorld;
	TWeakObjectPtr<UWorld> RegisteredWorld;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;
	bool bStartupIntroPlayed = false;
#if WITH_EDITOR
	bool bDebugFailNextRaidLoad = false;
#endif
};
