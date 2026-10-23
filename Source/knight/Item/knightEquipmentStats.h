#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

class UknightPartItemDefinition;
class UknightMaintenanceCatalog;
class UknightEquipmentSetDefinition;
class UknightProgressionSubsystem;

/** A detached value calculation: never touches an ASC, mesh, inventory or save. */
struct KNIGHT_API FknightEquipmentStats
{
	TMap<FGameplayAttribute, float> Values;
	TMap<FGameplayTag, int32> SetCounts;
	bool bComplete = true;
	FString IncompleteReason;
};

/** Isolated item modifiers, including its saved weapon upgrade and statically evaluable AbilitySet effects.
 *  Set tiers are intentionally excluded. Values are deltas from an empty loadout, not character totals. */
struct KNIGHT_API FknightItemContribution
{
	TMap<FGameplayAttribute, float> Values;
	bool bComplete = true;
	FString IncompleteReason;
};

/** Each tier is listed once. Set is a catalog asset; the tier lives in Set->TwoPiece/FourPiece. */
struct KNIGHT_API FknightActiveSetBonus
{
	const UknightEquipmentSetDefinition* Set = nullptr;
	int32 EquippedCount = 0;
	bool bFourPiece = false;
};

struct KNIGHT_API FknightEquipmentComparison
{
	FknightEquipmentStats Current;
	FknightEquipmentStats Replacement;
	TMap<FGameplayAttribute, float> Deltas;
	bool bValidReplacement = false;
	bool bExact = false;
	FString IncompleteReason;
};

namespace knightEquipmentStats
{
	using FLoadout = TMap<FGameplayTag, const UknightPartItemDefinition*>;
	KNIGHT_API FLoadout ReadLoadout(const UknightProgressionSubsystem* Progression, const UknightMaintenanceCatalog* Catalog);
	KNIGHT_API FknightEquipmentStats Calculate(const FLoadout& Loadout, const UknightMaintenanceCatalog* Catalog,
		const UknightProgressionSubsystem* Progression);
	/** Does not mutate the supplied loadout, inventory, ASC, or save. Null replacement means an empty slot. */
	KNIGHT_API FknightEquipmentComparison CompareReplacement(const FLoadout& CurrentLoadout, FGameplayTag Slot,
		const UknightPartItemDefinition* Replacement, const UknightMaintenanceCatalog* Catalog,
		const UknightProgressionSubsystem* Progression);
	KNIGHT_API FknightItemContribution CalculateItemContribution(const UknightPartItemDefinition* Part,
		const UknightProgressionSubsystem* Progression);
	KNIGHT_API TArray<FknightActiveSetBonus> GetActiveSetBonuses(const FknightEquipmentStats& Stats,
		const UknightMaintenanceCatalog* Catalog);
}
