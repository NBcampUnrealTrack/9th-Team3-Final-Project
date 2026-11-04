#include "AI/knightMonsterStateTreeNodes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

namespace knightMonsterStateTree
{
	/**
	 * 이동 행동(Ability.Move 태그: 뒷걸음, 물러나기)은 공격 후보에서 뺀다. 공격을 고르는 상태(Ability.Attack)가 옆걸음·뒷걸음을
	 * 확률로 섞지 않고, 공격 사이 상태가 이동 행동을 Ability.Move 태그로 따로 고르게 하기 위함이다.
	 * 이동 행동도 Ability.Attack을 함께 가진다(반응 어빌리티가 공격과 똑같이 끊고 막도록).
	 */
	static bool IsExcludedMove(const UGameplayAbility& Ability, const FGameplayTagContainer& RequestedTags)
	{
		return Ability.GetAssetTags().HasTag(knightGameplayTags::Ability_Move) && !RequestedTags.HasTag(knightGameplayTags::Ability_Move);
	}
}

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
		InstanceData.AbsAngleToTarget = FMath::Abs(InstanceData.AngleToTarget);
		const UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		InstanceData.bTargetAirborne = TargetASC && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Airborne);
	}
	else
	{
		InstanceData.Distance = 0.f;
		InstanceData.HeightDifference = 0.f;
		InstanceData.AngleToTarget = 0.f;
		InstanceData.AbsAngleToTarget = 0.f;
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

	if (const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr)
	{
		InstanceData.ChaseStopDistance = Definition->AirChaseStopDistance > 0.f && ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Airborne)
			? Definition->AirChaseStopDistance : Definition->ChaseStopDistance;
		InstanceData.RangedMinDistance = Definition->RangedMinDistance;
		InstanceData.RangedMaxDistance = Definition->RangedMaxDistance;
		InstanceData.RecoverRetreatChance = Definition->RecoverRetreatChance;
		InstanceData.SpacingMaxDistance = InstanceData.ChaseStopDistance + Definition->SpacingDistanceSlack;
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
	InstanceData.RestTime = 0.f;
	InstanceData.SucceedAtTime = -1.0;
	InstanceData.bMayFollowUp = false;
	InstanceData.StopWaitUntil = -1.0;
	const bool bStopWaitDone = InstanceData.bStopWaitDone;
	InstanceData.bStopWaitDone = false;

	// 달려오다 들어왔으면 걷는 속도까지 줄인 뒤에 고른다(Tick이 기다렸다가 다시 부른다).
	const AknightMonsterCharacterBase* const WaitingMonster = Cast<AknightMonsterCharacterBase>(InstanceData.Actor);
	const UknightMonsterMovementComponent* const Movement = WaitingMonster ? WaitingMonster->GetMonsterMovementComponent() : nullptr;
	if (!bStopWaitDone && InstanceData.MaxStopWaitTime > 0.f && Movement && Movement->IsSlowingFromRun())
	{
		UE_LOG(LogknightAI, Verbose, TEXT("%s: waits for the run to slow down (%.0f cm/s) before attacking."), *GetNameSafe(InstanceData.Actor), Movement->Velocity.Size2D());
		InstanceData.StopWaitUntil = Context.GetWorld()->GetTimeSeconds() + InstanceData.MaxStopWaitTime;
		return EStateTreeRunStatus::Running;
	}

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
		if (!Instance || knightMonsterStateTree::IsExcludedMove(*Instance, InstanceData.AbilityTags))
		{
			continue;
		}
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

	// 직전 공격 연속 금지. 이 상태의 후보만이 아니라 모든 몬스터 공격 중 마지막에 쓴 것을 뺀다(원거리 → 근접 상태가 바뀌어도 같다).
	// 그것 하나만 남으면 그대로 둔다. 쓸 공격이 없어 멈춰 서는 것보다 같은 공격을 한 번 더 쓰는 편이 낫다.
	const AknightMonsterCharacterBase* const MonsterActor = Cast<AknightMonsterCharacterBase>(InstanceData.Actor);
	const UknightMonsterDefinition* const Definition = MonsterActor ? MonsterActor->GetMonsterDefinition() : nullptr;
	if (Definition && Definition->bAvoidRepeatingLastAttack && Candidates.Num() > 1)
	{
		FGameplayAbilitySpecHandle LastHandle;
		double LastTime = -UE_BIG_NUMBER;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Spec.GetPrimaryInstance());
			if (Attack && Attack->GetLastActivationTime() > LastTime)
			{
				LastTime = Attack->GetLastActivationTime();
				LastHandle = Spec.Handle;
			}
		}
		const int32 LastIndex = Candidates.IndexOfByPredicate([LastHandle](const TPair<FGameplayAbilitySpecHandle, float>& Candidate) { return Candidate.Key == LastHandle; });
		if (LastIndex != INDEX_NONE)
		{
			TotalWeight -= Candidates[LastIndex].Value;
			Candidates.RemoveAtSwap(LastIndex);
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
		UE_LOG(LogknightAI, Verbose, TEXT("%s: [%s] activates %s."), *GetNameSafe(InstanceData.Actor), *InstanceData.AbilityTags.ToStringSimple(),
			Spec ? *GetNameSafe(Spec->Ability) : TEXT("none"));
		if (!Spec || !Spec->IsActive())
		{
			// 발동 도중에 이미 끝났다(즉발 기술).
			return EStateTreeRunStatus::Succeeded;
		}
		if (const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Spec->GetPrimaryInstance()))
		{
			if (Definition && InstanceData.bRestAfterAttack)
			{
				InstanceData.RestTime = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Phase2) ? Definition->Phase2AttackRestTime : Definition->AttackRestTime;
			}
			InstanceData.bMayFollowUp = Attack->HasFollowUps();
		}
		BindAbilityEnded(Context, InstanceData, Handle);
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

