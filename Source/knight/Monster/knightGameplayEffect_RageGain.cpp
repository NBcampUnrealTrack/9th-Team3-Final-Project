#include "Monster/knightGameplayEffect_RageGain.h"

#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "System/knightGameplayTags.h"

UknightGameplayEffect_RageGain::UknightGameplayEffect_RageGain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Rage;
	Rage.DataTag = knightGameplayTags::SetByCaller_Rage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UknightMonsterAttributeSet::GetRageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Rage);
	Modifiers.Add(Modifier);
}
