#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Combat/knightCombatLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "knight.h"
#include "knightPerfCounters.h"

namespace knightMonsterAreaAttack
{
	/** 판정이 유지되는 동안 오버랩을 다시 하는 간격. */
	constexpr float ActiveCheckInterval = 1.f / 20.f;
}

UknightGameplayAbility_MonsterAreaAttack::UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 구체는 몬스터 앞쪽 AreaOffset.X + AreaRadius까지 닿으므로 그 안에서 고르도록 한다.
	Selection.MaxRange = 600.f;
	Selection.MaxAngle = 45.f;
}

void UknightGameplayAbility_MonsterAreaAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	HitActors.Reset();

	const FVector Center = GetAreaCenter();
	if (TelegraphCue.IsValid())
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Center;
			CueParams.RawMagnitude = AreaRadius;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			ASC->ExecuteGameplayCue(TelegraphCue, CueParams);
		}
	}
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, AreaRadius, 16, FColor::Orange, false, WindupTime);
	}
#endif

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->ReadyForActivation();
	}

	if (LungeMaxDistance > 0.f && LungeEndTime > LungeStartTime)
	{
		UAbilityTask_WaitDelay* const LungeDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, LungeStartTime);
		LungeDelayTask->OnFinish.AddDynamic(this, &ThisClass::StartLunge);
		LungeDelayTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* const WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, WindupTime);
	WindupTask->OnFinish.AddDynamic(this, &ThisClass::HandleWindupFinished);
	WindupTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::StartLunge()
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const UCharacterMovementComponent* const Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	if (!Target || !Movement)
	{
		return;
	}
	const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
	const float Distance = FMath::Clamp(static_cast<float>(ToTarget.Size2D()) - AreaOffset.X, 0.f, LungeMaxDistance);
	if (Distance < 1.f)
	{
		return;
	}
	// 정면이 아니라 타깃 쪽으로 던진다. 예고 동안 타깃이 옆으로 조금 움직여도 몸이 따라간다.
	const FVector Direction = ToTarget.GetSafeNormal2D();
	const float Duration = LungeEndTime - LungeStartTime;
	UAbilityTask_ApplyRootMotionConstantForce* const LungeTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("AreaLunge"), Direction, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 0.f, false);
	LungeTask->ReadyForActivation();
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s lunges %.0f cm over %.2f s."), *GetNameSafe(Monster), *GetName(), Distance, Duration);
}

void UknightGameplayAbility_MonsterAreaAttack::HandleWindupFinished()
{
	OnImpact();
	if (ActiveTime <= 0.f)
	{
		StartRecovery();
		return;
	}
	const int32 Checks = FMath::Max(1, FMath::RoundToInt(ActiveTime / knightMonsterAreaAttack::ActiveCheckInterval));
	UAbilityTask_Repeat* const ActiveTask = UAbilityTask_Repeat::RepeatAction(this, knightMonsterAreaAttack::ActiveCheckInterval, Checks);
	ActiveTask->OnPerformAction.AddDynamic(this, &ThisClass::HandleActiveTick);
	ActiveTask->OnFinished.AddDynamic(this, &ThisClass::StartRecovery);
	ActiveTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::HandleActiveTick(int32 ActionNumber)
{
	OnImpact();
}

void UknightGameplayAbility_MonsterAreaAttack::StartRecovery(int32 ActionNumber)
{
	UAbilityTask_WaitDelay* const RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryTime);
	RecoveryTask->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
	RecoveryTask->ReadyForActivation();
}

void UknightGameplayAbility_MonsterAreaAttack::HandleRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MonsterAreaAttack::OnImpact()
{
	KNIGHT_PERF_SCOPE("Monster.AreaAttackImpact");

	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	const FVector Center = GetAreaCenter();
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Avatar, Center, FCollisionShape::MakeSphere(AreaRadius), HitParams, HitActors,
		[this, Avatar](const AActor& Target) { return IsInsideCone(*Avatar, Target.GetActorLocation()); },
		[this](AActor& Target, EknightHitOutcome Outcome)
		{
			if (Outcome != EknightHitOutcome::Hit || !HitTargetEffect)
			{
				return;
			}
			UAbilitySystemComponent* const SourceASC = GetAbilitySystemComponentFromActorInfo();
			UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target);
			if (SourceASC && TargetASC)
			{
				const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(HitTargetEffect, GetAbilityLevel());
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		});

#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(World, Center, AreaRadius, 16, NewHits == 0 ? FColor::Red : FColor::Magenta, false,
			ActiveTime > 0.f ? knightMonsterAreaAttack::ActiveCheckInterval : 0.5f, 0, 3.f);
		if (ConeHalfAngle > 0.f)
		{
			const FVector Origin = Avatar->GetActorTransform().TransformPosition(ConeOrigin);
			const float Length = FVector::Dist(Origin, Center) + AreaRadius;
			DrawDebugCone(World, Origin, Avatar->GetActorForwardVector(), Length, FMath::DegreesToRadians(ConeHalfAngle),
				FMath::DegreesToRadians(5.f), 12, FColor::Yellow, false, ActiveTime > 0.f ? knightMonsterAreaAttack::ActiveCheckInterval : 0.5f);
		}
	}
#endif
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s hit %d target(s)."), *GetNameSafe(Avatar), *GetName(), NewHits);
}

bool UknightGameplayAbility_MonsterAreaAttack::IsInsideCone(const AActor& Avatar, const FVector& Point) const
{
	if (ConeHalfAngle <= 0.f)
	{
		return true;
	}
	// 높이는 보지 않는다. 브레스가 아래로 기울어도 같은 좌우 폭으로 판정한다.
	const FVector Origin = Avatar.GetActorTransform().TransformPosition(ConeOrigin);
	const FVector ToPoint = (Point - Origin).GetSafeNormal2D();
	const FVector Forward = Avatar.GetActorForwardVector().GetSafeNormal2D();
	return !ToPoint.IsNearlyZero() && FVector::DotProduct(ToPoint, Forward) >= FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle));
}

FVector UknightGameplayAbility_MonsterAreaAttack::GetAreaCenter() const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->GetActorTransform().TransformPosition(AreaOffset) : FVector::ZeroVector;
}