void FknightStateTreeActivateAbilityTask::BindAbilityEnded(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData,
	FGameplayAbilitySpecHandle Handle)
{
	UAbilitySystemComponent* const ASC = InstanceData.AbilitySystem.Get();
	if (InstanceData.EndedDelegateHandle.IsValid())
	{
		ASC->OnAbilityEnded.Remove(InstanceData.EndedDelegateHandle);
	}
	// 발동 후에 바인딩해서 EnterState가 아직 실행 중일 때 콜백이 Task를 끝내는 일이 없게 한다.
	// 쉬는 시간이나 연계가 있으면 정상 종료는 Tick이 맡는다(어빌리티가 끝난 걸 보고 연계를 쓰거나 RestTime 뒤에 성공).
	InstanceData.EndedDelegateHandle = ASC->OnAbilityEnded.AddLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Handle, bWaitInTick = InstanceData.RestTime > 0.f || InstanceData.bMayFollowUp](const FAbilityEndedData& Data)
		{
			if (Data.AbilitySpecHandle != Handle)
			{
				return;
			}
			if (Data.bWasCancelled)
			{
				WeakContext.FinishTask(EStateTreeFinishTaskType::Failed);
			}
			else if (!bWaitInTick)
			{
				WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
			}
		});
	InstanceData.ActiveHandle = Handle;
}

bool FknightStateTreeActivateAbilityTask::TryFollowUp(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData)
{
	InstanceData.bMayFollowUp = false;
	UAbilitySystemComponent* const ASC = InstanceData.AbilitySystem.Get();
	const FGameplayAbilitySpec* const Ended = ASC ? ASC->FindAbilitySpecFromHandle(InstanceData.ActiveHandle) : nullptr;
	const UknightGameplayAbility_MonsterAttack* const Attack = Ended ? Cast<UknightGameplayAbility_MonsterAttack>(Ended->GetPrimaryInstance()) : nullptr;
	if (!Attack)
	{
		return false;
	}
	const FknightMonsterAttackContext AttackContext = FknightMonsterAttackContext::Make(InstanceData.Actor, InstanceData.Target);
	const FGameplayAbilitySpecHandle Next = Attack->PickFollowUp(*ASC, AttackContext);
	if (!Next.IsValid() || !ASC->TryActivateAbility(Next))
	{
		UE_LOG(LogknightAI, Verbose, TEXT("%s: %s has no usable follow-up (%s, %.0f cm, %.0f deg)."), *GetNameSafe(InstanceData.Actor), *GetNameSafe(Attack),
			Next.IsValid() ? TEXT("activation blocked") : TEXT("none passes selection"), AttackContext.Distance, AttackContext.AngleToTarget);
		return false;
	}
	const FGameplayAbilitySpec* const Spec = ASC->FindAbilitySpecFromHandle(Next);
	UE_LOG(LogknightAI, Log, TEXT("%s: %s follows up with %s."), *GetNameSafe(InstanceData.Actor), *GetNameSafe(Attack),
		Spec ? *GetNameSafe(Spec->GetPrimaryInstance()) : TEXT("none"));
	if (!Spec || !Spec->IsActive())
	{
		return false;
	}
	BindAbilityEnded(Context, InstanceData, Next);
	return true;
}

