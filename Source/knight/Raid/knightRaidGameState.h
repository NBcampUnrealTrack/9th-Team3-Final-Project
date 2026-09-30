#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Raid/knightRaidTypes.h"
#include "knightRaidGameState.generated.h"

class UknightRaidDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightRaidPhaseChanged, EknightRaidPhase, OldPhase, EknightRaidPhase, NewPhase);

/** Current raid state for UI and gameplay to read. Written only by AknightRaidGameMode. */
UCLASS()
class KNIGHT_API AknightRaidGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	EknightRaidPhase GetRaidPhase() const { return RaidPhase; }

	const UknightRaidDefinition* GetRaidDefinition() const { return RaidDefinition; }

	/** Seconds since exploration began. Zero before that. */
	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	float GetElapsedRaidTime() const;

	/** Seconds left before time up, or -1 with no time limit. */
	UFUNCTION(BlueprintPure, Category="Knight|Raid")
	float GetRemainingRaidTime() const;

	UPROPERTY(BlueprintAssignable, Category="Knight|Raid")
	FknightRaidPhaseChanged OnRaidPhaseChanged;

private:
	friend class AknightRaidGameMode;

	void SetRaidDefinition(const UknightRaidDefinition* InDefinition) { RaidDefinition = InDefinition; }
	void SetRaidPhase(EknightRaidPhase NewPhase);
	void StartRaidClock();
	void StopRaidClock();

	UPROPERTY()
	TObjectPtr<const UknightRaidDefinition> RaidDefinition;

	EknightRaidPhase RaidPhase = EknightRaidPhase::None;
	double RaidClockStartTime = -1.0;
	double RaidClockStopTime = -1.0;
};
