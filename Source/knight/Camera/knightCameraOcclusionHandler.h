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

	/** (2026-10-23) 벽에 막혀 카메라가 당겨진 만큼 넓힐 시야각(도). 카메라 컴포넌트가 모드의 시야각에 더한다. */
	float GetCloseUpExtraFOV() const { return CloseUpExtraFOV * (1.f - CurrentFraction); }

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
	 * (2026-10-23) 게임에서 보이지 않는 충돌(맵 경계 볼륨·투명벽)은 무시한다. 맵 끝에서 보이지도 않는 벽에 카메라가
	 * 걸려 캐릭터 바로 뒤로 당겨지던 문제. 지형·건물처럼 보이는 것은 그대로 막는다.
	 */
	UPROPERTY(EditAnywhere, Category="Occlusion")
	bool bIgnoreInvisibleBlockers = true;

	/**
	 * (2026-10-23) 벽에 막혀 카메라가 당겨지면, 당겨진 비율만큼(다 당겨지면 이 값 전부) 시야각을 넓힌다.
	 * 벽 앞에서 캐릭터가 화면을 꽉 채우던 것을 줄인다. 0이면 끈다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditAnywhere, Category="Occlusion|Close Up", meta=(ClampMin=0, ClampMax=40, Units="deg"))
	float CloseUpExtraFOV = 12.f;

	/** (2026-10-23) 같은 비율로 카메라를 이만큼까지 올린다(위가 막히면 덜). 캐릭터가 화면 아래쪽으로 내려가 앞이 보인다. 0이면 끈다. 바뀔 수 있는 값. */
	UPROPERTY(EditAnywhere, Category="Occlusion|Close Up", meta=(ClampMin=0, Units="cm"))
	float CloseUpRaise = 40.f;

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
