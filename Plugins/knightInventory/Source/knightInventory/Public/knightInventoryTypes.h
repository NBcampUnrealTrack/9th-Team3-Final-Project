#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "knightInventoryTypes.generated.h"

class UknightItemDefinition;

/** A number of one item, at runtime. */
USTRUCT(BlueprintType)
struct KNIGHTINVENTORY_API FknightItemStack
{
	GENERATED_BODY()

	FknightItemStack() = default;
	FknightItemStack(const UknightItemDefinition* InItem, int32 InCount) : Item(InItem), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
	TObjectPtr<const UknightItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item", meta=(ClampMin=1))
	int32 Count = 1;
};

/** A number of one item, by id, for save games (definitions are resolved through the asset manager on load). */
USTRUCT(BlueprintType)
struct KNIGHTINVENTORY_API FknightSavedItemStack
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Item")
	FPrimaryAssetId ItemId;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Item")
	int32 Count = 0;
};

/**
 * Stacked item list with per-item stack limits. Used by UknightInventoryComponent and by anything else that
 * keeps items (the item box in the save game, reward lists).
 */
USTRUCT(BlueprintType)
struct KNIGHTINVENTORY_API FknightInventoryList
{
	GENERATED_BODY()

	/** Adds up to Count, respecting MaxStack. Returns how many were added. */
	int32 AddItem(const UknightItemDefinition* Item, int32 Count);

	/** Removes Count if available. Returns false and changes nothing otherwise. */
	bool RemoveItem(const UknightItemDefinition* Item, int32 Count);

	int32 GetCount(const UknightItemDefinition* Item) const;

	const TArray<FknightItemStack>& GetStacks() const { return Stacks; }
	void Reset() { Stacks.Reset(); }

	void ToSaved(TArray<FknightSavedItemStack>& OutSaved) const;
	/** Rebuilds the list from saved ids. Ids whose definition no longer exists are logged and dropped. */
	void FromSaved(const TArray<FknightSavedItemStack>& Saved);

private:
	UPROPERTY()
	TArray<FknightItemStack> Stacks;
};
