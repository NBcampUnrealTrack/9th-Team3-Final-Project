#include "AbilitySystem/Attributes/knightAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

UknightAttributeSet::UknightAttributeSet()
{
	InitBoostGauge(100.f);
	InitMaxBoostGauge(100.f);
	InitBoostRecoveryRate(25.f);
}

void UknightAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UknightAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetBoostGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxBoostGauge());
	}
	else if (Attribute == GetMaxBoostGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetBoostRecoveryRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UknightAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxBoostGaugeAttribute())
	{
		AdjustForMaxChange(GetOwningAbilitySystemComponent(), GetBoostGaugeAttribute(), OldValue, NewValue, true);
	}
	else if (Attribute == GetBoostGaugeAttribute() && NewValue > 0.f)
	{
		bBoostDepleted = false;
	}
}

void UknightAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetBoostGaugeAttribute() && GetBoostGauge() <= 0.f && !bBoostDepleted)
	{
		bBoostDepleted = true;
		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
		OnBoostDepleted.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			Data.EvaluatedData.Magnitude, GetBoostGauge() - Data.EvaluatedData.Magnitude, GetBoostGauge());
	}
}
