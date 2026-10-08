#include "AbilitySystem/Abilities/knightGameplayAbility_Guard.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
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

	if (ParryWindow > 0.f && ParrySucceededEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* const CounterTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ParrySucceededEventTag, nullptr, true, true);
		CounterTask->EventReceived.AddDynamic(this, &ThisClass::OnParrySucceeded);
		CounterTask->ReadyForActivation();
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
		// 가드 모션은 행동 속도 배속으로 재생한다. 패리 판정 시간(ParryWindow)은 난이도가 바뀌지 않게 그대로 둔다. (2026-10-08)
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardMontage, GetActionSpeed());
		MontageTask->ReadyForActivation();
	}

	// With a parry window the release is checked when the window closes (a tap still lasts through it, and a
	// second press during the window keeps the guard up until that press is released).
	if (ParryWindow <= 0.f)
	{
		WaitForRelease();
	}
}

void UknightGameplayAbility_Guard::WaitForRelease()
{
	UAbilityTask_WaitInputRelease* const ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnGuardReleased);
	ReleaseTask->ReadyForActivation();
}

void UknightGameplayAbility_Guard::OnParrySucceeded(FGameplayEventData Payload)
{
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		UWorld* const World = GetWorld();
		if (ParryCounterTag.IsValid() && ParryCounterWindow > 0.f && World)
		{
			// The window outlives this ability, so the timer belongs to the ASC, not to the guard.
			ASC->AddLooseGameplayTag(ParryCounterTag);
			const FGameplayTag Tag = ParryCounterTag;
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(ASC, [ASC, Tag]()
			{
				ASC->RemoveLooseGameplayTag(Tag);
			}), ParryCounterWindow, false);
		}
		if (ParryCounterEffect)
		{
			ASC->ApplyGameplayEffectToSelf(ParryCounterEffect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
		}
	}
	// End the guard so the attack key can start the counter right away.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_Guard::OnParryWindowEnded()
{
	RemoveParryTag();
	// A tap-parry ends once its window closes if the input is no longer held; otherwise guard until release.
	// Waiting for the release only now also covers a double click: the first release falls inside the window,
	// and the second press is still the one being held here.
	const FGameplayAbilitySpec* const Spec = GetCurrentAbilitySpec();
	if (!Spec || !Spec->InputPressed)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}
	WaitForRelease();
}

void UknightGameplayAbility_Guard::OnGuardReleased(float TimeHeld)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
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
