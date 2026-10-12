#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

class UknightPartItemDefinition;
class UknightMaintenanceCatalog;
class UknightProgressionSubsystem;

/** A detached value calculation: never touches an ASC, mesh, inventory or save. */
struct KNIGHT_API FknightEquipmentStats
{
	TMap<FGameplayAttribute, float> Values;
	TMap<FGameplayTag, int32> SetCounts;
	bool bComplete = true;
	FString IncompleteReason;
};

namespace knightEquipmentStats
{
	using FLoadout = TMap<FGameplayTag, const UknightPartItemDefinition*>;
	KNIGHT_API FLoadout ReadLoadout(const UknightProgressionSubsystem* Progression, const UknightMaintenanceCatalog* Catalog);
	KNIGHT_API FknightEquipmentStats Calculate(const FLoadout& Loadout, const UknightMaintenanceCatalog* Catalog,
		const UknightProgressionSubsystem* Progression);
}
