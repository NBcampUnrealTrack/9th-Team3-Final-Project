#include "Camera/knightCameraMode_ThirdPerson.h"

#include "Curves/CurveFloat.h"

float UknightCameraMode_ThirdPerson::GetArmLength(const FRotator& ViewRotation) const
{
	const float PitchScale = ArmLengthByPitch ? ArmLengthByPitch->GetFloatValue(FRotator::NormalizeAxis(ViewRotation.Pitch)) : 1.f;
	return ArmLength * PitchScale;
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
	else
	{
		LaggedPivot = FMath::VInterpTo(LaggedPivot, Pivot, DeltaTime, PivotLagSpeed);
	}

	const FRotationMatrix ViewAxes(ViewRotation);
	View.Location = LaggedPivot
		- ViewAxes.GetUnitAxis(EAxis::X) * GetArmLength(ViewRotation)
		+ ViewAxes.GetUnitAxis(EAxis::Y) * CameraOffset.Y
		+ ViewAxes.GetUnitAxis(EAxis::Z) * CameraOffset.Z
		+ ViewAxes.GetUnitAxis(EAxis::X) * CameraOffset.X;
	View.PivotLocation = LaggedPivot;
	View.Rotation = ViewRotation;
	View.FieldOfView = FieldOfView;
}