EStateTreeRunStatus FknightStateTreeActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const double Now = Context.GetWorld()->GetTimeSeconds();
	if (InstanceData.StopWaitUntil >= 0.0)
	{
		const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(InstanceData.Actor);
		const UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
		if (Movement && Movement->IsSlowingFromRun() && Now < InstanceData.StopWaitUntil)
		{
			return EStateTreeRunStatus::Running;
		}
		// 다 줄었다. 줄이는 동안 거리가 바뀌었으니 지금 상황으로 다시 고른다.
		InstanceData.bStopWaitDone = true;
		return EnterState(Context, FStateTreeTransitionResult());
	}
	if (InstanceData.FailAtTime >= 0.0 && Now >= InstanceData.FailAtTime)
	{
		return EStateTreeRunStatus::Failed;
	}
	if ((InstanceData.RestTime > 0.f || InstanceData.bMayFollowUp) && InstanceData.ActiveHandle.IsValid())
	{
		if (InstanceData.SucceedAtTime < 0.0)
		{
			const UAbilitySystemComponent* const ASC = InstanceData.AbilitySystem.Get();
			const FGameplayAbilitySpec* const Spec = ASC ? ASC->FindAbilitySpecFromHandle(InstanceData.ActiveHandle) : nullptr;
			if (!Spec || !Spec->IsActive())
			{
				if (InstanceData.bMayFollowUp && TryFollowUp(Context, InstanceData))
				{
					return EStateTreeRunStatus::Running;
				}
				if (InstanceData.RestTime <= 0.f)
				{
					return EStateTreeRunStatus::Succeeded;
				}
				InstanceData.SucceedAtTime = Now + InstanceData.RestTime;
			}
		}
		else if (Now >= InstanceData.SucceedAtTime)
		{
			return EStateTreeRunStatus::Succeeded;
		}
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

namespace knightMonsterStateTree
{
	const FName TurnWarpTargetName(TEXT("MonsterTurn"));
	/** 타깃이 반대편으로 넘어가도 회전 몽타주가 이 각도보다 덜 돌지는 않는다(발 디딤과 회전이 크게 어긋나지 않게). */
	constexpr float TurnWarpMinAngle = 30.f;

	/** 회전 몽타주 재생 속도. 회전 구간 동안 Turn(도)을 돌 때 TurnMontageMaxSpeed를 넘지 않게 늦추고, TurnScale(탈진)을 곱한다. */
	static float GetTurnPlayRate(const UknightMonsterDefinition& Definition, float Turn, float TurnScale)
	{
		const float WarpDuration = Definition.TurnWarpEndTime - Definition.TurnWarpStartTime;
		float Rate = 1.f;
		if (Definition.TurnMontageMaxSpeed > 0.f && WarpDuration > 0.f && !FMath::IsNearlyZero(Turn))
		{
			Rate = FMath::Min(1.f, Definition.TurnMontageMaxSpeed * WarpDuration / FMath::Abs(Turn));
		}
		return Rate * TurnScale;
	}

	/**
	 * 지금 각도에서 고를 수 있는 옆·뒤 공격이 있는가. 옆·뒤 전용(MinAngle > 0)과 둘레 전체를 덮는 공격(MaxAngle 180, 예: 주변 범위)을
	 * 센다. 둘레 전체 공격은 타깃이 회전 몽타주가 필요할 만큼(TurnMontageMinAngle) 벗어났을 때만 센다. 조금 벗어났을 때는 짧게
	 * 조준하고 정면 공격을 고르는 게 맞다. Tags가 있으면 그 태그를 모두 가진 공격만 본다.
	 */
	static bool HasUsableSideAttack(const AknightMonsterCharacterBase& Monster, const AActor* Target, const FGameplayTagContainer& Tags)
	{
		UAbilitySystemComponent* const ASC = Monster.GetAbilitySystemComponent();
		if (!ASC || !Target)
		{
			return false;
		}
		const FknightMonsterAttackContext AttackContext = FknightMonsterAttackContext::Make(&Monster, Target);
		const UknightMonsterDefinition* const Definition = Monster.GetMonsterDefinition();
		const bool bNeedsTurnMontage = FMath::Abs(AttackContext.AngleToTarget) >= (Definition ? Definition->TurnMontageMinAngle : 45.f);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UGameplayAbility* const Instance = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
			const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Instance);
			const bool bCoversSides = Attack && !IsExcludedMove(*Attack, Tags)
				&& (Attack->GetSelection().MinAngle > 0.f || (Attack->GetSelection().MaxAngle >= 180.f && bNeedsTurnMontage));
			if (bCoversSides && (Tags.IsEmpty() || Attack->GetAssetTags().HasAll(Tags)) && Attack->GetSelectionWeight(AttackContext) > 0.f)
			{
				return true;
			}
		}
		return false;
	}
}

