#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/World.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"

namespace knightMonsterAttack
{
	constexpr float StartTurnInterval = 1.f / 60.f;
	constexpr float StartTurnTolerance = 3.f;
}

FknightMonsterAttackContext FknightMonsterAttackContext::Make(const AActor* Monster, const AActor* Target)
{
	FknightMonsterAttackContext Context;
	Context.MonsterASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Monster);
	Context.WorldTime = Monster && Monster->GetWorld() ? Monster->GetWorld()->GetTimeSeconds() : 0.0;
	if (const UknightCoreAttributeSet* const CoreSet = Context.MonsterASC ? Context.MonsterASC->GetSet<UknightCoreAttributeSet>() : nullptr)
	{
		Context.TimeSinceHit = static_cast<float>(Context.WorldTime - CoreSet->GetLastHitTime());
	}
	if (Monster && Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		Context.bHasTarget = true;
		Context.Distance = ToTarget.Size2D();
		Context.AngleToTarget = FRotator::NormalizeAxis(ToTarget.GetSafeNormal2D().Rotation().Yaw - Monster->GetActorRotation().Yaw);
		const UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		Context.bTargetAirborne = TargetASC && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Airborne);
		const AknightMonsterCharacterBase* const MonsterCharacter = Cast<AknightMonsterCharacterBase>(Monster);
		const UknightMonsterTargetingComponent* const Targeting = MonsterCharacter ? MonsterCharacter->GetTargetingComponent() : nullptr;
		Context.TargetBehindTime = Targeting && Targeting->GetCurrentTarget() == Target ? Targeting->GetTargetBehindTime() : 0.f;
	}
	return Context;
}

UknightGameplayAbility_MonsterAttack::UknightGameplayAbility_MonsterAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 입력이 아니라 몬스터의 StateTree가 발동한다.
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

float UknightGameplayAbility_MonsterAttack::GetSelectionWeight(const FknightMonsterAttackContext& Context) const
{
	// 연계 전용은 PickFollowUp(GetSelectionWeightInternal 직접 호출)에서만 고른다.
	if (Selection.bOnlyAsFollowUp)
	{
		return 0.f;
	}
	return GetSelectionWeightInternal(Context, true);
}

FGameplayAbilitySpecHandle UknightGameplayAbility_MonsterAttack::PickFollowUp(UAbilitySystemComponent& ASC, const FknightMonsterAttackContext& Context) const
{
	if (!FollowUpAttacks.IsEmpty() && ASC.HasAllMatchingGameplayTags(FollowUpRequiredTags) && FMath::FRand() <= FollowUpChance)
	{
		const FGameplayAbilitySpecHandle FollowUp = PickWeighted(ASC, Context, FollowUpAttacks);
		if (FollowUp.IsValid())
		{
			return FollowUp;
		}
	}
	// 연계가 없으면 공격 후 이동(물러나기 등)을 본다. 각자 Selection 조건(맞지 않은 시간 등)으로 걸러진다.
	return PickWeighted(ASC, Context, AfterAttackMoves);
}

FGameplayAbilitySpecHandle UknightGameplayAbility_MonsterAttack::PickWeighted(UAbilitySystemComponent& ASC, const FknightMonsterAttackContext& Context,
	const TArray<TSubclassOf<UknightGameplayAbility_MonsterAttack>>& Classes) const
{
	TArray<TPair<FGameplayAbilitySpecHandle, float>> Candidates;
	float TotalWeight = 0.f;
	for (const TSubclassOf<UknightGameplayAbility_MonsterAttack>& AttackClass : Classes)
	{
		const FGameplayAbilitySpec* const Spec = AttackClass ? ASC.FindAbilitySpecFromClass(AttackClass) : nullptr;
		const UknightGameplayAbility_MonsterAttack* const Attack = Spec && !Spec->IsActive()
			? Cast<UknightGameplayAbility_MonsterAttack>(Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get()) : nullptr;
		const float Weight = Attack ? Attack->GetSelectionWeightInternal(Context, false) : 0.f;
		if (Weight > 0.f)
		{
			Candidates.Emplace(Spec->Handle, Weight);
			TotalWeight += Weight;
		}
	}
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const TPair<FGameplayAbilitySpecHandle, float>& Candidate : Candidates)
	{
		Roll -= Candidate.Value;
		if (Roll <= 0.f)
		{
			return Candidate.Key;
		}
	}
	return Candidates.IsEmpty() ? FGameplayAbilitySpecHandle() : Candidates.Last().Key;
}

