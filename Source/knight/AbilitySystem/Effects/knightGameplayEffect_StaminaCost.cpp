#include "AbilitySystem/Effects/knightGameplayEffect_StaminaCost.h"

#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "System/knightGameplayTags.h"

UknightGameplayEffect_StaminaCost::UknightGameplayEffect_StaminaCost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Cost;
	Cost.DataTag = knightGameplayTags::SetByCaller_BoostCost;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UknightAttributeSet::GetBoostGaugeAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);
	Modifiers.Add(Modifier);
}
