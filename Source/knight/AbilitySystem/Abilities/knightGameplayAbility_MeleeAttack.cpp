#include "AbilitySystem/Abilities/knightGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Combat/knightCombatLibrary.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightGameplayAbility_MeleeAttack::UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

void UknightGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!AttackMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no AttackMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UknightCombatLibrary::SetWarpTargetToLockedTarget(GetAvatarActorFromActorInfo(), WarpTargetName, MaxWarpDistance);

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, PlayRate, StartSection, true, RootMotionScale);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();
}

void UknightGameplayAbility_MeleeAttack::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MeleeAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}
	const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, HitParams);
	OnHitResolved(*Hit, Outcome);
}