EStateTreeRunStatus FknightStateTreeFaceTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	if (!InstanceData.Monster || !InstanceData.Target)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.bSkipForSideAttacks && knightMonsterStateTree::HasUsableSideAttack(*InstanceData.Monster, InstanceData.Target, InstanceData.SideAttackTags))
	{
		UE_LOG(LogknightAI, Verbose, TEXT("%s: Face Target skipped, a side attack is usable."), *GetNameSafe(InstanceData.Monster));
		return EStateTreeRunStatus::Succeeded;
	}
	TryStartTurnMontage(InstanceData);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FknightStateTreeFaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UknightMonsterMovementComponent* const Movement = InstanceData.Monster ? InstanceData.Monster->GetMonsterMovementComponent() : nullptr;
	if (!Movement || !InstanceData.Target)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.TurnMontage.IsValid())
	{
		if (IsTurnMontagePlaying(InstanceData))
		{
			UpdateTurnWarpTarget(InstanceData);
			return EStateTreeRunStatus::Running;
		}
		// 몽타주가 끝났다. 그사이 등 뒤 공격을 쓸 수 있게 됐으면(타깃이 계속 뒤로 돈다) 멈추고 공격으로 넘긴다.
		// 아니면 많이 남았을 때 한 번 더 돌고, 적게 남았으면 보간 회전으로 맞춘다.
		InstanceData.TurnMontage.Reset();
		InstanceData.ElapsedTime = 0.f;
		if (InstanceData.bSkipForSideAttacks && knightMonsterStateTree::HasUsableSideAttack(*InstanceData.Monster, InstanceData.Target, InstanceData.SideAttackTags))
		{
			UE_LOG(LogknightAI, Verbose, TEXT("%s: Face Target ended between turns, a side attack is usable."), *GetNameSafe(InstanceData.Monster));
			return EStateTreeRunStatus::Succeeded;
		}
		if (TryStartTurnMontage(InstanceData))
		{
			return EStateTreeRunStatus::Running;
		}
	}
	// 허용 각도에 들어와도 관성 회전이 다 멈춘 뒤에 끝낸다. 남은 회전이 다음 공격 첫머리까지 이어지면 공격 중에 몸이 돌며 발이 미끄러진다.
	if (Movement->TurnTowards(InstanceData.Target->GetActorLocation(), DeltaTime, InstanceData.TurnRate, InstanceData.Tolerance)
		&& !Movement->IsTurning())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	InstanceData.ElapsedTime += DeltaTime;
	if (InstanceData.MaxTime > 0.f && InstanceData.ElapsedTime >= InstanceData.MaxTime)
	{
		UE_LOG(LogknightAI, Verbose, TEXT("%s: Face Target gave up after %.1fs."), *GetNameSafe(InstanceData.Monster), InstanceData.ElapsedTime);
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void FknightStateTreeFaceTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAnimMontage* const Montage = InstanceData.TurnMontage.Get();
	USkeletalMeshComponent* const Mesh = InstanceData.Monster ? InstanceData.Monster->GetMesh() : nullptr;
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	// 반응 몽타주가 끊었으면 이미 멈춰 있다. 다른 전이로 상태를 벗어날 때만 여기서 멈춘다.
	if (Montage && AnimInstance && AnimInstance->Montage_IsActive(Montage) && IsTurnMontagePlaying(InstanceData))
	{
		AnimInstance->Montage_Stop(0.25f, Montage);
	}
	InstanceData.TurnMontage.Reset();
}