float UknightGameplayAbility_MonsterAttack::GetSelectionWeightInternal(const FknightMonsterAttackContext& Context, bool bCheckCooldown) const
{
	if (Selection.Weight <= 0.f || (bCheckCooldown && GetCooldownRemaining(Context.WorldTime) > 0.f))
	{
		return 0.f;
	}
	if (Context.MonsterASC)
	{
		// 분노 마무리 공격은 몬스터가 직접 쓴다. 그동안 AI는 다른 공격을 고르지 않는다.
		if (Context.MonsterASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_EnrageFinale))
		{
			return 0.f;
		}
		if (!Context.MonsterASC->HasAllMatchingGameplayTags(Selection.RequiredOwnerTags)
			|| Context.MonsterASC->HasAnyMatchingGameplayTags(Selection.BlockedOwnerTags))
		{
			return 0.f;
		}
	}
	// 타깃이 없으면 사거리와 각도를 판단할 수 없으므로 타깃이 필요 없는 기술(포효, 버프)만 통과시킨다.
	if (!Context.bHasTarget)
	{
		return Selection.MinRange <= 0.f && Selection.MaxAngle >= 180.f ? Selection.Weight : 0.f;
	}
	const float AbsAngle = FMath::Abs(Context.AngleToTarget);
	const bool bCheckAngle = !Context.bIgnoreAngle || Selection.TargetSide != EknightTargetSide::Any;
	// AngleToTarget은 오른쪽이 +다.
	const bool bWrongSide = (Selection.TargetSide == EknightTargetSide::Left && Context.AngleToTarget > 0.f)
		|| (Selection.TargetSide == EknightTargetSide::Right && Context.AngleToTarget < 0.f);
	if (Context.Distance < Selection.MinRange || Context.Distance > Selection.MaxRange
		|| (bCheckAngle && (AbsAngle > Selection.MaxAngle || AbsAngle < Selection.MinAngle || bWrongSide)))
	{
		return 0.f;
	}
	if ((Selection.AirborneRule == EknightTargetAirborneRule::OnlyAirborne && !Context.bTargetAirborne)
		|| (Selection.AirborneRule == EknightTargetAirborneRule::OnlyGrounded && Context.bTargetAirborne))
	{
		return 0.f;
	}
	if (Selection.MinTargetBehindTime > 0.f && Context.TargetBehindTime < Selection.MinTargetBehindTime)
	{
		return 0.f;
	}
	if (Selection.MinTimeSinceHit > 0.f && Context.TimeSinceHit < Selection.MinTimeSinceHit)
	{
		return 0.f;
	}
	return Selection.Weight;
}

bool UknightGameplayAbility_MonsterAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		&& ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted))
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

float UknightGameplayAbility_MonsterAttack::GetMonsterTimeStep(float Interval) const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Interval * FMath::Max(Avatar->CustomTimeDilation, 0.f) : Interval;
}

float UknightGameplayAbility_MonsterAttack::GetCooldownRemaining(double WorldTime) const
{
	return FMath::Max(0.f, static_cast<float>(LastActivationTime + Selection.Cooldown - WorldTime));
}

void UknightGameplayAbility_MonsterAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (const UWorld* const World = GetWorld())
	{
		LastActivationTime = World->GetTimeSeconds();
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if ((bStartTurnToTarget || !FMath::IsNearlyZero(StartTurnYaw)) && StartTurnMaxTime > 0.f)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(StartTurnMaxTime / knightMonsterAttack::StartTurnInterval));
		UAbilityTask_Repeat* const TurnTask = UAbilityTask_Repeat::RepeatAction(this, knightMonsterAttack::StartTurnInterval, Steps);
		TurnTask->OnPerformAction.AddDynamic(this, &ThisClass::HandleStartTurnTick);
		StartTurnTask = TurnTask;
		TurnTask->ReadyForActivation();
	}
}

void UknightGameplayAbility_MonsterAttack::HandleStartTurnTick(int32 ActionNumber)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	bool bDone = !Movement || !Target;
	if (!bDone)
	{
		// 타깃이 움직여도 따라가도록 매번 방향을 다시 잡는다.
		const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		const FVector Facing = Monster->GetActorLocation() + ToTarget.RotateAngleAxis(StartTurnYaw, FVector::UpVector) * 100.f;
		bDone = Movement->TurnTowards(Facing, GetMonsterTimeStep(knightMonsterAttack::StartTurnInterval), StartTurnRate, knightMonsterAttack::StartTurnTolerance);
	}
	if (bDone && StartTurnTask.IsValid())
	{
		StartTurnTask->EndTask();
	}
}
