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

	// Path following accelerates like a heavy body instead of snapping to full speed.
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
	// Orient-to-movement would otherwise keep turning toward the last path while the monster reels.
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
