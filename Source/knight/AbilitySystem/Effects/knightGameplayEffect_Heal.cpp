#include "AbilitySystem/Effects/knightGameplayEffect_Heal.h"

#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "System/knightGameplayTags.h"

UknightGameplayEffect_Heal::UknightGameplayEffect_Heal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Healing;
	Healing.DataTag = knightGameplayTags::SetByCaller_Healing;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UknightCoreAttributeSet::GetHealingAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Healing);
	Modifiers.Add(Modifier);
}
