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
	float ArmPerTargetRadius = 0.22f;

	/** Extra arm length per cm between the Knight and the target, so close giants still fit. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float ArmPerDistance = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0, Units="cm"))
	float MaxArmLength = 760.f;

	/** Extra pivot height per cm of target radius. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float PivotHeightPerTargetRadius = 0.12f;

	/** Interpolation speed of the framing as the target moves or changes, independent of the mode blend. */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0))
	float FramingInterpSpeed = 4.f;

	/**
	 * 올려다볼 때(피치 40도 이상) 몬스터 크기·거리로 늘린 거리를 이 비율까지만 줄인다(몬스터 줌인을 줄이려고 넣음).
	 * 작을수록 큰 몬스터를 올려다볼 때 카메라가 플레이어 쪽으로 붙어 몬스터가 크게(줌인된 듯) 보인다. 전에는 0.45로 고정. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Lock On", meta=(ClampMin=0, ClampMax=1))
	float LookUpArmScale = 0.8f;

private:
	float CurrentExtraArm = 0.f;
	float CurrentExtraHeight = 0.f;
};
