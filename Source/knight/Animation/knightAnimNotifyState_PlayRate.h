#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_PlayRate.generated.h"

struct FAnimMontageInstance;

/**
 * 몽타주 재생 속도를 이 구간 동안만 PlayRateScale배로 바꾼다. (2026-10-12, 공격 무게감)
 *
 * 공격 준비 동작(칼을 들어 올리는 구간)을 살짝 느리게 해서 "힘을 모았다가 내려치는" 느낌을 낸다.
 * 판정·콤보·워프 노티파이는 몽타주 시간 기준이라 위치가 어긋나지 않는다. 실제 걸리는 시간만 늘어난다.
 *
 * - 몽타주에 둘 때만 동작한다(시퀀스에 직접 두면 무시).
 * - 시작할 때 현재 재생 속도에 곱하고, 끝날 때 나눠서 되돌린다. 어빌리티가 정한 재생 속도(PlayRate)는 유지된다.
 * - 히트스톱은 CustomTimeDilation을 쓰므로 이 노티파이와 겹쳐도 서로 영향을 주지 않는다.
 */
UCLASS(meta=(DisplayName="Knight Play Rate"))
class KNIGHT_API UknightAnimNotifyState_PlayRate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 이 구간의 재생 속도 배율. 1보다 작으면 느려진다(0.8 = 20% 느리게). */
	UPROPERTY(EditAnywhere, Category="Play Rate", meta=(ClampMin=0.1, ClampMax=3))
	float PlayRateScale = 0.8f;

private:
	FAnimMontageInstance* FindInstance(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const;
};
