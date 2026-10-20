#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "knightCameraOcclusionHandler.generated.h"

/**
 * Keeps the camera out of walls: sweeps from the pivot to the desired camera location and pulls the
 * camera in to the first blocking hit, easing back out once clear.
 *
 * Lock-on targets (ITargetableInterface actors, i.e. monsters) are ignored. Pulling the camera in front
 * of a giant monster's leg every time it walks past would be worse than seeing through it;
 * UknightCameraFadeHandler fades or hides the monster instead.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class KNIGHT_API UknightCameraOcclusionHandler : public UObject
{
	GENERATED_BODY()

public:
	/** Returns the camera location after occlusion. */
	FVector ResolveCameraLocation(const AActor* ViewOwner, const FVector& Pivot, const FVector& DesiredLocation, float DeltaTime);

protected:
	UPROPERTY(EditAnywhere, Category="Occlusion", meta=(ClampMin=1, Units="cm"))
	float ProbeRadius = 20.f;

	UPROPERTY(EditAnywhere, Category="Occlusion")
	TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

	/** Speed of easing back out after the obstruction clears. Pulling in is always instant. */
	UPROPERTY(EditAnywhere, Category="Occlusion", meta=(ClampMin=0))
	float RecoverySpeed = 6.f;

	UPROPERTY(EditAnywhere, Category="Occlusion")
	bool bIgnoreTargetableActors = true;

	/**
	 * (2026-10-20) 카메라가 물(워터 바디) 속으로 들어가지 않게 수면 + MinHeightAboveWater 아래로는 내리지 않는다.
	 * 수면은 충돌이 아니라 워터 바디 질의(UknightWaterRippleSubsystem::FindWaterSurface)로 찾는다.
	 * 예: L_Raid02 물가에서 올려다보거나 깊은 물에 들어갈 때.
	 */
	UPROPERTY(EditAnywhere, Category="Occlusion|Water")
	bool bKeepAboveWater = true;

	UPROPERTY(EditAnywhere, Category="Occlusion|Water", meta=(ClampMin=0, Units="cm", EditCondition="bKeepAboveWater"))
	float MinHeightAboveWater = 25.f;

	/**
	 * (2026-10-20) 벽 판정 스윕에서 워터 바디는 무시한다(이 맵에서 물 충돌이 WorldStatic이라 카메라가 수면에 걸려
	 * 캐릭터 쪽으로 확 당겨질 수 있다). 물은 위의 수면 높이 제한으로 대신한다.
	 */
	UPROPERTY(EditAnywhere, Category="Occlusion|Water")
	bool bIgnoreWaterInSweep = true;

private:
	/** 0..1 fraction of the pivot-to-camera distance currently allowed. */
	float CurrentFraction = 1.f;
};
