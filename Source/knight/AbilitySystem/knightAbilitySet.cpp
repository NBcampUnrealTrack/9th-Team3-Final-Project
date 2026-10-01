#include "AbilitySystem/knightAbilitySet.h"

#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "knight.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "knightAbilitySet"

void FknightAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FknightAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FknightAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (Set)
	{
		GrantedAttributeSets.Add(Set);
	}
}

void FknightAbilitySet_GrantedHandles::TakeFromAbilitySystem(UknightAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		ASC->ClearAbility(Handle);
	}
	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	for (UAttributeSet* const Set : GrantedAttributeSets)
	{
		ASC->RemoveSpawnedAttribute(Set);
	}
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

void UknightAbilitySet::GiveToAbilitySystem(UknightAbilitySystemComponent* ASC, FknightAbilitySet_GrantedHandles* OutGrantedHandles,
	UObject* SourceObject, int32 LevelOffset) const
{
	if (!ASC)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s: GiveToAbilitySystem called without an AbilitySystemComponent."), *GetName());
		return;
	}

	// Attribute sets first, so effects below have attributes to modify.
	for (int32 Index = 0; Index < GrantedAttributes.Num(); ++Index)
	{
		const FknightAbilitySet_AttributeSet& Entry = GrantedAttributes[Index];
		if (!Entry.AttributeSet)
		{
			UE_LOG(LogknightAbility, Error, TEXT("%s: GrantedAttributes[%d] is empty."), *GetName(), Index);
			continue;
		}
		UAttributeSet* const NewSet = NewObject<UAttributeSet>(ASC->GetOwner(), Entry.AttributeSet);
		ASC->AddAttributeSetSubobject(NewSet);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		const FknightAbilitySet_GameplayAbility& Entry = GrantedGameplayAbilities[Index];
		if (!Entry.Ability)
		{
			UE_LOG(LogknightAbility, Error, TEXT("%s: GrantedGameplayAbilities[%d] is empty."), *GetName(), Index);
			continue;
		}
		FGameplayAbilitySpec Spec(Entry.Ability, Entry.AbilityLevel + LevelOffset, INDEX_NONE, SourceObject);
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(Handle);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		const FknightAbilitySet_GameplayEffect& Entry = GrantedGameplayEffects[Index];
		if (!Entry.GameplayEffect)
		{
			UE_LOG(LogknightAbility, Error, TEXT("%s: GrantedGameplayEffects[%d] is empty."), *GetName(), Index);
			continue;
		}
		const UGameplayEffect* const EffectCDO = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(EffectCDO, Entry.EffectLevel + LevelOffset, ASC->MakeEffectContext());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UknightAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		if (!GrantedGameplayAbilities[Index].Ability)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyAbility", "GrantedGameplayAbilities[{0}] has no ability class."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		if (!GrantedGameplayEffects[Index].GameplayEffect)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyEffect", "GrantedGameplayEffects[{0}] has no effect class."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	for (int32 Index = 0; Index < GrantedAttributes.Num(); ++Index)
	{
		if (!GrantedAttributes[Index].AttributeSet)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptySet", "GrantedAttributes[{0}] has no attribute set class."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
