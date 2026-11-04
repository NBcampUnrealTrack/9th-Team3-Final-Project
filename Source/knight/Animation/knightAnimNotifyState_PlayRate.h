#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_PlayRate.generated.h"

class UAnimMontage;

/**
 * 몽타주의 이 구간만 PlayRate배로 재생한다. 원본 애니메이션을 고치지 않고 동작의 완급(예: 천천히 들어 올렸다 빠르게 내려찍기)을 만든다.
 *
 * 몽타주 재생 속도를 바꾸는 방식이라 몽타주 시간축은 그대로다. 같은 몽타주의 다른 노티파이(근접 판정 등)는 위치를 옮길 필요가 없고,
 * 실제 시간으로만 빨라진다. 실제 초로 타이밍을 재는 값(범위 공격의 WindupTime 등)은 줄어든 시간만큼 직접 맞춰야 한다.
 * 몽타주에 넣을 때만 동작한다(시퀀스에 넣으면 무시). 구간이 끝나거나 몽타주가 끊기면 원래 속도로 돌아간다.
 */
UCLASS(meta=(DisplayName="Knight Play Rate"))
class KNIGHT_API UknightAnimNotifyState_PlayRate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * 몽타주 위치 From에서 To까지 재생하는 데 걸리는 실제 초. 이 몽타주의 Knight Play Rate 구간, RateScale, 재생할 때 준 배율(PlayRate)을 반영한다.
	 * 판정이 몽타주 위치로 정해진 공격(근접 판정 노티파이, 돌진 Start 섹션)의 예고 시간을 잴 때 쓴다.
	 */
	static float GetPlaySeconds(const UAnimMontage& Montage, float From, float To, float PlayRate = 1.f);

protected:
	/** 구간 안의 재생 배율. 2 = 두 배 빠르게. 몽타주에 이미 걸린 재생 속도에 곱해진다. */
	UPROPERTY(EditAnywhere, Category="Play Rate", meta=(ClampMin=0.1, ClampMax=10))
	float PlayRate = 2.f;

private:
	void ScalePlayRate(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float Scale) const;
};
