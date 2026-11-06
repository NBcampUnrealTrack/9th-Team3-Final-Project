#pragma once
#include "CoreMinimal.h"

class UKnightQuestCatalog;
class UKnightQuestDefinition;
class UGameInstance;

struct FKnightQuestPresentation
{
	bool bCanLaunch = false;
	bool bProgressionLocked = false;
	bool bCompleted = false;
	FText Availability;
	FText Reason;
	FText ClearLabel;
};

/** Read-only projection of the existing catalog gate and active profile history. */
namespace KnightOperationMapPresentation
{
	KNIGHT_API FKnightQuestPresentation Query(const UKnightQuestCatalog* Catalog, const UKnightQuestDefinition* Quest, UGameInstance* Instance);
}
