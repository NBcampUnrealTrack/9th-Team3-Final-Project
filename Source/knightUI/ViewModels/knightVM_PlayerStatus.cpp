#include "ViewModels/knightVM_PlayerStatus.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Combat/knightTargetLockComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartData.h"
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
	BindParts(nullptr);
	BindTargetPoise(nullptr);
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
	FText Name;
	FText PartName;
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(NewTarget);
	if (const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr)
	{
		Name = Definition->DisplayName;
		if (Definition->PartData && PartTag.IsValid())
		{
			const FknightMonsterPartDefinition* const Part = Definition->PartData->Parts.FindByPredicate(
				[&PartTag](const FknightMonsterPartDefinition& Each) { return Each.PartTag == PartTag; });
			PartName = Part ? Part->DisplayName : FText::GetEmpty();
		}
	}
	if (Name.IsEmpty() && NewTarget)
	{
		Name = FText::FromString(NewTarget->GetActorNameOrLabel());
	}

	UknightMonsterPartComponent* const Parts = Monster && PartTag.IsValid() ? Monster->FindComponentByClass<UknightMonsterPartComponent>() : nullptr;
	BindParts(Parts);
	LockedPartTag = Parts ? PartTag : FGameplayTag();
	BindTargetPoise(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewTarget));

	// Part fields first: widgets refresh on bLockedOn and should already see the new part.
	UE_MVVM_SET_PROPERTY_VALUE(LockedPartCondition, Parts ? Parts->GetPartCondition(PartTag) : EknightPartCondition::Unbreakable);
	UE_MVVM_SET_PROPERTY_VALUE(LockedPartName, PartName);
	UE_MVVM_SET_PROPERTY_VALUE(bHasLockedPart, Parts != nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(LockedTargetName, Name);
	UE_MVVM_SET_PROPERTY_VALUE(bLockedOn, NewTarget != nullptr);
}

void UknightVM_PlayerStatus::BindParts(UknightMonsterPartComponent* InParts)
{
	if (BoundParts.Get() == InParts)
	{
		return;
	}
	if (UknightMonsterPartComponent* const Old = BoundParts.Get())
	{
		Old->OnPartConditionChanged.RemoveDynamic(this, &ThisClass::HandlePartConditionChanged);
	}
	BoundParts = InParts;
	if (InParts)
	{
		InParts->OnPartConditionChanged.AddUniqueDynamic(this, &ThisClass::HandlePartConditionChanged);
	}
}

void UknightVM_PlayerStatus::BindTargetPoise(UAbilitySystemComponent* InTargetASC)
{
	// Only targets with the core attribute set have poise; the player's own ASC is never a lock target.
	if (InTargetASC && !InTargetASC->HasAttributeSetForAttribute(UknightCoreAttributeSet::GetPoiseAttribute()))
	{
		InTargetASC = nullptr;
	}
	if (BoundTargetASC.Get() != InTargetASC)
	{
		if (UAbilitySystemComponent* const Old = BoundTargetASC.Get())
		{
			Old->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetPoiseAttribute()).Remove(TargetPoiseHandle);
			Old->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxPoiseAttribute()).Remove(TargetMaxPoiseHandle);
		}
		TargetPoiseHandle.Reset();
		TargetMaxPoiseHandle.Reset();
		BoundTargetASC = InTargetASC;
		if (InTargetASC)
		{
			TargetPoiseHandle = InTargetASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetPoiseAttribute()).AddUObject(this, &ThisClass::HandleTargetPoiseChanged);
			TargetMaxPoiseHandle = InTargetASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxPoiseAttribute()).AddUObject(this, &ThisClass::HandleTargetPoiseChanged);
		}
	}
	RefreshTargetStagger();
}

void UknightVM_PlayerStatus::HandleTargetPoiseChanged(const FOnAttributeChangeData& Data)
{
	RefreshTargetStagger();
}

void UknightVM_PlayerStatus::RefreshTargetStagger()
{
	const UAbilitySystemComponent* const ASC = BoundTargetASC.Get();
	float Stagger = 0.f;
	if (ASC)
	{
		bool bFound = false;
		const float Poise = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetPoiseAttribute(), bFound);
		const float MaxPoise = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxPoiseAttribute(), bFound);
		// Poise counts down to the stagger; the gauge shows how much has been taken.
		Stagger = MaxPoise > 0.f ? FMath::Clamp(1.f - Poise / MaxPoise, 0.f, 1.f) : 0.f;
	}
	UE_MVVM_SET_PROPERTY_VALUE(LockedTargetStagger, Stagger);
	UE_MVVM_SET_PROPERTY_VALUE(bLockedTargetHasStagger, ASC != nullptr);
}

void UknightVM_PlayerStatus::HandlePartConditionChanged(FGameplayTag PartTag, EknightPartCondition NewCondition)
{
	if (PartTag != LockedPartTag || NewCondition == LockedPartCondition)
	{
		return;
	}
	UE_MVVM_SET_PROPERTY_VALUE(LockedPartCondition, NewCondition);
	OnLockedPartConditionChanged.Broadcast(NewCondition);
}
