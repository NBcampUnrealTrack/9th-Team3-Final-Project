#include "Item/knightItemDefinitions.h"

#include "AbilitySystem/knightAbilitySet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/CustomCanApplyGameplayEffectComponent.h"
#include "GameplayEffectComponents/ChanceToApplyGameplayEffectComponent.h"
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
	// hand_r_weapon → weapon_r. 지금 기사 메시(Knight_v2)에는 hand_r_weapon 소켓·뼈가 없어 무기가 메시 원점에 붙었고,
	// '손에 들었는지' 검사(weapon_r)도 실패해 첫 공격 때 검을 뽑았다. 단절(DA_Weapon_Danjeol)은 원래 weapon_r.
	Attachment.AttachSocket = TEXT("weapon_r");
}

float UknightWeaponItemDefinition::GetUpgradeAttackBonus(int32 Level) const
{
	float Bonus = 0.f;
	for (int32 Index = 0; Index < FMath::Clamp(Level, 0, UpgradeSteps.Num()); ++Index)
		Bonus += UpgradeSteps[Index].AttackIncrease;
	return Bonus;
}

float UknightWeaponItemDefinition::GetWeaponAttackAtLevel(int32 Level) const
{
	float Attack = GetUpgradeAttackBonus(Level);
	for (const FknightPartStatModifier& Mod : StatModifiers)
		if (Mod.Attribute == UknightCoreAttributeSet::GetAttackPowerAttribute()
			&& Mod.ModifierOp == EGameplayModOp::AddBase) Attack += Mod.Value;
	// Some legacy/default weapons express their fixed base attack in an AbilitySet.
	// Include only an unconditional, constant additive contribution. Dynamic/conditional
	// effects remain outside this weapon-only quote; never manufacture their exact value.
	if (AbilitySet)
		for (const FknightAbilitySet_GameplayEffect& Granted : AbilitySet->GetGrantedGameplayEffects())
		{
			const UGameplayEffect* Effect = Granted.GameplayEffect ? Granted.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
			if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite) continue;
			// These components can suppress the GE even when individual modifiers have no tags.
			if (Effect->FindComponent<UTargetTagRequirementsGameplayEffectComponent>()
				|| Effect->FindComponent<UCustomCanApplyGameplayEffectComponent>()
				|| Effect->FindComponent<UChanceToApplyGameplayEffectComponent>()) continue;
			for (const FGameplayModifierInfo& Mod : Effect->Modifiers)
			{
				float Value = 0.f;
				if (Mod.Attribute == UknightCoreAttributeSet::GetAttackPowerAttribute() && Mod.ModifierOp == EGameplayModOp::AddBase
					&& Mod.SourceTags.IsEmpty() && Mod.TargetTags.IsEmpty()
					&& Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(Granted.EffectLevel, Value))
				{
					// Level-dependent GEs are not the separate upgrade bonus. Only include
					// contributions stable across every supported integer equipment level.
					bool bFixed = true;
					for (int32 Offset = 1; Offset <= GetMaxUpgradeLevel(); ++Offset)
					{
						float AtLevel = 0.f;
						if (!Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(Granted.EffectLevel + Offset, AtLevel)
							|| !FMath::IsNearlyEqual(AtLevel, Value)) { bFixed = false; break; }
					}
					if (bFixed) Attack += Value;
				}
			}
		}
	return Attack;
}

#undef LOCTEXT_NAMESPACE
