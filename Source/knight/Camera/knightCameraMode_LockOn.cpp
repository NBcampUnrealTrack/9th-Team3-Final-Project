#include "Camera/knightCameraMode_LockOn.h"

#include "Combat/knightTargetLockComponent.h"
#include "GameFramework/Actor.h"

UknightCameraMode_LockOn::UknightCameraMode_LockOn()
{
	ArmLength = 380.f;
	FieldOfView = 82.f;
	PivotLagSpeed = 8.f;
}

float UknightCameraMode_LockOn::GetArmLength(const FRotator& ViewRotation) const
{
	const float NormalizedPitch = FRotator::NormalizeAxis(ViewRotation.Pitch);
	const float UpScale = NormalizedPitch > 0.f
		? FMath::Lerp(1.f, LookUpArmScale, FMath::Clamp(NormalizedPitch / 40.f, 0.f, 1.f))
		: 1.f;
	return FMath::Min(Super::GetArmLength(ViewRotation) + CurrentExtraArm * UpScale, MaxArmLength);
}

void UknightCameraMode_LockOn::UpdateView(float DeltaTime)
{
	float TargetExtraArm = 0.f;
	float TargetExtraHeight = 0.f;

	const AActor* const Owner = GetTargetActor();
	const UknightTargetLockComponent* const TargetLock = Owner ? Owner->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	FVector LockedLocation;
	if (TargetLock && TargetLock->GetLockedCameraLocation(LockedLocation))
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
