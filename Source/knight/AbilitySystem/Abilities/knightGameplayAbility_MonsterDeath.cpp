#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterDeath.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_MonsterDeath::UknightGameplayAbility_MonsterDeath(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Death));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = knightGameplayTags::Event_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UknightGameplayAbility_MonsterDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (DeathMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DeathMontage);
		MontageTask->ReadyForActivation();
	}
}
