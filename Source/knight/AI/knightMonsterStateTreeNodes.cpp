#include "AI/knightMonsterStateTreeNodes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Abilities/GameplayAbility.h"
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
	}
}

// Knight Activate Ability

EStateTreeRunStatus FknightStateTreeActivateAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	KNIGHT_PERF_SCOPE("Monster.SelectAbility");
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ActiveHandle = FGameplayAbilitySpecHandle();
	InstanceData.EndedDelegateHandle.Reset();

	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	InstanceData.AbilitySystem = ASC;
	if (!ASC || InstanceData.AbilityTags.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	TArray<FGameplayAbilitySpec*> Specs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(InstanceData.AbilityTags, Specs, true);
	TArray<FGameplayAbilitySpecHandle> Candidates;
	for (const FGameplayAbilitySpec* const Spec : Specs)
	{
		if (Spec && !Spec->IsActive())
		{
			Candidates.Add(Spec->Handle);
		}
	}
	// Random order gives a varied move set when several moves share the same tags.
	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		Candidates.Swap(Index, FMath::RandRange(0, Index));
	}

	for (const FGameplayAbilitySpecHandle& Handle : Candidates)
	{
		if (!ASC->TryActivateAbility(Handle))
		{
			continue;
		}
		KNIGHT_PERF_COUNT("Monster.AbilitiesActivated", 1);
		const FGameplayAbilitySpec* const Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->IsActive())
		{
			// Ended within its own activation (an instant move).
			return EStateTreeRunStatus::Succeeded;
		}
		// Bound after activation so the callback never finishes the task while EnterState is still running.
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
	UE_LOG(LogknightAI, Verbose, TEXT("%s: no activatable ability for %s."), *GetNameSafe(InstanceData.Actor), *InstanceData.AbilityTags.ToStringSimple());
	return EStateTreeRunStatus::Failed;
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
