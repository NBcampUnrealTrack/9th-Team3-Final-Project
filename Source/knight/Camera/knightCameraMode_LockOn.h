#pragma once

#include "CoreMinimal.h"
#include "Camera/knightCameraMode_ThirdPerson.h"
#include "knightCameraMode_LockOn.generated.h"

/**
 * Lock-on framing for giant targets. The owner's UknightTargetLockComponent turns the control rotation
 * toward the locked point; this mode pulls the camera back and up with the target's radius and
 * distance, so both the Knight and a monster many times its size stay on screen.
 */
UCLASS(Blueprintable)
class KNIGHT_API UknightCameraMode_LockOn : public UknightCameraMode_ThirdPerson
{
	GENERATED_BODY()

public:
	UknightCameraMode_LockOn();

protected:
	virtual void UpdateView(float DeltaTime) override;
	virtual float GetArmLength(const FRotator& ViewRotation) const override;

	/** Extra arm length per cm of target radius. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float ArmPerTargetRadius = 0.5f;

	/** Extra arm length per cm between the Knight and the target, so close giants still fit. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float ArmPerDistance = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0, Units="cm"))
	float MaxArmLength = 1400.f;

	/** Extra pivot height per cm of target radius. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float PivotHeightPerTargetRadius = 0.15f;

	/** Interpolation speed of the framing as the target moves or changes, independent of the mode blend. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float FramingInterpSpeed = 4.f;

private:
	float CurrentExtraArm = 0.f;
	float CurrentExtraHeight = 0.f;
};
