#include "ViewModels/knightVM_BossStatus.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartComponent.h"
#include "System/knightGameplayTags.h"

void UknightVM_BossStatus::Bind(AknightMonsterCharacterBase* InMonster)
{
	Unbind();
	BoundMonster = InMonster;
	if (InMonster)
	{
		if (UAbilitySystemComponent* const ASC = InMonster->GetAbilitySystemComponent())
		{
			HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
			MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
			EnragedHandle = ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Enraged, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleStateTagChanged);
			ExhaustedHandle = ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Exhausted, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleStateTagChanged);
		}
		if (UknightMonsterPartComponent* const Parts = InMonster->GetPartComponent())
		{
			Parts->OnPartBroken.AddUniqueDynamic(this, &ThisClass::HandlePartBroken);
		}
		InMonster->OnMonsterDefeated.AddUniqueDynamic(this, &ThisClass::HandleDefeated);
	}
	RefreshAll();
}

void UknightVM_BossStatus::Unbind()
{
	if (AknightMonsterCharacterBase* const Monster = BoundMonster.Get())
	{
		if (UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
			ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
			ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Enraged, EGameplayTagEventType::NewOrRemoved).Remove(EnragedHandle);
			ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Exhausted, EGameplayTagEventType::NewOrRemoved).Remove(ExhaustedHandle);
		}
		if (UknightMonsterPartComponent* const Parts = Monster->GetPartComponent())
		{
			Parts->OnPartBroken.RemoveDynamic(this, &ThisClass::HandlePartBroken);
		}
		Monster->OnMonsterDefeated.RemoveDynamic(this, &ThisClass::HandleDefeated);
	}
	BoundMonster.Reset();
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	EnragedHandle.Reset();
	ExhaustedHandle.Reset();
}

void UknightVM_BossStatus::RefreshAll()
{
	const AknightMonsterCharacterBase* const Monster = BoundMonster.Get();
	UE_MVVM_SET_PROPERTY_VALUE(bHasMonster, Monster != nullptr);

	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Definition ? Definition->DisplayName : FText::GetEmpty());

	const UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	bool bFound = false;
	const float Health = ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound) : 0.f;
	const float MaxHealth = ASC ? ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound) : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bEnraged, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged));
	UE_MVVM_SET_PROPERTY_VALUE(bExhausted, ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted));
	UE_MVVM_SET_PROPERTY_VALUE(bDefeated, Monster && Monster->IsDead());

	int32 Broken = 0;
	if (const UknightMonsterPartComponent* const Parts = Monster ? Monster->GetPartComponent() : nullptr)
	{
		for (const FknightMonsterPartState& State : Parts->GetPartStates())
		{
			Broken += State.bBroken ? 1 : 0;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(BrokenPartCount, Broken);
}

void UknightVM_BossStatus::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshAll();
}

void UknightVM_BossStatus::HandleStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (Tag == knightGameplayTags::State_Monster_Enraged)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bEnraged, NewCount > 0);
	}
	else if (Tag == knightGameplayTags::State_Monster_Exhausted)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bExhausted, NewCount > 0);
	}
}

void UknightVM_BossStatus::HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator)
{
	UE_MVVM_SET_PROPERTY_VALUE(BrokenPartCount, BrokenPartCount + 1);
}

void UknightVM_BossStatus::HandleDefeated(AknightMonsterCharacterBase* Monster, AActor* Killer)
{
	UE_MVVM_SET_PROPERTY_VALUE(bDefeated, true);
}