bool FknightStateTreeFaceTargetTask::TryStartTurnMontage(FInstanceDataType& InstanceData)
{
	AknightMonsterCharacterBase* const Monster = InstanceData.Monster;
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
	UMotionWarpingComponent* const MotionWarping = Monster ? Monster->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	UAnimInstance* const AnimInstance = Monster && Monster->GetMesh() ? Monster->GetMesh()->GetAnimInstance() : nullptr;
	if (!Definition || !Movement || !MotionWarping || !AnimInstance || !InstanceData.Target || Movement->IsMovementLocked())
	{
		return false;
	}
	const FVector ToTarget = (InstanceData.Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
	const float Delta = FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - Monster->GetActorRotation().Yaw);
	UAnimMontage* const Montage = Delta < 0.f ? Definition->TurnLeftMontage : Definition->TurnRightMontage;
	if (!Montage || ToTarget.IsNearlyZero() || FMath::Abs(Delta) < Definition->TurnMontageMinAngle)
	{
		return false;
	}
	// 큰 각도는 최대 회전 속도에 맞춰 천천히, 탈진처럼 느려진 상태면 보간 회전과 같은 비율로 더 천천히 돈다.
	const float FirstTurn = FMath::Clamp(FMath::Abs(Delta), knightMonsterStateTree::TurnWarpMinAngle, Definition->TurnMontageMaxAngle);
	const float PlayRate = knightMonsterStateTree::GetTurnPlayRate(*Definition, FirstTurn, Movement->GetTurnScale());
	if (PlayRate <= UE_KINDA_SMALL_NUMBER || AnimInstance->Montage_Play(Montage, PlayRate) <= 0.f)
	{
		return false;
	}
	InstanceData.TurnMontage = Montage;
	InstanceData.TurnStartYaw = Monster->GetActorRotation().Yaw;
	// 바로 뒤(±180°)라도 여기서 고른 방향으로 끝까지 돈다.
	InstanceData.TurnAmount = Delta;
	// 몽타주의 회전(90°)을 회전 구간 동안 실제로 돌 각도에 맞춘다. 이동은 애니메이션 그대로 둔다.
	URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(MotionWarping, Montage, Definition->TurnWarpStartTime, Definition->TurnWarpEndTime,
		knightMonsterStateTree::TurnWarpTargetName, EWarpPointAnimProvider::None, FTransform::Identity, NAME_None,
		/*bInWarpTranslation*/ false, /*bInIgnoreZAxis*/ true, /*bInWarpRotation*/ true);
	UpdateTurnWarpTarget(InstanceData);
	UE_LOG(LogknightAI, Verbose, TEXT("%s: turn montage %s for %.0f deg at %.2fx."), *GetNameSafe(Monster), *GetNameSafe(Montage), Delta, PlayRate);
	return true;
}

bool FknightStateTreeFaceTargetTask::IsTurnMontagePlaying(const FInstanceDataType& InstanceData)
{
	const UAnimInstance* const AnimInstance = InstanceData.Monster && InstanceData.Monster->GetMesh() ? InstanceData.Monster->GetMesh()->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* const MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(InstanceData.TurnMontage.Get()) : nullptr;
	// 블렌드 아웃(몽타주의 BlendOutTriggerTime)에 들어가면 끝난 것으로 본다. 정지 동작의 꼬리를 기다리지 않는다.
	return MontageInstance && !MontageInstance->IsStopped();
}

