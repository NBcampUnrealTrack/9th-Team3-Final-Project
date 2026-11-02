#pragma once

#include "CoreMinimal.h"
#include "Combat/knightHitStopComponent.h"
#include "knightMonsterHitStopComponent.generated.h"

/**
 * 몬스터용 히트스톱. 맞아서 멈출 때는 때린 쪽보다 조금 더 길게 멈추고, 멈춘 동안 메시를 좌우로 짧게 떨어
 * "맞았다"는 느낌을 준다. 때린 쪽으로 멈출 때는 기본 히트스톱과 같다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterHitStopComponent : public UknightHitStopComponent
{
	GENERATED_BODY()

public:
	virtual void ApplyHitStop(float Duration) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 맞아서 멈출 때 히트스톱 시간 배율. 때린 쪽(플레이어)은 원래 시간만큼 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Target", meta=(ClampMin=1))
	float TargetDurationScale = 1.3f;

	/** 떨림 폭. 캡슐 반지름에 대한 비율이라 덩치가 클수록 크게 떤다. 0이면 떨지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Target", meta=(ClampMin=0))
	float ShakeAmplitudeRatio = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Target", meta=(ClampMin=0, Units="cm"))
	float ShakeMaxAmplitude = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Target", meta=(ClampMin=1, Units="Hz"))
	float ShakeFrequency = 30.f;

private:
	/** 이번 히트스톱이 이 몬스터가 맞아서 생긴 것인지. 피해 실행이 같은 프레임에 맞은 시각을 남긴다. */
	bool IsHitThisFrame() const;

	void StartShake(float Duration);
	void UpdateShake();
	void StopShake();

	FTimerHandle ShakeTimer;
	double ShakeStartTime = 0.0;
	double ShakeEndTime = 0.0;
	float ShakeAmplitude = 0.f;
	/** 지금 메시에 더해 둔 오프셋. 다른 코드가 메시 위치를 바꿔도 이만큼만 되돌린다. */
	FVector AppliedOffset = FVector::ZeroVector;
};
