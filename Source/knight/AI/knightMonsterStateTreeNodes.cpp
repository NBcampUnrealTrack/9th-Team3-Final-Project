#include "AI/knightMonsterStateTreeNodes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

// Knight Monster Target

void FknightStateTreeMonsterTargetEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	Update(Context.GetInstanceData(*this));
}

void FknightStateTreeMonsterTargetEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	KNIGHT_PERF_SCOPE("Monster.Evaluator");
	KNIGHT_PERF_COUNT("Monster.EvaluatorUpdates", 1);
	Update(Context.GetInstanceData(*this));
}

void FknightStateTreeMonsterTargetEvaluator::Update(FInstanceDataType& InstanceData)
{
	const AknightMonsterCharacterBase* const Monster = InstanceData.Monster;
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;

	InstanceData.Target = Target;
	InstanceData.bHasTarget = Target != nullptr;
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		InstanceData.Distance = ToTarget.Size2D();
		InstanceData.HeightDifference = ToTarget.Z;
		const float TargetYaw = ToTarget.GetSafeNormal2D().Rotation().Yaw;
		InstanceData.AngleToTarget = FRotator::NormalizeAxis(TargetYaw - Monster->GetActorRotation().Yaw);
		const UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		InstanceData.bTargetAirborne = TargetASC && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Airborne);
	}
	else
	{
		InstanceData.Distance = 0.f;
		InstanceData.HeightDifference = 0.f;
		InstanceData.AngleToTarget = 0.f;
		InstanceData.bTargetAirborne = false;
	}

	const UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	if (ASC)
	{
		bool bFound = false;
		const float Health = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound);
		const float MaxHealth = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound);
		InstanceData.HealthPercent = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
		InstanceData.bEnraged = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
		InstanceData.bExhausted = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted);
		InstanceData.bIsReacting = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting);
	}
}

// Knight Activate Ability

EStateTreeRunStatus FknightStateTreeActivateAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	KNIGHT_PERF_SCOPE("Monster.SelectAbility");
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ActiveHandle = FGameplayAbilitySpecHandle();
	InstanceData.EndedDelegateHandle.Reset();
	InstanceData.FailAtTime = -1.0;

	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	InstanceData.AbilitySystem = ASC;
	if (!ASC || InstanceData.AbilityTags.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 가중치가 붙은 후보 목록. 몬스터 공격은 타깃을 보고 스스로 가중치를 정하고, 나머지는 1로 친다.
	const FknightMonsterAttackContext AttackContext = FknightMonsterAttackContext::Make(InstanceData.Actor, InstanceData.Target);
	TArray<FGameplayAbilitySpec*> Specs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(InstanceData.AbilityTags, Specs, true);
	TArray<TPair<FGameplayAbilitySpecHandle, float>> Candidates;
	float TotalWeight = 0.f;
	for (const FGameplayAbilitySpec* const Spec : Specs)
	{
		if (!Spec || Spec->IsActive())
		{
			continue;
		}
		float Weight = 1.f;
		const UGameplayAbility* const Instance = Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get();
		if (const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Instance))
		{
			Weight = Attack->GetSelectionWeight(AttackContext);
		}
		if (Weight > 0.f)
		{
			Candidates.Emplace(Spec->Handle, Weight);
			TotalWeight += Weight;
		}
	}

	// 가중치로 뽑는다. 발동을 거부한 후보(차단, 비용 부족)는 빼고 다시 뽑는다.
	while (Candidates.Num() > 0)
	{
		float Roll = FMath::FRandRange(0.f, TotalWeight);
		int32 Picked = Candidates.Num() - 1;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Roll -= Candidates[Index].Value;
			if (Roll <= 0.f)
			{
				Picked = Index;
				break;
			}
		}
		const FGameplayAbilitySpecHandle Handle = Candidates[Picked].Key;
		TotalWeight -= Candidates[Picked].Value;
		Candidates.RemoveAtSwap(Picked);
		if (!ASC->TryActivateAbility(Handle))
		{
			continue;
		}
		KNIGHT_PERF_COUNT("Monster.AbilitiesActivated", 1);
		const FGameplayAbilitySpec* const Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->IsActive())
		{
			// 발동 도중에 이미 끝났다(즉발 기술).
			return EStateTreeRunStatus::Succeeded;
		}
		// 발동 후에 바인딩해서 EnterState가 아직 실행 중일 때 콜백이 Task를 끝내는 일이 없게 한다.
		InstanceData.EndedDelegateHandle = ASC->OnAbilityEnded.AddLambda(
			[WeakContext = Context.MakeWeakExecutionContext(), Handle](const FAbilityEndedData& Data)
			{
				if (Data.AbilitySpecHandle == Handle)
				{
					WeakContext.FinishTask(Data.bWasCancelled ? EStateTreeFinishTaskType::Failed : EStateTreeFinishTaskType::Succeeded);
				}
			});
		InstanceData.ActiveHandle = Handle;
		return EStateTreeRunStatus::Running;
	}

	KNIGHT_PERF_COUNT("Monster.AbilitySelectFailed", 1);
	UE_LOG(LogknightAI, Verbose, TEXT("%s: no usable ability for %s."), *GetNameSafe(InstanceData.Actor), *InstanceData.AbilityTags.ToStringSimple());
	if (InstanceData.RetryDelay > 0.f)
	{
		// 바로 실패해서 즉시 재진입하지 않도록 잠시 상태를 유지한다.
		InstanceData.FailAtTime = Context.GetWorld()->GetTimeSeconds() + InstanceData.RetryDelay;
		return EStateTreeRunStatus::Running;
	}
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FknightStateTreeActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.FailAtTime >= 0.0 && Context.GetWorld()->GetTimeSeconds() >= InstanceData.FailAtTime)
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FknightStateTreeActivateAbilityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAbilitySystemComponent* const ASC = InstanceData.AbilitySystem.Get();
	if (ASC)
	{
		if (InstanceData.EndedDelegateHandle.IsValid())
		{
			ASC->OnAbilityEnded.Remove(InstanceData.EndedDelegateHandle);
		}
		if (InstanceData.bCancelOnExit && InstanceData.ActiveHandle.IsValid())
		{
			const FGameplayAbilitySpec* const Spec = ASC->FindAbilitySpecFromHandle(InstanceData.ActiveHandle);
			if (Spec && Spec->IsActive())
			{
				ASC->CancelAbilityHandle(InstanceData.ActiveHandle);
			}
		}
	}
	InstanceData.EndedDelegateHandle.Reset();
	InstanceData.ActiveHandle = FGameplayAbilitySpecHandle();
}

// Knight Face Target

EStateTreeRunStatus FknightStateTreeFaceTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	return InstanceData.Monster && InstanceData.Target ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FknightStateTreeFaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UknightMonsterMovementComponent* const Movement = InstanceData.Monster ? InstanceData.Monster->GetMonsterMovementComponent() : nullptr;
	if (!Movement || !InstanceData.Target)
	{
		return EStateTreeRunStatus::Failed;
	}
	return Movement->TurnTowards(InstanceData.Target->GetActorLocation(), DeltaTime, InstanceData.TurnRate, InstanceData.Tolerance)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

// Knight Has Gameplay Tag

bool FknightStateTreeHasGameplayTagCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	bool bResult = false;
	if (ASC && !InstanceData.Tags.IsEmpty())
	{
		bResult = InstanceData.bRequireAll ? ASC->HasAllMatchingGameplayTags(InstanceData.Tags) : ASC->HasAnyMatchingGameplayTags(InstanceData.Tags);
	}
	return InstanceData.bInvert ? !bResult : bResult;
}
