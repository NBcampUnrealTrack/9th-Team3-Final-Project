#include "knightItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightItemDefinition"

const FPrimaryAssetType UknightItemDefinition::ItemAssetType(TEXT("Item"));

FPrimaryAssetId UknightItemDefinition::GetPrimaryAssetId() const
{
	// Class default objects are not items.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(ItemAssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UknightItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoName", "Item has no DisplayName."));
	}
	if (!ItemType.IsValid())
	{
		Context.AddWarning(LOCTEXT("NoType", "Item has no ItemType tag."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
