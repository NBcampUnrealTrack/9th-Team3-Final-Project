#pragma once

#include "CoreMinimal.h"
#include "Camera/knightCameraMode.h"
#include "knightCameraMode_ThirdPerson.generated.h"

class UCurveFloat;

/**
 * Orbit camera behind the pivot. The default mode, and the base for Boost / Flight / Parry variants,
 * which are Blueprint subclasses with different arm length, offsets and FOV.
 */
UCLASS(Blueprintable)
class KNIGHT_API UknightCameraMode_ThirdPerson : public UknightCameraMode
{
	GENERATED_BODY()

protected:
	virtual void UpdateView(float DeltaTime) override;

	/** Distance behind the pivot at level pitch. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person", meta=(ClampMin=0, Units="cm"))
	float ArmLength = 450.f;

	/** Optional arm length multiplier by view pitch (degrees). Pulls the camera in when looking up. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person")
	TObjectPtr<const UCurveFloat> ArmLengthByPitch;

	/** Camera-space offset applied after the arm (Y: over-the-shoulder, Z: height). */
	UPROPERTY(EditDefaultsOnly, Category="Third Person")
	FVector CameraOffset = FVector(0.f, 60.f, 20.f);

	/** Pivot follow speed. Zero follows instantly; lower values add a heavy, lagging follow. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person", meta=(ClampMin=0))
	float PivotLagSpeed = 12.f;

	/** Arm length for the current frame, for subclasses that extend it. */
	virtual float GetArmLength(const FRotator& ViewRotation) const;

private:
	FVector LaggedPivot = FVector::ZeroVector;
	bool bHasLaggedPivot = false;

public:
	virtual void OnActivation() override { bHasLaggedPivot = false; }
};
