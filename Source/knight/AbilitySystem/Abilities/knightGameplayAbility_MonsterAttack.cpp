#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
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
	if (Monster && Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		Context.bHasTarget = true;
		Context.Distance = ToTarget.Size2D();
		Context.AngleToTarget = FRotator::NormalizeAxis(ToTarget.GetSafeNormal2D().Rotation().Yaw - Monster->GetActorRotation().Yaw);
		const UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		Context.bTargetAirborne = TargetASC && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Airborne);
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
	if (Selection.Weight <= 0.f || GetCooldownRemaining(Context.WorldTime) > 0.f)
	{
		return 0.f;
	}
	if (Context.MonsterASC)
	{
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
	if (Context.Distance < Selection.MinRange || Context.Distance > Selection.MaxRange
		|| FMath::Abs(Context.AngleToTarget) > Selection.MaxAngle)
	{
		return 0.f;
	}
	if ((Selection.AirborneRule == EknightTargetAirborneRule::OnlyAirborne && !Context.bTargetAirborne)
		|| (Selection.AirborneRule == EknightTargetAirborneRule::OnlyGrounded && Context.bTargetAirborne))
	{
		return 0.f;
	}
	return Selection.Weight;
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

	if (!FMath::IsNearlyZero(StartTurnYaw) && StartTurnMaxTime > 0.f)
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
		bDone = Movement->TurnTowards(Facing, knightMonsterAttack::StartTurnInterval, StartTurnRate, knightMonsterAttack::StartTurnTolerance);
	}
	if (bDone && StartTurnTask.IsValid())
	{
		StartTurnTask->EndTask();
	}
}
