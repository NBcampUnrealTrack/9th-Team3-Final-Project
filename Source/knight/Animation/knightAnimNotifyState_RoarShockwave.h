#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_RoarShockwave.generated.h"

/**
 * 포효 구간. 구간 동안 플레이어 화면에 몬스터 위치에서 퍼지는 파동(왜곡, 색수차, 지글거림, 약한 흔들림)을 일정 간격으로 내보낸다.
 *
 * 파동 횟수는 구간 길이로 정해진다(간격과 세기는 UknightCameraModifier_RoarShockwave 기본값). 포효 소리가 실제로 나는 부분에만 깐다.
 * 몽타주가 끊기면 NotifyEnd가 불려서 새 파동은 멈추고, 이미 나간 파동은 마저 퍼진다.
 */
UCLASS(meta=(DisplayName="Knight Roar Shockwave"))
class KNIGHT_API UknightAnimNotifyState_RoarShockwave : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 파동이 시작되는 소켓(보통 머리). 없으면 메시 중심에서 시작한다. */
	UPROPERTY(EditAnywhere, Category="Roar")
	FName Socket = TEXT("head");

	/** 파동 고리를 내보낸다. 끄면 화면 일렁임과 약한 흔들림만 남는다. */
	UPROPERTY(EditAnywhere, Category="Roar")
	bool bPulses = true;

	/** 화면 전체가 천천히 일렁이는 세기(1 = 기본). 0이면 일렁임 없음. */
	UPROPERTY(EditAnywhere, Category="Roar", meta=(ClampMin=0, ClampMax=3))
	float Wiggle = 0.f;
};
