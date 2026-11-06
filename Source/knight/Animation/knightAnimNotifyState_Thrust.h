// 부스터 노티파이 스테이트("Knight Thrust")
//
// 몽타주(애니)의 이 구간 동안 나이트의 부스터를 뿜는다. 어빌리티가 공격 시작 순간에 뿜는 대신,
// 애니의 동작(예: 내딛어 돌진하는 순간)에 맞춰 켜고 끄고 싶을 때 쓴다.
//
// 사용법:
//   1. 몽타주 노티파이 트랙에 Knight Thrust를 놓고 구간을 뿜을 시간에 맞춘다.
//   2. Thrust 칸에 노즐 이름(BP_Knight → Thrusters의 Nozzles)·이펙트·방향·세기를 넣는다.
//      Duration은 0으로 두면 구간이 끝날 때 끈다(0보다 크면 그 시간 뒤 스스로 끈다).
//   3. 몽타주가 중간에 끊겨도(캔슬·피격) 구간 끝 처리가 불려 부스터가 꺼진다.
// 부스터 컴포넌트가 없는 메시(애니 편집기 미리보기 등)에서는 아무것도 하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Character/knightThrusterComponent.h"
#include "knightAnimNotifyState_Thrust.generated.h"

UCLASS(meta=(DisplayName="Knight Thrust"))
class KNIGHT_API UknightAnimNotifyState_Thrust : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 구간 시작: 주인 액터의 부스터 컴포넌트로 Thrust를 뿜는다. 이동 방향은 지금 속도(없으면 몸 앞). */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	/** 구간 끝(끊긴 경우 포함): 이 구간에서 켠 분사를 끈다. 이미 나온 불꽃은 끝까지 보인다. */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 뿜을 노즐·이펙트·방향·세기. 노즐이나 이펙트를 비우면 뿜지 않는다. 바뀔 수 있는 값. */
	UPROPERTY(EditAnywhere, Category="Thrust")
	FknightThrustSettings Thrust;

private:
	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로, 메시마다 켠 분사 번호를 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, int32> ActiveThrustIds;
};
