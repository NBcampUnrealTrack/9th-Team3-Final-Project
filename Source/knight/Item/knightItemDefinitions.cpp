#include "Item/knightItemDefinitions.h"

#include "AbilitySystem/knightAbilitySet.h"
#include "Engine/SkeletalMesh.h"
#include "Monster/knightMonsterDefinition.h"
#include "System/knightGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightItemDefinitions"

UknightMaterialItemDefinition::UknightMaterialItemDefinition()
{
	ItemType = knightGameplayTags::Item_Type_Material;
	MaxStack = 999;
}

UknightPartItemDefinition::UknightPartItemDefinition()
{
	ItemType = knightGameplayTags::Item_Type_Part;
	MaxStack = 1;
}

#if WITH_EDITOR
EDataValidationResult UknightPartItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	if (!SlotTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoSlot", "Part has no SlotTag."));
		Result = EDataValidationResult::Invalid;
	}
	if (Mesh.IsNull())
	{
		Context.AddWarning(LOCTEXT("NoMesh", "Part has no Mesh; it grants stats but shows nothing."));
	}
	for (int32 Step = 0; Step < UpgradeSteps.Num(); ++Step)
	{
		for (const FknightItemCost& Cost : UpgradeSteps[Step].Costs)
		{
			if (!Cost.Item)
			{
				Context.AddError(FText::Format(LOCTEXT("EmptyCost", "UpgradeSteps[{0}] has a cost with no item."), Step));
				Result = EDataValidationResult::Invalid;
			}
		}
	}
	return Result;
}
#endif

UknightWeaponItemDefinition::UknightWeaponItemDefinition()
{
	ItemType = knightGameplayTags::Item_Type_Weapon;
	SlotTag = knightGameplayTags::Equipment_Slot_Weapon;
	MeshComponentTags.Add(TEXT("Weapon"));
	Attachment.AttachMode = EMechPartAttachMode::Socket;
	Attachment.AttachSocket = TEXT("hand_r_weapon");
}

#undef LOCTEXT_NAMESPACE
