#include "AbilitySystem/Abilities/knightGameplayAbility.h"

#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility::UknightGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UknightAbilitySystemComponent* UknightGameplayAbility::GetKnightAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UknightAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

void UknightGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	// Abilities granted before InitAbilityActorInfo have no avatar yet; the ASC retries once it is set.
	if (ActivationPolicy != EknightAbilityActivationPolicy::OnSpawn || !ActorInfo || !ActorInfo->AvatarActor.IsValid() || Spec.IsActive())
	{
		return;
	}
	if (UAbilitySystemComponent* const ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->TryActivateAbility(Spec.Handle);
	}
}

bool UknightGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (ASC && ASC->IsActivationGroupBlocked(ActivationGroup))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(knightGameplayTags::Ability_ActivateFail_ActivationGroup);
		}
		return false;
	}
	return true;
}

void UknightGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}

bool UknightGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(&AbilitySystemComponent);
	if (!ASC)
	{
		return true;
	}
	FGameplayTagContainer RequiredTags;
	FGameplayTagContainer BlockedTags;
	ASC->GetAdditionalActivationTagRequirements(GetAssetTags(), RequiredTags, BlockedTags);
	if (RequiredTags.IsEmpty() && BlockedTags.IsEmpty())
	{
		return true;
	}

	const FGameplayTagContainer& OwnedTags = AbilitySystemComponent.GetOwnedGameplayTags();
	if (!BlockedTags.IsEmpty() && OwnedTags.HasAny(BlockedTags))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag);
			OptionalRelevantTags->AppendMatchingTags(OwnedTags, BlockedTags);
		}
		return false;
	}
	if (!RequiredTags.IsEmpty() && !OwnedTags.HasAll(RequiredTags))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsMissingTag);
		}
		return false;
	}
	return true;
}
