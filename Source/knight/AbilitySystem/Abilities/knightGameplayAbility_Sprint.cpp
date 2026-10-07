#include "AbilitySystem/Abilities/knightGameplayAbility_Sprint.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_Sprint::UknightGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	DashAbilityTag = knightGameplayTags::Ability_Boost_Dash;
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_InputBlocked);
}

void UknightGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastPressTime <= DoubleTapWindow)
	{
		// Double tap: dash, then normal speed until the next press.
		LastPressTime = -1.e9;
		SetRunning(false);
		if (UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(DashAbilityTag));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	LastPressTime = Now;
	SetRunning(true);
	UAbilityTask_WaitInputRelease* const Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	Task->ReadyForActivation();
}

void UknightGameplayAbility_Sprint::OnReleased(float TimeHeld)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetRunning(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_Sprint::SetRunning(bool bRun) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->SetWantsToRun(bRun);
	}
}