void FknightStateTreeFaceTargetTask::UpdateTurnWarpTarget(FInstanceDataType& InstanceData)
{
	AknightMonsterCharacterBase* const Monster = InstanceData.Monster;
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	UMotionWarpingComponent* const MotionWarping = Monster ? Monster->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!Definition || !MotionWarping || !InstanceData.Target)
	{
		return;
	}
	// 도는 동안 움직인 타깃을 따라가되, 몽타주 방향과 최대 각도는 넘지 않는다. 각도는 직전 값에서 이어서 재야
	// 타깃이 바로 뒤를 지날 때 +179°가 -179°로 바뀌어 반대 방향 최솟값으로 잘리는 일이 없다.
	const FVector ToTarget = (InstanceData.Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
	const float RawTurn = FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - InstanceData.TurnStartYaw);
	float Turn = InstanceData.TurnAmount + FRotator::NormalizeAxis(RawTurn - InstanceData.TurnAmount);
	const float MaxAngle = Definition->TurnMontageMaxAngle;
	Turn = InstanceData.TurnMontage.Get() == Definition->TurnLeftMontage
		? FMath::Clamp(Turn, -MaxAngle, -knightMonsterStateTree::TurnWarpMinAngle)
		: FMath::Clamp(Turn, knightMonsterStateTree::TurnWarpMinAngle, MaxAngle);
	InstanceData.TurnAmount = Turn;
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(knightMonsterStateTree::TurnWarpTargetName, Monster->GetActorLocation(),
		FRotator(0.f, InstanceData.TurnStartYaw + Turn, 0.f));

	// 타깃이 움직여 돌 각도가 바뀌면 재생 속도도 다시 맞춘다.
	UAnimInstance* const AnimInstance = Monster->GetMesh() ? Monster->GetMesh()->GetAnimInstance() : nullptr;
	const UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent();
	if (AnimInstance && Movement && InstanceData.TurnMontage.IsValid())
	{
		AnimInstance->Montage_SetPlayRate(InstanceData.TurnMontage.Get(), knightMonsterStateTree::GetTurnPlayRate(*Definition, Turn, Movement->GetTurnScale()));
	}
}

// Knight Rest

EStateTreeRunStatus FknightStateTreeRestTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime = InstanceData.Duration;
	if (InstanceData.RemainingTime <= 0.f && InstanceData.Monster)
	{
		const UknightMonsterDefinition* const Definition = InstanceData.Monster->GetMonsterDefinition();
		const UAbilitySystemComponent* const ASC = InstanceData.Monster->GetAbilitySystemComponent();
		if (Definition)
		{
			InstanceData.RemainingTime = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Phase2)
				? Definition->Phase2AttackRestTime : Definition->AttackRestTime;
		}
	}
	UE_LOG(LogknightAI, Verbose, TEXT("%s: rests %.2fs."), *GetNameSafe(InstanceData.Monster), InstanceData.RemainingTime);
	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FknightStateTreeRestTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;
	if (InstanceData.RemainingTime <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	UknightMonsterMovementComponent* const Movement = InstanceData.Monster ? InstanceData.Monster->GetMonsterMovementComponent() : nullptr;
	if (Movement && InstanceData.Target && InstanceData.TurnRate > 0.f)
	{
		const FVector ToTarget = InstanceData.Target->GetActorLocation() - InstanceData.Monster->GetActorLocation();
		const float Angle = FMath::Abs(FRotator::NormalizeAxis(ToTarget.GetSafeNormal2D().Rotation().Yaw - InstanceData.Monster->GetActorRotation().Yaw));
		if (Angle <= InstanceData.MaxTurnAngle)
		{
			Movement->TurnTowards(InstanceData.Target->GetActorLocation(), DeltaTime, InstanceData.TurnRate, InstanceData.Tolerance);
		}
	}
	return EStateTreeRunStatus::Running;
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

// Knight Can Use Ability

bool FknightStateTreeCanUseAbilityCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	bool bResult = false;
	if (ASC && !InstanceData.AbilityTags.IsEmpty())
	{
		FknightMonsterAttackContext AttackContext = FknightMonsterAttackContext::Make(InstanceData.Actor, InstanceData.Target);
		AttackContext.bIgnoreAngle = InstanceData.bIgnoreAngle;
		TArray<FGameplayAbilitySpec*> Specs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(InstanceData.AbilityTags, Specs, true);
		for (const FGameplayAbilitySpec* const Spec : Specs)
		{
			if (!Spec || Spec->IsActive())
			{
				continue;
			}
			// Activate Ability 태스크와 같은 판정: 몬스터 공격은 Selection 가중치, 나머지는 쓸 수 있으면 통과.
			const UGameplayAbility* const Instance = Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get();
			if (!Instance || knightMonsterStateTree::IsExcludedMove(*Instance, InstanceData.AbilityTags))
			{
				continue;
			}
			const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Instance);
			if (!Attack || Attack->GetSelectionWeight(AttackContext) > 0.f)
			{
				bResult = true;
				break;
			}
		}
	}
	return InstanceData.bInvert ? !bResult : bResult;
}
