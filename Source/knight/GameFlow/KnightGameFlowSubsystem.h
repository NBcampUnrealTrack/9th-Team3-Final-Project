#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightFlowTypes.h"
#include "KnightGameFlowSubsystem.generated.h"

class UWorld;
class AKnightMenuGameMode;

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

	/** True means the request was accepted, not that the destination was entered. */
	UFUNCTION(BlueprintCallable, Category="Knight|Flow")
	bool RequestOpenMainMenu();

	/** True means the request was accepted, not that the destination was entered. */
	UFUNCTION(BlueprintCallable, Category="Knight|Flow")
	bool RequestOpenTitle();

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
	bool RejectRequest(EKnightFlowState Destination, const FText& Reason);
	void SetCurrentFlowState(EKnightFlowState NewState);
	void SetTravelPending(bool bNewPending);
	void ResetPendingTravel();
	void HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	EKnightFlowState CurrentFlowState = EKnightFlowState::Uninitialized;
	EKnightFlowState PendingDestination = EKnightFlowState::Uninitialized;
	bool bTravelPending = false;
	FString PendingMapPackage;
	TWeakObjectPtr<UWorld> PendingSourceWorld;
	TWeakObjectPtr<UWorld> RegisteredWorld;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;
};
