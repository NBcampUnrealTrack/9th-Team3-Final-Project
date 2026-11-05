#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LoadingProcessInterface.h"
#include "Raid/knightRaidTypes.h"
#include "knightRaidGameState.generated.h"

class UknightRaidDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightRaidPhaseChanged, EknightRaidPhase, OldPhase, EknightRaidPhase, NewPhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FknightRaidFinishStageChanged, EknightRaidFinishStage);

/**
 * Current raid state for UI and gameplay to read. Written only by AknightRaidGameMode.
 * Keeps the CommonLoadingScreen up until the raid has started, so the player never sees the map before
 * the monster and loadout are in place.
 */
UCLASS()
class KNIGHT_API AknightRaidGameState : public AGameStateBase, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	//~ ILoadingProcessInterface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;

	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	EknightRaidPhase GetRaidPhase() const { return RaidPhase; }

	const UknightRaidDefinition* GetRaidDefinition() const { return RaidDefinition; }
	const FText& GetLoadingStatus() const { return LoadingStatus; }

	/** Seconds since exploration began. Zero before that. */
	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	float GetElapsedRaidTime() const;

	/** Seconds left before time up, or -1 with no time limit. */
	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	float GetRemainingRaidTime() const;

	UPROPERTY(BlueprintAssignable, Category="Knight|Raid")
	FknightRaidPhaseChanged OnRaidPhaseChanged;

	/** 처치 연출 단계. None이 아니고 Done 전이면 연출 중이다(HUD는 숨기고, 결과 화면은 Done에서 연다). */
	EknightRaidFinishStage GetFinishStage() const { return FinishStage; }
	bool IsFinishPresentationPlaying() const { return FinishStage != EknightRaidFinishStage::None && FinishStage != EknightRaidFinishStage::Done; }
	FknightRaidFinishStageChanged OnFinishStageChanged;

private:
	friend class AknightRaidGameMode;
	friend class UknightRaidFinishPresentation;

	void SetFinishStage(EknightRaidFinishStage NewStage)
	{
		if (FinishStage != NewStage)
		{
			FinishStage = NewStage;
			OnFinishStageChanged.Broadcast(NewStage);
		}
	}

	void SetRaidDefinition(const UknightRaidDefinition* InDefinition) { RaidDefinition = InDefinition; }
	void SetLoadingStatus(const FText& InStatus) { LoadingStatus = InStatus; }
	void SetRaidPhase(EknightRaidPhase NewPhase);
	void StartRaidClock();
	void StopRaidClock();

	UPROPERTY()
	TObjectPtr<const UknightRaidDefinition> RaidDefinition;
	FText LoadingStatus;

	EknightRaidPhase RaidPhase = EknightRaidPhase::None;
	EknightRaidFinishStage FinishStage = EknightRaidFinishStage::None;
	double RaidClockStartTime = -1.0;
	double RaidClockStopTime = -1.0;
};
