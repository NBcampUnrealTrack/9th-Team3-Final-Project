#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

UknightMonsterAttributeSet::UknightMonsterAttributeSet()
{
	InitRage(0.f);
	InitMaxRage(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

void UknightMonsterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightMonsterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightMonsterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetRageAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxRage());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetMaxRageAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UknightMonsterAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxRageAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetRageAttribute(), OldValue, NewValue, false);
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetStaminaAttribute(), OldValue, NewValue, false);
	}
	// Re-arm the threshold events once the value moves away from its limit (rage cooled, stamina recovered).
	else if (Attribute == GetRageAttribute() && NewValue < GetMaxRage())
	{
		bRageMaxed = false;
	}
	else if (Attribute == GetStaminaAttribute() && NewValue > 0.f)
	{
		bStaminaDepleted = false;
	}
}

void UknightMonsterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	if (Data.EvaluatedData.Attribute == GetRageAttribute() && GetRage() >= GetMaxRage() && !bRageMaxed)
	{
		bRageMaxed = true;
		OnRageMaxed.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			Data.EvaluatedData.Magnitude, GetRage() - Data.EvaluatedData.Magnitude, GetRage());
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute() && GetStamina() <= 0.f && !bStaminaDepleted)
	{
		bStaminaDepleted = true;
		OnStaminaDepleted.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			Data.EvaluatedData.Magnitude, GetStamina() - Data.EvaluatedData.Magnitude, GetStamina());
	}
}
