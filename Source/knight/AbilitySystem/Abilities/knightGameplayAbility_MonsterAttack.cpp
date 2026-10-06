#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "System/knightGameplayTags.h"

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
}
