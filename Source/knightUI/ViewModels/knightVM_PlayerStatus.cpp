#include "ViewModels/knightVM_PlayerStatus.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Combat/knightGroggyFeedbackComponent.h"
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
			UknightAttributeSet::GetMaxBoostGaugeAttribute(),
			UknightAttributeSet::GetHeadHealthAttribute(),
			UknightAttributeSet::GetMaxHeadHealthAttribute(),
			UknightAttributeSet::GetTorsoHealthAttribute(),
			UknightAttributeSet::GetMaxTorsoHealthAttribute(),
			UknightAttributeSet::GetArmsHealthAttribute(),
			UknightAttributeSet::GetMaxArmsHealthAttribute(),
			UknightAttributeSet::GetLegsHealthAttribute(),
			UknightAttributeSet::GetMaxLegsHealthAttribute(),
			UknightCoreAttributeSet::GetPoiseAttribute(),
			UknightCoreAttributeSet::GetMaxPoiseAttribute(),
			UknightAttributeSet::GetRepairKitCountAttribute(),
			UknightAttributeSet::GetMaxRepairKitCountAttribute(),
			UknightAttributeSet::GetRepairKitGaugeAttribute()
		};
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			AttributeHandles.Emplace(Attribute, InASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
		}
		OverheatedHandle = InASC->RegisterGameplayTagEvent(knightGameplayTags::State_Overheated, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleOverheatedChanged);
		GroggyHandle = InASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Groggy, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleGroggyChanged);
		KnockdownHandle = InASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Knockdown, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleKnockdownChanged);
		if (UknightGroggyFeedbackComponent* const Feedback = InASC->GetAvatarActor() ? InASC->GetAvatarActor()->FindComponentByClass<UknightGroggyFeedbackComponent>() : nullptr)
		{
			BoundGroggyFeedback = Feedback;
			GroggyPresentationHandle = Feedback->OnPresentationEvent.AddWeakLambda(this, [this](bool bImpact) { OnGroggyPresentation.Broadcast(bImpact); });
		}
		for (int32 Index = 0; Index < static_cast<int32>(EknightBodyPart::Count); ++Index)
		{
			const EknightBodyPart Part = static_cast<EknightBodyPart>(Index);
			PartBrokenHandles[Index] = InASC->RegisterGameplayTagEvent(UknightPartHealthComponent::GetBrokenTag(Part), EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &ThisClass::HandlePartBrokenChanged, Part);
		}
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
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Groggy, EGameplayTagEventType::NewOrRemoved).Remove(GroggyHandle);
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Knockdown, EGameplayTagEventType::NewOrRemoved).Remove(KnockdownHandle);
		for (int32 Index = 0; Index < static_cast<int32>(EknightBodyPart::Count); ++Index)
		{
			ASC->RegisterGameplayTagEvent(UknightPartHealthComponent::GetBrokenTag(static_cast<EknightBodyPart>(Index)), EGameplayTagEventType::NewOrRemoved)
				.Remove(PartBrokenHandles[Index]);
		}
	}
	if (UknightTargetLockComponent* const TargetLock = BoundTargetLock.Get())
	{
		TargetLock->OnLockOnTargetChanged.RemoveDynamic(this, &ThisClass::HandleLockOnChanged);
	}
	if (UknightGroggyFeedbackComponent* const Feedback = BoundGroggyFeedback.Get())
	{
		Feedback->OnPresentationEvent.Remove(GroggyPresentationHandle);
	}
	BoundGroggyFeedback.Reset();
	GroggyPresentationHandle.Reset();
	KnockdownHandle.Reset();
	BindParts(nullptr);
	AttributeHandles.Reset();
	OverheatedHandle.Reset();
	GroggyHandle.Reset();
	for (FDelegateHandle& Handle : PartBrokenHandles)
	{
		Handle.Reset();
	}
	BoundASC.Reset();
	BoundTargetLock.Reset();
}

void UknightVM_PlayerStatus::RefreshAll()
{
	const UAbilitySystemComponent* const ASC = BoundASC.Get();
	bool bFound = false;
	UE_MVVM_SET_PROPERTY_VALUE(Health, ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound) : 0.f);
	// 스테미너 가불로 음수가 될 수 있다. 화면에는 0으로 보인다.
	UE_MVVM_SET_PROPERTY_VALUE(BoostGauge, ASC ? FMath::Max(0.f, ASC->GetGameplayAttributeValue(UknightAttributeSet::GetBoostGaugeAttribute(), bFound)) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxBoostGauge, ASC ? ASC->GetGameplayAttributeValue(UknightAttributeSet::GetMaxBoostGaugeAttribute(), bFound) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bOverheated, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Overheated));
	UE_MVVM_SET_PROPERTY_VALUE(bGroggy, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Groggy));
	int32 Broken = 0;
	for (int32 Index = 0; Index < static_cast<int32>(EknightBodyPart::Count); ++Index)
	{
		if (ASC && ASC->HasMatchingGameplayTag(UknightPartHealthComponent::GetBrokenTag(static_cast<EknightBodyPart>(Index))))
		{
			Broken |= 1 << Index;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(BrokenParts, Broken);
	RefreshPercents();
	RefreshKnightParts();

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
		UE_MVVM_SET_PROPERTY_VALUE(BoostGauge, FMath::Max(0.f, Data.NewValue));
	}
	else if (Data.Attribute == UknightAttributeSet::GetMaxBoostGaugeAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxBoostGauge, Data.NewValue);
	}
	else
	{
		RefreshKnightParts();
	}
	RefreshPercents();
}

