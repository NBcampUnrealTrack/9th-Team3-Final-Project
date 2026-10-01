#include "Monster/knightMonsterMovementComponent.h"

#include "GameFramework/Actor.h"

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
}

bool UknightMonsterMovementComponent::TurnTowards(const FVector& WorldLocation, float DeltaTime, float TurnRate, float ToleranceDegrees)
{
	AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return true;
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
