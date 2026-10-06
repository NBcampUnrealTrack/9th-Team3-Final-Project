#include "AbilitySystem/Abilities/knightGameplayAbility_Death.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightGameplayAbility_Death::UknightGameplayAbility_Death(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Death));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = knightGameplayTags::Event_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UknightGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The hit reaction is still running here: the character cancels it only after Event.Death is handled.
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bKnockedDown = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown);
	UAnimMontage* const Montage = bKnockedDown && KnockdownDeathMontage ? KnockdownDeathMontage.Get() : DeathMontage.Get();
	if (!Montage)
	{
		UE_LOG(LogknightAbility, Warning, TEXT("%s: no death montage; the Knight stays in its current pose."), *GetName());
		return;
	}

	// Not stopped when the ability ends, so the last frame holds even if the ability is cancelled later.
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds*/ false);
	Task->ReadyForActivation();
	UE_LOG(LogknightAbility, Log, TEXT("%s: playing %s%s."), *GetName(), *GetNameSafe(Montage), bKnockedDown ? TEXT(" (knocked down)") : TEXT(""));
}
