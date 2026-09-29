#pragma once

#include "CoreMinimal.h"
#include "Raid/knightRaidTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "knightRaidSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightRaidResultRecorded, const FknightRaidResult&, Result);

/**
 * Carries the last raid's result across the travel back to the main menu, where the result screen and
 * (later) rewards read it. Travel itself stays with UKnightGameFlowSubsystem.
 */
UCLASS()
class KNIGHT_API UknightRaidSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void RecordResult(const FknightRaidResult& Result);

	/** False if no raid has finished since the last ClearLastResult. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	bool GetLastResult(FknightRaidResult& OutResult) const;

	/** Call once the menu has shown the result. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void ClearLastResult();

	UPROPERTY(BlueprintAssignable, Category="Knight|Raid")
	FknightRaidResultRecorded OnRaidResultRecorded;

private:
	FknightRaidResult LastResult;
	bool bHasResult = false;
};