void UknightVM_PlayerStatus::HandleGroggyChanged(const FGameplayTag Tag, int32 NewCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(bGroggy, NewCount > 0);
}

void UknightVM_PlayerStatus::HandleKnockdownChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		OnPlayerKnockdown.Broadcast();
	}
}

bool UknightVM_PlayerStatus::IsGroggyPresentationActive() const
{
	const UknightGroggyFeedbackComponent* const Feedback = BoundGroggyFeedback.Get();
	return Feedback && Feedback->IsPresentationActive();
}

void UknightVM_PlayerStatus::HandlePartBrokenChanged(const FGameplayTag Tag, int32 NewCount, EknightBodyPart Part)
{
	// Follows the part health component's tag instead of re-checking health: a broken part stays broken
	// until it is repaired to 1 or more, so health dipping back to 0 in between is not a new break.
	const int32 Bit = 1 << static_cast<int32>(Part);
	UE_MVVM_SET_PROPERTY_VALUE(BrokenParts, NewCount > 0 ? BrokenParts | Bit : BrokenParts & ~Bit);
	if (NewCount > 0)
	{
		OnPlayerPartBroken.Broadcast(Part);
	}
}

float UknightVM_PlayerStatus::GetPartHealthPercent(EknightBodyPart Part) const
{
	switch (Part)
	{
	case EknightBodyPart::Head: return HeadHealthPercent;
	case EknightBodyPart::Torso: return TorsoHealthPercent;
	case EknightBodyPart::Arms: return ArmsHealthPercent;
	case EknightBodyPart::Legs: return LegsHealthPercent;
	default: return 1.f;
	}
}

void UknightVM_PlayerStatus::RefreshKnightParts()
{
	const UAbilitySystemComponent* const ASC = BoundASC.Get();
	const auto Value = [ASC](const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		return ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
	};
	// A part with no maximum (no attribute set yet) reads as intact rather than broken.
	const auto Percent = [&Value](const FGameplayAttribute& Current, const FGameplayAttribute& Max)
	{
		const float MaxValue = Value(Max);
		return MaxValue > 0.f ? FMath::Clamp(Value(Current) / MaxValue, 0.f, 1.f) : 1.f;
	};
	UE_MVVM_SET_PROPERTY_VALUE(HeadHealthPercent, Percent(UknightAttributeSet::GetHeadHealthAttribute(), UknightAttributeSet::GetMaxHeadHealthAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(TorsoHealthPercent, Percent(UknightAttributeSet::GetTorsoHealthAttribute(), UknightAttributeSet::GetMaxTorsoHealthAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ArmsHealthPercent, Percent(UknightAttributeSet::GetArmsHealthAttribute(), UknightAttributeSet::GetMaxArmsHealthAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(LegsHealthPercent, Percent(UknightAttributeSet::GetLegsHealthAttribute(), UknightAttributeSet::GetMaxLegsHealthAttribute()));

	// Poise counts down to the stagger; the gauge shows how much has been taken.
	const float MaxPoise = Value(UknightCoreAttributeSet::GetMaxPoiseAttribute());
	UE_MVVM_SET_PROPERTY_VALUE(Stagger, MaxPoise > 0.f ? FMath::Clamp(1.f - Value(UknightCoreAttributeSet::GetPoiseAttribute()) / MaxPoise, 0.f, 1.f) : 0.f);

	UE_MVVM_SET_PROPERTY_VALUE(RepairKitCount, FMath::RoundToInt(Value(UknightAttributeSet::GetRepairKitCountAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(MaxRepairKitCount, FMath::RoundToInt(Value(UknightAttributeSet::GetMaxRepairKitCountAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(RepairKitGaugePercent, FMath::Clamp(Value(UknightAttributeSet::GetRepairKitGaugeAttribute()) / UknightAttributeSet::RepairKitGaugeFull, 0.f, 1.f));
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

void UknightVM_PlayerStatus::HandlePartConditionChanged(FGameplayTag PartTag, EknightPartCondition NewCondition)
{
	if (PartTag != LockedPartTag || NewCondition == LockedPartCondition)
	{
		return;
	}
	UE_MVVM_SET_PROPERTY_VALUE(LockedPartCondition, NewCondition);
	OnLockedPartConditionChanged.Broadcast(NewCondition);
}
