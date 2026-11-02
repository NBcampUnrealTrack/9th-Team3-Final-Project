#include "Camera/knightCameraMode_ThirdPerson.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "System/knightGameplayTags.h"

UknightCameraMode_ThirdPerson::UknightCameraMode_ThirdPerson()
{
	// (2026-10-29) 구르는 동안은 카메라가 뒤처진 거리를 그대로 둔다(FreezeLagTags 주석 참고).
	FreezeLagTags.AddTag(knightGameplayTags::State_Rolling);
}

bool UknightCameraMode_ThirdPerson::IsLagFrozen() const
{
	if (FreezeLagTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetTargetActor());
	return ASC && ASC->HasAnyMatchingGameplayTags(FreezeLagTags);
}

float UknightCameraMode_ThirdPerson::GetArmLength(const FRotator& ViewRotation) const
{
	const float NormalizedPitch = FRotator::NormalizeAxis(ViewRotation.Pitch);
	if (ArmLengthByPitch)
	{
		return ArmLength * ArmLengthByPitch->GetFloatValue(NormalizedPitch);
	}
	// Pull the camera in when looking up so long arms do not swing down into the floor or shallow water.
	if (NormalizedPitch > 0.f)
	{
		const float UpAlpha = FMath::Clamp(NormalizedPitch / 45.f, 0.f, 1.f);
		return ArmLength * FMath::Lerp(1.f, 0.55f, UpAlpha);
	}
	return ArmLength;
}

void UknightCameraMode_ThirdPerson::UpdateView(float DeltaTime)
{
	FRotator ViewRotation = GetPivotRotation();
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, ViewPitchMin, ViewPitchMax);
	ViewRotation.Roll = 0.f;

	const FVector Pivot = GetPivotLocation();
	if (!bHasLaggedPivot || PivotLagSpeed <= 0.f)
	{
		LaggedPivot = Pivot;
		bHasLaggedPivot = true;
	}
	else if (IsLagFrozen())
	{
		// (2026-10-29) 구르기 등: 지금 뒤처진 거리를 그대로 두고 캐릭터가 움직인 만큼만 같이 옮긴다.
		LaggedPivot += Pivot - LastPivot;
	}
	else
	{
		LaggedPivot = FMath::VInterpTo(LaggedPivot, Pivot, DeltaTime, PivotLagSpeed);
	}
	LastPivot = Pivot;

	const FRotationMatrix ViewAxes(ViewRotation);
	View.Location = LaggedPivot
		- ViewAxes.GetUnitAxis(EAxis::X) * GetArmLength(ViewRotation)
		+ ViewAxes.GetUnitAxis(EAxis::Y) * CameraOffset.Y
		+ ViewAxes.GetUnitAxis(EAxis::Z) * CameraOffset.Z
		+ ViewAxes.GetUnitAxis(EAxis::X) * CameraOffset.X;

	// Prevent the camera from dipping below the character's knee/waterline height when looking up at tall bosses
	// (e.g. L_Raid02 where shallow water sits ~32cm below the character's origin).
	const float BaseOriginZ = LaggedPivot.Z - PivotOffset.Z;
	float MinCameraZ = BaseOriginZ - 8.f;
	if (const AActor* const Target = GetTargetActor())
	{
		MinCameraZ = FMath::Max(MinCameraZ, Target->GetActorLocation().Z - 8.f);
	}
	View.Location.Z = FMath::Max(View.Location.Z, MinCameraZ);

	View.PivotLocation = LaggedPivot;
	View.Rotation = ViewRotation;
	View.FieldOfView = FieldOfView;
}
