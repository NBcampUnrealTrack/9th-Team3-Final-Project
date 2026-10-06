#include "Monster/knightMonsterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "System/knightGameplayTags.h"

UknightMonsterMovementComponent::UknightMonsterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxWalkSpeed = 700.f;
	MaxAcceleration = 800.f;
	BrakingDecelerationWalking = 800.f;
	RotationRate = FRotator(0.f, 90.f, 0.f);
	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
	GravityScale = 1.5f;

	// 경로 추종 시 곧바로 최고 속도를 내지 않고 무거운 몸체처럼 가속한다.
	bRequestedMoveUseAcceleration = true;
	bUseRVOAvoidance = false;

	LockTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	LockTags.AddTag(knightGameplayTags::State_Dead);
}

bool UknightMonsterMovementComponent::IsMovementLocked() const
{
	if (!CachedASC.IsValid())
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	}
	const UAbilitySystemComponent* const ASC = CachedASC.Get();
	return ASC && ASC->HasAnyMatchingGameplayTags(LockTags);
}

float UknightMonsterMovementComponent::GetMaxSpeed() const
{
	return IsMovementLocked() ? 0.f : Super::GetMaxSpeed();
}

void UknightMonsterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// 이렇게 하지 않으면 몬스터가 휘청이는 동안 Orient-to-movement가 마지막 경로 쪽으로 계속 회전시킨다.
	if (!IsMovementLocked())
	{
		Super::PhysicsRotation(DeltaTime);
	}
}

bool UknightMonsterMovementComponent::TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees)
{
	AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return true;
	}
	if (IsMovementLocked())
	{
		return false;
	}
	const FVector ToTarget = (WorldLocation - Owner->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}
	const float CurrentYaw = Owner->GetActorRotation().Yaw;
	const float DesiredYaw = ToTarget.Rotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(DesiredYaw - CurrentYaw);
	if (FMath::Abs(Delta) <= ToleranceDegrees)
	{
		return true;
	}
	const float Step = FMath::Clamp(Delta, -TurnRate * DeltaTime, TurnRate * DeltaTime);
	Owner->SetActorRotation(FRotator(0.f, CurrentYaw + Step, 0.f));
	return FMath::Abs(Delta - Step) <= ToleranceDegrees;
}
