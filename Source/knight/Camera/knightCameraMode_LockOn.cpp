#include "Camera/knightCameraMode_LockOn.h"

#include "Combat/knightTargetLockComponent.h"
#include "GameFramework/Actor.h"

UknightCameraMode_LockOn::UknightCameraMode_LockOn()
{
	ArmLength = 550.f;
	FieldOfView = 85.f;
	PivotLagSpeed = 8.f;
}

float UknightCameraMode_LockOn::GetArmLength(const FRotator& ViewRotation) const
{
	return FMath::Min(Super::GetArmLength(ViewRotation) + CurrentExtraArm, MaxArmLength);
}

void UknightCameraMode_LockOn::UpdateView(float DeltaTime)
{
	float TargetExtraArm = 0.f;
	float TargetExtraHeight = 0.f;

	const AActor* const Owner = GetTargetActor();
	const UknightTargetLockComponent* const TargetLock = Owner ? Owner->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	FVector LockedLocation;
	if (TargetLock && TargetLock->GetLockedTargetLocation(LockedLocation))
	{
		const float Radius = TargetLock->GetLockedTargetRadius();
		const float Distance = FVector::Dist2D(Owner->GetActorLocation(), LockedLocation);
		TargetExtraArm = Radius * ArmPerTargetRadius + Distance * ArmPerDistance;
		TargetExtraHeight = Radius * PivotHeightPerTargetRadius;
	}

	CurrentExtraArm = FMath::FInterpTo(CurrentExtraArm, TargetExtraArm, DeltaTime, FramingInterpSpeed);
	CurrentExtraHeight = FMath::FInterpTo(CurrentExtraHeight, TargetExtraHeight, DeltaTime, FramingInterpSpeed);

	const FVector BasePivotOffset = PivotOffset;
	PivotOffset.Z += CurrentExtraHeight;
	Super::UpdateView(DeltaTime);
	PivotOffset = BasePivotOffset;
}
