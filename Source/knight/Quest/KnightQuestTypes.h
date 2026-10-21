#pragma once

#include "CoreMinimal.h"
#include "KnightQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EKnightQuestDifficulty : uint8
{
	Normal,
	Hard
};

/** One accepted quest request paired with the raid's existing RunId. Never persisted as completion state. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightQuestRunContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName BossId;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	EKnightQuestDifficulty Difficulty = EKnightQuestDifficulty::Normal;

	bool IsValid() const
	{
		return RunId.IsValid() && !QuestId.IsNone() && !StageId.IsNone() && !BossId.IsNone();
	}
};
