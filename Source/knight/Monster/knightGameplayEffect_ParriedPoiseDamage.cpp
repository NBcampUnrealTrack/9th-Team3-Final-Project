#include "Monster/knightGameplayEffect_ParriedPoiseDamage.h"

#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "System/knightGameplayTags.h"

UknightGameplayEffect_ParriedPoiseDamage::UknightGameplayEffect_ParriedPoiseDamage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat PoiseDamage;
	PoiseDamage.DataTag = knightGameplayTags::SetByCaller_PoiseDamage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UknightCoreAttributeSet::GetPoiseDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(PoiseDamage);
	Modifiers.Add(Modifier);
}
