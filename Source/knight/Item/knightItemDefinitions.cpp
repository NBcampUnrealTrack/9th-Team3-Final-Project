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

UknightSocketItemDefinition::UknightSocketItemDefinition()
{
	ItemType = knightGameplayTags::Item_Type_Socket;
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
	if (Mesh.IsNull() && StaticMesh.IsNull())
	{
		Context.AddWarning(LOCTEXT("NoMesh", "Part has no Mesh or StaticMesh; it grants stats but shows nothing."));
	}
	// 스태틱 메시는 뼈대가 없어 LeaderPose로 붙일 수 없다(장착할 때 Socket으로 붙이지만 데이터를 바로잡게 알린다).
	if (Mesh.IsNull() && !StaticMesh.IsNull() && Attachment.AttachMode == EMechPartAttachMode::LeaderPose)
	{
		Context.AddWarning(LOCTEXT("StaticLeaderPose", "StaticMesh parts cannot use LeaderPose; set Attachment to Socket mode."));
	}
	// 능력치 줄에 스탯을 고르지 않으면 장착해도 아무 효과가 없다.
	for (int32 Index = 0; Index < StatModifiers.Num(); ++Index)
	{
		if (!StatModifiers[Index].Attribute.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoStatAttribute", "StatModifiers[{0}] has no attribute."), Index));
			Result = EDataValidationResult::Invalid;
		}
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
