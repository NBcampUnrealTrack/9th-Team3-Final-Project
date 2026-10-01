#include "AbilitySystem/Attributes/knightAttributeSetBase.h"

#include "AbilitySystem/knightAbilitySystemComponent.h"

UknightAbilitySystemComponent* UknightAttributeSetBase::GetKnightAbilitySystemComponent() const
{
	return Cast<UknightAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

void UknightAttributeSetBase::AdjustForMaxChange(UAbilitySystemComponent* ASC, const FGameplayAttribute& CurrentAttribute,
	float OldMax, float NewMax, bool bPreserveRatio)
{
	if (!ASC || FMath::IsNearlyEqual(OldMax, NewMax))
	{
		return;
	}
	const float Current = ASC->GetNumericAttributeBase(CurrentAttribute);
	float NewCurrent = Current;
	if (bPreserveRatio && OldMax > 0.f)
	{
		NewCurrent = Current * NewMax / OldMax;
	}
	NewCurrent = FMath::Clamp(NewCurrent, 0.f, NewMax);
	if (!FMath::IsNearlyEqual(NewCurrent, Current))
	{
		ASC->SetNumericAttributeBase(CurrentAttribute, NewCurrent);
	}
}
