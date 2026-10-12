#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Combat/knightCombatLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "knight.h"
#include "knightPerfCounters.h"

namespace knightMonsterAreaAttack
{
	/** 판정이 유지되는 동안 오버랩을 다시 하는 간격. */
	constexpr float ActiveCheckInterval = 1.f / 20.f;
}

namespace knightMonsterAttackCVars
{
	static int32 DrawAttacks = 0;
	static FAutoConsoleVariableRef CVarDrawAttacks(
		TEXT("knight.Monster.DrawAttacks"), DrawAttacks,
		TEXT("Draw monster area attacks: telegraph (orange) and impact (red). 0: off, 1: on"), ECVF_Cheat);
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
	if (knightMonsterAttackCVars::DrawAttacks != 0)
	{
		DrawDebugSphere(GetWorld(), Center, AreaRadius, 16, FColor::Orange, false, WindupTime);
	}
#endif

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* const WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, WindupTime);
	WindupTask->OnFinish.AddDynamic(this, &ThisClass::HandleWindupFinished);
	WindupTask->ReadyForActivation();
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
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightMonsterAreaAttack), false, Avatar);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(AreaRadius), QueryParams);

	int32 NewHits = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		UPrimitiveComponent* const Component = Overlap.GetComponent();
		if (!Target || !Component || HitActors.Contains(Target))
		{
			continue;
		}
		if (!Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
			|| !ICombatInterface::Execute_CanReceiveHitBP(Target, Avatar)
			|| FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile
			|| !IsInsideCone(*Avatar, Target->GetActorLocation()))
		{
			continue;
		}
		HitActors.Add(Target);
		++NewHits;

		// 피해 컨텍스트, 큐, 부위 조회는 히트 결과를 필요로 하므로 가장 가까운 표면 지점으로 하나 만든다.
		FVector ImpactPoint = Component->GetComponentLocation();
		FVector ClosestPoint;
		if (Component->GetClosestPointOnCollision(Center, ClosestPoint) >= 0.f)
		{
			ImpactPoint = ClosestPoint;
		}
		FHitResult Hit(Target, Component, ImpactPoint, (Center - ImpactPoint).GetSafeNormal());
		Hit.bBlockingHit = true;
		Hit.TraceStart = Center;
		Hit.TraceEnd = ImpactPoint;
		UknightCombatLibrary::ApplyMeleeHit(Avatar, Hit, HitParams);
	}

#if ENABLE_DRAW_DEBUG
	if (knightMonsterAttackCVars::DrawAttacks != 0)
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
