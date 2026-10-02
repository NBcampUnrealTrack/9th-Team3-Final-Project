#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "knightInventoryTypes.h"
#include "knightRaidTypes.generated.h"

/** Where a raid is. Explore and Combat alternate as the monster finds and loses the Knight. */
UENUM(BlueprintType)
enum class EknightRaidPhase : uint8
{
	None,
	/** Dropped into the area; input can be locked for the landing sequence. */
	Deploy,
	/** Searching for the monster. */
	Explore,
	/** The target monster is fighting the Knight. */
	Combat,
	/** Target defeated; the result countdown runs before returning. */
	Success,
	Failed,
	/** Travel back to the main menu has been requested. */
	Returning
};

UENUM(BlueprintType)
enum class EknightRaidFailReason : uint8
{
	None,
	KnightDestroyed,
	TimeUp,
	Abandoned
};

/** Outcome handed to the main menu through UknightRaidSubsystem. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightRaidResult
{
	GENERATED_BODY()

	/** Unique across launches and persisted with reward application. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Raid")
	EknightRaidFailReason FailReason = EknightRaidFailReason::None;

	/** Seconds from the end of deployment to the result. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	float ElapsedTime = 0.f;

	/** Parts broken during the raid. Part-break rewards require these. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	FGameplayTagContainer BrokenParts;

	/** Items won. Added to the item box by UknightProgressionSubsystem. */
	UPROPERTY(BlueprintReadOnly, Category="Raid")
	TArray<FknightItemStack> Rewards;
};
