#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightItemDefinition.generated.h"

class UTexture2D;

/**
 * Static description of an item. Every item subclass shares the primary asset type "Item", so an item is
 * identified by FPrimaryAssetId("Item", AssetName) no matter its class. Save games store that id.
 * Asset names must therefore be unique across all item assets.
 */
UCLASS(Abstract, BlueprintType, Const)
class KNIGHTINVENTORY_API UknightItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ItemAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Category tag for sorting and filters (Item.Type.Material, Item.Type.Part, ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(Categories="Item.Type"))
	FGameplayTag ItemType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin=1, ClampMax=10))
	int32 Rarity = 1;

	/** Largest stack. One for equipment, which also makes each copy a separate instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin=1))
	int32 MaxStack = 99;
};
