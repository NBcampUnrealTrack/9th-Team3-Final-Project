#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Camera/knightCameraImpactSettings.h"
#include "knightCameraModifier_Impact.generated.h"

/**
 * 전투 순간의 카메라 반응(FknightCameraImpactReaction)을 재생한다. 카메라 모드가 만든 최종 시점 위에 덧씌우므로
 * 카메라 컴포넌트나 모드 설정은 바꾸지 않는다.
 *
 * 반응마다 올라감 → 머묾 → 돌아옴 곡선을 따로 돌리고, 여러 개가 겹치면 시야각·위치 변화는 더하고 색 갈라짐은 가장 센 것을 쓴다.
 * 시간은 실제 시간이라 히트스톱·전역 슬로·정지와 상관없이 정해진 길이만큼 재생된다.
 */
UCLASS()
class KNIGHT_API UknightCameraModifier_Impact : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** Scale은 0~1(세기와 화면 흔들림 설정을 반영한 배율). Direction은 Push 방향(월드, 길이 1). */
	void Play(const FknightCameraImpactReaction& Reaction, float Scale, const FVector& Direction);

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

	/**
	 * 이번 프레임에 좁힐 시야각(도). 시야각은 사용자 FOV 보정(최대 110도로 자름)보다 뒤에 빼야 넓은 시야 설정에서도 보이므로,
	 * 모디파이어가 직접 바꾸지 않고 카메라 매니저가 UpdateViewTarget 끝에서 뺀다.
	 */
	float GetFOVDelta() const { return CurrentFOVDelta; }

private:
	struct FActiveReaction
	{
		FknightCameraImpactReaction Reaction;
		float Scale = 1.f;
		float Age = 0.f;
		FVector Direction = FVector::ZeroVector;
	};

	static float GetEnvelope(const FknightCameraImpactReaction& Reaction, float Age);

	TArray<FActiveReaction> Active;
	float CurrentFringe = 0.f;
	float CurrentFOVDelta = 0.f;
};
