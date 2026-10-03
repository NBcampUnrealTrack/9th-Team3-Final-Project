#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterReaction.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

UknightGameplayAbility_MonsterReaction::UknightGameplayAbility_MonsterReaction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	// A second poise break during a down does not restart it.
	bRetriggerInstancedAbility = false;

	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_HitReact));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	CancelAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);
	BlockAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);

	StateTag = knightGameplayTags::State_HitReact_Knockdown;

	// Default: a down on poise break. Blueprint subclasses replace Triggers for other reactions.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = knightGameplayTags::Event_PoiseBroken;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UknightGameplayAbility_MonsterReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!ASC || !Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	KNIGHT_PERF_COUNT("Monster.Reactions", 1);

	if (StateTag.IsValid())
	{
		ASC->AddLooseGameplayTag(StateTag);
		bStateTagApplied = true;
	}

	// Stop the current path; the StateTree's reaction state keeps a new one from starting.
	if (AAIController* const Controller = Cast<AAIController>(Character->GetController()))
	{
		Controller->StopMovement();
	}
	Character->GetCharacterMovement()->StopMovementImmediately();

	if (HoldMontage)
	{
		UAbilityTask_PlayMontageAndWait* const HoldTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Hold"), HoldMontage);
		HoldTask->ReadyForActivation();
	}
	UAbilityTask_WaitDelay* const HoldDelay = UAbilityTask_WaitDelay::WaitDelay(this, HoldTime);
	HoldDelay->OnFinish.AddDynamic(this, &ThisClass::HandleHoldFinished);
	HoldDelay->ReadyForActivation();

	UE_LOG(LogknightAI, Log, TEXT("%s: %s started (%s)."), *GetNameSafe(Character), *GetName(),
		TriggerEventData ? *TriggerEventData->EventTag.ToString() : TEXT("tag trigger"));
}

void UknightGameplayAbility_MonsterReaction::HandleHoldFinished()
{
	if (RecoverMontage)
	{
		UAbilityTask_PlayMontageAndWait* const RecoverTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Recover"), RecoverMontage);
		RecoverTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->ReadyForActivation();
		return;
	}
	UAbilityTask_WaitDelay* const RecoverDelay = UAbilityTask_WaitDelay::WaitDelay(this, RecoverTime);
	RecoverDelay->OnFinish.AddDynamic(this, &ThisClass::HandleRecoverFinished);
	RecoverDelay->ReadyForActivation();
}

void UknightGameplayAbility_MonsterReaction::HandleRecoverFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MonsterReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Montage blend-out and completion can both call in; only the first end may restore poise.
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && bStateTagApplied)
	{
		ASC->RemoveLooseGameplayTag(StateTag);
	}
	bStateTagApplied = false;

	// Death cancels the reaction; a corpse keeps its broken poise.
	if (!bWasCancelled && bRestorePoise)
	{
		RestorePoise();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_MonsterReaction::RestorePoise() const
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->GetSet<UknightCoreAttributeSet>())
	{
		return;
	}
	// Base values, so the change is permanent; raising Poise above zero also re-arms the break event.
	const float NewMaxPoise = ASC->GetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute()) * (1.f + MaxPoiseGrowth);
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute(), NewMaxPoise);
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseAttribute(), NewMaxPoise);
}
