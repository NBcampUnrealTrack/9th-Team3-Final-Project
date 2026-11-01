#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterStepBack.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/RootMotionSource.h"
#include "knight.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterTargetingComponent.h"

void UknightGameplayAbility_MonsterStepBack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->ReadyForActivation();

	if (StepDistance > 0.f && StepEndTime > StepStartTime)
	{
		UAbilityTask_WaitDelay* const DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, StepStartTime);
		DelayTask->OnFinish.AddDynamic(this, &ThisClass::StartStep);
		DelayTask->ReadyForActivation();
	}
}

void UknightGameplayAbility_MonsterStepBack::StartStep()
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	// 타깃 반대쪽으로 물러난다. 타깃이 없으면 등 뒤로.
	FVector Direction = Target ? (Monster->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	if (Direction.IsNearlyZero())
	{
		Direction = -Monster->GetActorForwardVector().GetSafeNormal2D();
	}
	const float Duration = StepEndTime - StepStartTime;
	UAbilityTask_ApplyRootMotionConstantForce* const StepTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("StepBack"), Direction, StepDistance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 0.f, true);
	StepTask->ReadyForActivation();
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s steps back %.0f cm over %.2f s."), *GetNameSafe(Monster), *GetName(), StepDistance, Duration);
}

void UknightGameplayAbility_MonsterStepBack::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
