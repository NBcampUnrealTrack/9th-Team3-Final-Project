#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/PrimaryAssetId.h"
#include "knightInventoryLibrary.generated.h"

class UknightItemDefinition;

UCLASS()
class KNIGHTINVENTORY_API UknightInventoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolves an item id to its definition, loading it synchronously if needed. Requires the "Item" primary
	 * asset type to be registered for scanning in the Asset Manager settings.
	 */
	static const UknightItemDefinition* LoadItemDefinition(FPrimaryAssetId ItemId);
};
