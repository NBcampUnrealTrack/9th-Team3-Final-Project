#include "ViewModels/knightVM_PlayerStatus.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Combat/knightTargetLockComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "System/knightGameplayTags.h"

void UknightVM_PlayerStatus::Bind(UAbilitySystemComponent* InASC, UknightTargetLockComponent* InTargetLock)
{
	Unbind();
	BoundASC = InASC;
	BoundTargetLock = InTargetLock;

	if (InASC)
	{
		const FGameplayAttribute Attributes[] = {
			UknightCoreAttributeSet::GetHealthAttribute(),
			UknightCoreAttributeSet::GetMaxHealthAttribute(),
			UknightAttributeSet::GetBoostGaugeAttribute(),
			UknightAttributeSet::GetMaxBoostGaugeAttribute()
		};
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			AttributeHandles.Emplace(Attribute, InASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
		}
		OverheatedHandle = InASC->RegisterGameplayTagEvent(knightGameplayTags::State_Overheated, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleOverheatedChanged);
	}
	if (InTargetLock)
	{
		InTargetLock->OnLockOnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleLockOnChanged);
	}
	RefreshAll();
}

void UknightVM_PlayerStatus::Unbind()
{
	if (UAbilitySystemComponent* const ASC = BoundASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Entry : AttributeHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Entry.Key).Remove(Entry.Value);
		}
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Overheated, EGameplayTagEventType::NewOrRemoved).Remove(OverheatedHandle);
	}
	if (UknightTargetLockComponent* const TargetLock = BoundTargetLock.Get())
	{
		TargetLock->OnLockOnTargetChanged.RemoveDynamic(this, &ThisClass::HandleLockOnChanged);
	}
	AttributeHandles.Reset();
	OverheatedHandle.Reset();
	BoundASC.Reset();
	BoundTargetLock.Reset();
}

void UknightVM_PlayerStatus::RefreshAll()
{
	const UAbilitySystemComponent* const ASC = BoundASC.Get();
	bool bFound = false;
	UE_MVVM_SET_PROPERTY_VALUE(Health, ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(BoostGauge, ASC ? ASC->GetGameplayAttributeValue(UknightAttributeSet::GetBoostGaugeAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxBoostGauge, ASC ? ASC->GetGameplayAttributeValue(UknightAttributeSet::GetMaxBoostGaugeAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bOverheated, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Overheated));
	RefreshPercents();

	const UknightTargetLockComponent* const TargetLock = BoundTargetLock.Get();
	HandleLockOnChanged(TargetLock ? TargetLock->GetLockedTarget() : nullptr, TargetLock ? TargetLock->GetLockedPartTag() : FGameplayTag());
}

void UknightVM_PlayerStatus::RefreshPercents()
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(BoostPercent, MaxBoostGauge > 0.f ? BoostGauge / MaxBoostGauge : 0.f);
}

void UknightVM_PlayerStatus::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == UknightCoreAttributeSet::GetHealthAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(Health, Data.NewValue);
	}
	else if (Data.Attribute == UknightCoreAttributeSet::GetMaxHealthAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Data.NewValue);
	}
	else if (Data.Attribute == UknightAttributeSet::GetBoostGaugeAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(BoostGauge, Data.NewValue);
	}
	else if (Data.Attribute == UknightAttributeSet::GetMaxBoostGaugeAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxBoostGauge, Data.NewValue);
	}
	RefreshPercents();
}

void UknightVM_PlayerStatus::HandleOverheatedChanged(const FGameplayTag Tag, int32 NewCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(bOverheated, NewCount > 0);
}

void UknightVM_PlayerStatus::HandleLockOnChanged(AActor* NewTarget, FGameplayTag PartTag)
{
	UE_MVVM_SET_PROPERTY_VALUE(bLockedOn, NewTarget != nullptr);

	FText Name;
	if (const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(NewTarget))
	{
		const UknightMonsterDefinition* const Definition = Monster->GetMonsterDefinition();
		Name = Definition ? Definition->DisplayName : FText::GetEmpty();
	}
	if (Name.IsEmpty() && NewTarget)
	{
		Name = FText::FromString(NewTarget->GetActorNameOrLabel());
	}
	UE_MVVM_SET_PROPERTY_VALUE(LockedTargetName, Name);
}
