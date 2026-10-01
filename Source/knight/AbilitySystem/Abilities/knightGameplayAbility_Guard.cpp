#include "AbilitySystem/Abilities/knightGameplayAbility_Guard.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_Guard::UknightGameplayAbility_Guard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Guard));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Guarding);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

void UknightGameplayAbility_Guard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ParryWindow > 0.f)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Parrying);
			bParryTagApplied = true;
		}
		UAbilityTask_WaitDelay* const ParryTask = UAbilityTask_WaitDelay::WaitDelay(this, ParryWindow);
		ParryTask->OnFinish.AddDynamic(this, &ThisClass::OnParryWindowEnded);
		ParryTask->ReadyForActivation();
	}

	if (GuardMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardMontage);
		MontageTask->ReadyForActivation();
	}

	// Already released (a tap): the guard still lasts through the parry window.
	UAbilityTask_WaitInputRelease* const ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnGuardReleased);
	ReleaseTask->ReadyForActivation();
}

void UknightGameplayAbility_Guard::OnParryWindowEnded()
{
	RemoveParryTag();
	// A tap-parry ends once its window closes if the input is no longer held.
	if (const FGameplayAbilitySpec* const Spec = GetCurrentAbilitySpec())
	{
		if (!Spec->InputPressed)
		{
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		}
	}
}

void UknightGameplayAbility_Guard::OnGuardReleased(float TimeHeld)
{
	if (!bParryTagApplied)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_Guard::RemoveParryTag()
{
	if (bParryTagApplied)
	{
		bParryTagApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Parrying);
		}
	}
}

void UknightGameplayAbility_Guard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveParryTag();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
