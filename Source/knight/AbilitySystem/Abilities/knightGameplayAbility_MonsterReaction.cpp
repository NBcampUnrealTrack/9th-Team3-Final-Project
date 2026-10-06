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
	// 다운 중에 Poise가 다시 깨져도 다운을 처음부터 다시 시작하지 않는다.
	bRetriggerInstancedAbility = false;

	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_HitReact));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	CancelAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);
	BlockAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);

	StateTag = knightGameplayTags::State_HitReact_Knockdown;

	// 기본값은 Poise가 깨질 때의 다운이다. 다른 반응은 Blueprint 하위 클래스에서 Triggers를 바꾼다.
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

	// 현재 이동 경로를 멈춘다. 새 경로가 시작되지 않게 막는 것은 StateTree의 반응 상태가 맡는다.
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
	// 몽타주 블렌드 아웃과 완료가 둘 다 호출될 수 있으므로 처음 종료될 때만 Poise를 회복한다.
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

	// 사망하면 반응이 취소되며, 시체는 깨진 Poise를 그대로 둔다.
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
	// 기본값(Base)을 바꾸므로 변경이 영구적이다. Poise가 0보다 커지면 Poise 붕괴 이벤트도 다시 발생할 수 있게 된다.
	const float NewMaxPoise = ASC->GetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute()) * (1.f + MaxPoiseGrowth);
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute(), NewMaxPoise);
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseAttribute(), NewMaxPoise);
}
