#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "knightRaidDefinition.generated.h"

class UknightMonsterDefinition;

/**
 * Rules of one raid (hunt). Referenced from the team's stage catalog entry (FKnightStageDefinition::RaidDefinition),
 * so the menu picks a stage and the raid GameMode reads its rules on arrival.
 * Rewards are added with the item phase.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightRaidDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Monster that must be defeated. Monsters placed in the map with this definition are the targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid")
	TObjectPtr<const UknightMonsterDefinition> TargetMonster;

	/** Zero means no time limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float TimeLimit = 1800.f;

	/** Length of the drop-in phase before exploring starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float DeployDuration = 3.f;

	/** Countdown after a success before returning to the main menu (time to carve and look around). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float SuccessReturnDelay = 20.f;

	/** Delay after a failure before returning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Raid", meta=(ClampMin=0, Units="s"))
	float FailureReturnDelay = 5.f;
};
