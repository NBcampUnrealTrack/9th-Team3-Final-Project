#pragma once

#include "CoreMinimal.h"
#include "Camera/knightCameraMode.h"
#include "GameplayTagContainer.h"
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

public:
	UknightCameraMode_ThirdPerson();

protected:
	virtual void UpdateView(float DeltaTime) override;

	/** Distance behind the pivot at level pitch. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person", meta=(ClampMin=0, Units="cm"))
	float ArmLength = 340.f;

	/** Optional arm length multiplier by view pitch (degrees). Pulls the camera in when looking up. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person")
	TObjectPtr<const UCurveFloat> ArmLengthByPitch;

	/** Camera-space offset applied after the arm (Y: over-the-shoulder, Z: height). */
	UPROPERTY(EditDefaultsOnly, Category="Third Person")
	FVector CameraOffset = FVector(0.f, 60.f, 20.f);

	/** Pivot follow speed. Zero follows instantly; lower values add a heavy, lagging follow. */
	UPROPERTY(EditDefaultsOnly, Category="Third Person", meta=(ClampMin=0))
	float PivotLagSpeed = 12.f;

	/**
	 * (2026-10-29) 주인(캐릭터)에게 이 태그 중 하나라도 있는 동안은 뒤따라오는 거리(PivotLagSpeed로 생긴 지연)를 그대로 두고
	 * 캐릭터와 같이 움직인다. 구르기처럼 갑자기 빨라졌다 멈추는 동작에서 카메라가 멀어졌다 돌아오는(축소처럼 보이는) 것을 막는다.
	 * 기본값 State.Rolling(구르기). 태그가 빠지면 다시 PivotLagSpeed로 부드럽게 따라간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Third Person", meta=(Categories="State"))
	FGameplayTagContainer FreezeLagTags;

	/** Arm length for the current frame, for subclasses that extend it. */
	virtual float GetArmLength(const FRotator& ViewRotation) const;

private:
	/** 주인에게 FreezeLagTags 중 하나가 있는지. */
	bool IsLagFrozen() const;

	FVector LaggedPivot = FVector::ZeroVector;
	/** 지난 프레임의 실제 피벗. 지연을 고정하는 동안 캐릭터가 움직인 만큼 LaggedPivot을 옮기는 데 쓴다. */
	FVector LastPivot = FVector::ZeroVector;
	bool bHasLaggedPivot = false;

public:
	virtual void OnActivation() override { bHasLaggedPivot = false; }
};
